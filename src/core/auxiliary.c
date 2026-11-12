#include "core.h"
#include "../parsers/stringlib.h"
#include "../parsers/parsers.h"
#include "../macros.h"
#if defined(_WIN32)
  #include <stdlib.h>
  #ifndef PATH_MAX
    #define PATH_MAX _MAX_PATH
  #endif
#else
  #include <limits.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * This function will temporarely modify its input, but will leave it as original
 * It returns if the type is actually the type of a variable, and not a structure definition or typedef
 */
static int check_variable_type_name_against_keywords(char * typebuf) {
  char * first_word = typebuf;
  while (*first_word == ' ' && *first_word != 0) first_word++;

  char * first_space = first_word;
  while (*first_space != ' ' && *first_space != 0) first_space++;
  if (*first_space == 0) return 0;

  *first_space = 0;
  if (streq("extern", first_word) || streq("static", first_word) || streq("volatile", first_word)) {
    // Skip this word
    *first_space = ' ';
    while (*first_space == ' ' && *first_space != 0) first_space++;
    if (*first_space == 0) return 0;
    first_word = first_space;

    while (*first_space != ' ' && *first_space != 0) first_space++;
    if (*first_space == 0) return 0;
    *first_space = 0;
  }

  int is_not_variable = streq("struct", first_word) || streq("typedef", first_word);

  *first_space = ' ';
  return is_not_variable;
}
int normalize_type_string(const char * type, char * typebuf, int with_replace_newline) {
  int skip_first_word = starts_with(type, "extern") || starts_with(type, "static") || starts_with(type, "volatile");
  if (skip_first_word) {
    while (*type != ' ' && *type != 0) type++;
  }
  while (*type == ' ' && *type != 0) type++;

  while (*type != 0) {
    if (*type == ' ') {
      if (*(type - 1) != ' ') {
        *typebuf = *type;
        typebuf++;
        type++;
      } else type++;
    } else {
      if (*type == '\n' && with_replace_newline) break;
      *typebuf = *type;
      typebuf++;
      type++;
    }
  }
  *typebuf = 0;

  return 0;
}
/*
 * This function replaces in `path` the source `replace` by the new path `by`.
 * It writes in buffer as the function goes by
 */
static void rebase_path(const char * path, const char * replace, const char * by, char * buffer, size_t max) {
  unsigned i = 0;
  unsigned bufindex = 0;
  while (path[i] == replace[i] && i < max) i++;

  while (by[bufindex] != 0 && bufindex < max) {
    buffer[bufindex] = by[bufindex];
    bufindex++;
  }
  while (path[i] != 0 && i < max && bufindex < max) {
    buffer[bufindex] = path[i];
    bufindex++;
    i++;
  }
  buffer[bufindex] = 0;
}
static void swap_word_onwards_inplace(char *value, char *a, char *b, size_t max) {
  if (a > b) {
    char *c = a;
    a = b;
    b = c;
  }
  if (a < value) return;
  while (value != a) value++;

  size_t first_len = 0;
  while (a[first_len] != ' ' && a[first_len] != 0) first_len++;
  size_t second_len = strlen(b);

  char *tmp = malloc(max);
  if (tmp == NULL) {
    perror("malloc swap_word_onwards_inplace");
    return;
  }

  size_t i = 0;
  while (i < first_len) {
    tmp[i] = a[i];
    i++;
  }
  tmp[first_len] = 0;
  i = 0;
  while (i <= second_len) {
    a[i] = b[i];
    i++;
  }
  a[second_len++] = ' ';
  i = 0;
  while (i < first_len) {
    a[second_len + i] = tmp[i];
    i++;
  }
  a[second_len + first_len] = 0;
  free(tmp);
}
static char * normalize_variable_input(const char * input, size_t size, int * valid) {
  char * output;
  *valid = 1;
  if ((output = malloc(size)) == NULL) {
    perror("malloc normalize_variable_type");
    return NULL;
  }

  unsigned int output_index = 0;
  while (*input != 0) {
    if (*input == '=' || *input == ';') break;
    if (*input == '*') {
      if (output_index == 0) { // We have some strange type here : * name;
        *valid = 0;
        free(output);
        return NULL;
      }
      if (output[output_index - 1] == '*') output[output_index++] = '*';
      else {
        if (output[output_index - 1] != ' ') output[output_index++] = ' ';
        if (output[output_index - 1] == ' ' && output[output_index - 2] == '*') output[output_index - 1] = '*';
        else output[output_index++] = '*';
      }
    } else if (*input == ' ') {
      if (output_index > 0 && output[output_index - 1] != ' ') output[output_index++] = ' ';
    } else if (*input == '[') {
      if (output[output_index - 1] != ' ') output[output_index++] = ' ';
      output[output_index++] = '[';
    } else {
      if (output_index > 0 && output[output_index - 1] == '*') output[output_index++] = ' ';
      output[output_index++] = *input;
    }
    input++;
  }
  if (output_index > 0 && output[output_index - 1] == ' ') output[output_index - 1] = 0;
  output[output_index] = 0;

  return output;
}
int try_extract_variable(char * buffer, const char * path, callback_args params, struct status_s * status) {
  while (*buffer == ' ') buffer++;
  if (*buffer == '#') return 1;

  char * var_name = buffer;
  int valid = 0;
  char * input;

  if (check_variable_type_name_against_keywords(buffer) == 1) return 1; // We actually have a variable then

  if ((input = normalize_variable_input(buffer, ENTITIES_NAME_LENGTH, &valid)) == NULL) {
    if (!valid) fprintf(stderr, "    /!\\ Invalid variable definition at %s:%lld\n", path, status->line_count);
    return 0;
  }
  // We now have a normalized input, with no ';', no newline, and one space between each word
  unsigned i = 0;
  char * bracket_ptr = NULL;
  int needs_bracket_operation = 0;
  while (input[i] != 0) {
    if (input[i] == ' ') {
      if (*(input + i + 1) == '[') {// This is not the actual name
        needs_bracket_operation = 1;
        bracket_ptr = input + i + 1;
        break;
      }
      var_name = input + i + 1;
    }
    if (input[i] == ')' || input[i] == '}' || input[i] == '(' || input[i] == '{') { // This is not a variable
      free(input);
      return 0;
    }
    i++;
  }
  if (var_name == buffer) {
    fprintf(stderr, "    /!\\ Incomplete variable definition at %s:%lld (no type)\n", path, status->line_count);
    free(input);
    return 0;
  }

  if (needs_bracket_operation) { // Then we have type variableName [???];
    swap_word_onwards_inplace(input, var_name, bracket_ptr, ENTITIES_NAME_LENGTH);

    // Retrack the pointers to correct position. Variable name is now the last word
    char * tmp = var_name;
    while (*tmp != 0) {
      if (*tmp == ' ') var_name = tmp + 1;
      tmp++;
    }
  }
  *(var_name - 1) = 0;

  if (status->is_header) {
    if (params->source_code->definitions.variables.count == 0) {
      if ((params->source_code->definitions.variables.array = malloc(sizeof(struct variable))) == NULL) {
        perror("malloc");
        free(input);
        params->results.fails++;
        return 1;
      }
    } else if ((params->source_code->definitions.variables.array = realloc(params->source_code->definitions.variables.array, sizeof(struct variable) * (params->source_code->definitions.variables.count + 1))) == NULL) {
      perror("realloc");
      params->results.fails++;
      free(input);
      return 1;
    }

    params->source_code->definitions.variables.array[params->source_code->definitions.variables.count] = (struct variable){
      .type = &(entity_type_t){
        .ptr = NULL,
        .type = NoneType,
      }
    };

    struct variable * var = params->source_code->definitions.variables.array + params->source_code->definitions.variables.count;
    if ((var->ref = malloc(sizeof(struct reference))) == NULL) {
      perror("malloc var->ref");
      free(var);
      free(input);
    }
    if ((var->type = malloc(sizeof(entity_type_t))) == NULL) {
      perror("malloc var->type");
      free(var->ref);
      free(var);
      free(input);
      return 0;
    }
    var->ref->line = status->line_count;
    rebase_path(path, params->config->work, params->config->src, var->ref->filename, PATH_MAX);
    var->type->arrays.count = 0;
    var->type->arrays.array = NULL;
    var->type->pointers_level = 0;
    STRCP(var_name, var->name, ENTITIES_NAME_LENGTH, 1);
    STRCP(input, var->type->string, ENTITIES_NAME_LENGTH, 1);

    params->source_code->definitions.variables.count++;
  }
  free(input);
  return 0;
}

#define GET_MCOUNT(header, code) ((header) ? (code->definitions.macros.count) : (code->implementations.macros.count))
static int register_macro(const char * path, callback_args params, struct status_s * status) {
  struct macro_multiline_status_s * data = (status->data);
  data->content[data->content_index] = 0;

  struct macro_spec * macro;
  struct macro_args * args = NULL;
  if (data->is_function && (args = malloc(sizeof(struct macro_args))) == NULL) {
    perror("malloc macro_args");
    goto return_fail;
  }

  if (status->is_header) {
    if (params->source_code->definitions.macros.count == 0) {
      if ((params->source_code->definitions.macros.array = malloc(sizeof(struct macro_spec))) == NULL) {
        perror("malloc macro_spec");
        free(args);
        goto return_fail;
      }
    } else {
      if ((params->source_code->definitions.macros.array = realloc(params->source_code->definitions.macros.array, sizeof(struct macro_spec) * (params->source_code->definitions.macros.count + 1))) == NULL) {
        perror("realloc macro_spec");
        free(args);
        goto return_fail;
      }
    }

    macro = params->source_code->definitions.macros.array + params->source_code->definitions.macros.count;
    params->source_code->definitions.macros.count++;
  } else {
    if (params->source_code->implementations.macros.count == 0) {
      if ((params->source_code->implementations.macros.array = malloc(sizeof(struct macro_spec))) == NULL) {
        perror("malloc macro_spec");
        free(args);
        goto return_fail;
      }
    } else {
      if ((params->source_code->implementations.macros.array = realloc(params->source_code->implementations.macros.array, sizeof(struct macro_spec) * (params->source_code->implementations.macros.count + 1))) == NULL) {
        perror("realloc macro_spec");
        free(args);
        goto return_fail;
      }
    }

    macro = params->source_code->implementations.macros.array + params->source_code->implementations.macros.count;
    params->source_code->implementations.macros.count++;
  }
  if ((macro->ref = malloc(sizeof(struct reference))) == NULL) {
    perror("malloc macro_spec->ref");
    if (data->is_function) free(args);
    if (GET_MCOUNT(status->is_header, params->source_code) == 1) free(macro);
    status->data = NULL;
    return 1;
  }

  macro->is_function = data->is_function;
  macro->args = args;
  if (data->is_function) {
    args->count = data->params_count;
    args->array = data->array;
  }
  STRCP(data->name, macro->name, ENTITIES_NAME_LENGTH, 1);
  STRCP(data->content, macro->placeholder, data->content_index, 1);
  macro->ref->line = data->starts_on_line;
  rebase_path(path, params->config->work, params->config->src, macro->ref->filename, PATH_MAX);

  if (params->config->verbose_registration) printf("- Registering macro %s (%zu arguments)\n", macro->name, macro->is_function ? macro->args->count : 0);

  free(status->data);
  status->data = NULL;
  return 0;

return_fail:
  free(status->data);
  status->data = NULL;
  return 1;
}
#undef GET_MCOUNT
int try_extract_macro(char * buffer, const char * path, callback_args params, struct status_s * status) {
  while (*buffer == ' ') buffer++; // We know the line is not empty
  if (*buffer != '#') return 1;

  if (!starts_with(buffer, "#define")) return 1;

  // First determine if it is a text macro or a function macro
  while (*buffer != ' ' && *buffer != '\n') buffer++;
  if (*buffer == '\n') {
    fprintf(stderr, "    /!\\ Incomplete macro definition in %s:%lld\n", path, status->line_count);
    return 1;
  }

  while (*buffer == ' ' && *buffer != '\n') buffer++;
  if (*buffer == '\n') {
    fprintf(stderr, "    /!\\ Incomplete macro definition in %s:%lld\n", path, status->line_count);
    return 1;
  }

  if ((status->data = malloc(sizeof(struct macro_multiline_status_s))) == NULL) {
    perror("malloc macro_multiline buffer");
    return 1;
  }
  struct macro_multiline_status_s * data = status->data;
  data->array = NULL,
  data->is_function = 0;
  data->params_count = 0;
  data->content_index = 0;
  data->in_string = 0;
  data->in_char = 0;
  data->starts_on_line = status->line_count;

  char * parenthesis = NULL;
  char * word = buffer;
  while (*buffer != ' ' && *buffer != '\n') {
    if (*buffer == '(') {
      data->is_function = 1;
      *buffer = 0;
      parenthesis = buffer;
    }

    buffer++;
  }

  if (parenthesis != NULL) *parenthesis = 0;
  *buffer = 0;
  STRCP(word, data->name, ENTITIES_NAME_LENGTH, 1);
  *buffer = ' ';
  if (parenthesis != NULL) *parenthesis = '(';

  if (data->is_function) {
    *parenthesis = '(';

    // Read arguments for macro
    char * i = parenthesis + 1;
    word = parenthesis + 1;

    int escape = 0;
    // TODO : this does not support \ in macro parameters yet
    while (*i != ')') {
      if (*i == ' ') {
        while (*i == ' ') i++;
        word = i;
        continue;
      }

      if (*i == ',') {
register_arg:
        *i = 0;
        if (data->params_count == 0) {
          if ((data->array = malloc(sizeof(char *))) == NULL) {
            perror("malloc");
            free(status->data);
            status->data = NULL;
            return 1;
          }
        } else {
          if ((data->array = realloc(data->array, sizeof(char *) * (data->params_count + 1))) == NULL) {
            perror("realloc");
            free(data->array);
            free(status->data);
            status->data = NULL;
            return 1;
          }
        }

        if ((data->array[data->params_count] = strdup(word)) == NULL) {
          perror("strdup");
          free(data->array);
          free(status->data);
          status->data = NULL;
          return 1;
        };
        data->params_count++;
        if (escape) goto escape_back;
        *i = ',';
        word = i + 1;
      }
      i++;
    }
    escape = 1;
    goto register_arg;
escape_back:
    *i = ')';

    buffer = i + 1;
  } else {
    if (*buffer != '\n') *buffer = ' ';
  }

  while (*buffer == ' ' && *buffer != '\n') buffer++;
  if (*buffer == '\n') {
    if (data->array != NULL) free(data->array);
    fprintf(stderr, "    /!\\ Incomplete macro definition in %s:%lld\n", path, status->line_count);
    free(status->data);
    status->data = NULL;
    return 1;
  }

  char * origin = buffer;
  int * in_string = &(data->in_string);
  int * in_char = &(data->in_char);
  int dontregister = 0;
  // Start recording the following as macro placeholder
  while (*buffer != '\n' && *buffer != 0) { // A macro ends at the end of the line anyway, if we don't have an end of line, screw it, who writes macro over 2048 caracters anyway ?
    if (IS_STRING_DEL(origin, buffer) && !(*in_char)) *in_string = !(*in_string);
    if (IS_CHAR_DEL(origin, buffer) && !(*in_string)) *in_char = !(*in_char);

    if (*buffer == '\\' && (*(buffer + 1) == '\n' || *(buffer + 1) == ' ') && !(*in_string) && !(*in_char)) { // We have a \ that is used, not as an escape char but as a line break
      status->reading_multiline_macro = 1;
      dontregister = 1;
    }

    if (!dontregister) {
      data->content[data->content_index] = *buffer;
      data->content_index++;
    } else dontregister = 0;
    buffer++;
  }

  if (status->reading_multiline_macro == 0) {
    if (register_macro(path, params, status) != 0) {
      fprintf(stderr, "Unable to register macro for file %s at line %lld\n", path, status->line_count);
      free(status->data);
      status->data = NULL;
      return 1;
    }
  }

  return 0;
}

int try_extract_macro_multiline(char * buffer, const char * path, callback_args params, struct status_s * status) {
  // This is pretty  straightforward : Just copy until we reach either end of line, or line break (\) followed by some spaces/end of lines
  struct macro_multiline_status_s * data = status->data;
  const char * origin = buffer;
  int dontregister = 0;

  // If we made it here, that's because there is a newline, so, logically, add it
  data->content[(data->content_index)++] = '\n';
  status->reading_multiline_macro = 0; // Cancel, unless we reach later a new line to manage
  while (*buffer != '\n') {
    if (IS_STRING_DEL(origin, buffer) && !data->in_char) data->in_string = !data->in_string;
    if (IS_CHAR_DEL(origin, buffer) && !data->in_string) data->in_char = !data->in_char;
    if (*buffer == '\\' && (*(buffer + 1) == '\n' || *(buffer + 1) == ' ') && !data->in_string && !data->in_char) {
      status->reading_multiline_macro = 1;
      dontregister = 1;
    }

    if (!dontregister) {
      data->content[data->content_index] = *buffer;
      data->content_index++;
    } else dontregister = 0;
    buffer++;
  }

  if (status->reading_multiline_macro == 0) return register_macro(path, params, status);
  return 0;
}
static void compute_struct_member(char * value, char ** type, char ** name, int * free_type) {
  char * input;
  int valid;
  *type = NULL;
  *name = NULL;
  *free_type = 0;
  if ((input = normalize_variable_input(value, ENTITIES_NAME_LENGTH, &valid)) == NULL) {
    if (!valid) fprintf(stderr, "    /!\\ Incomplete member type\n");
    return;
  }

  *free_type = 1;
  // We now have a normalized input, with no ';', no newline, and one space between each word
  unsigned i = 0;
  char * bracket_ptr = NULL;
  int needs_bracket_operation = 0;
  while (input[i] != 0) {
    if (input[i] == ' ') {
      if (*(input + i + 1) == '[') {// This is not the actual name
        needs_bracket_operation = 1;
        bracket_ptr = input + i + 1;
        break;
      }
      *name = input + i + 1;
    }
    if (input[i] == '(' || input[i] == '{') { // This is not a variable
      free(input);
      return;
    }
    i++;
  }
  if (*name == input) {
    fprintf(stderr, "    /!\\ Incomplete struct member definition\n");
    free(input);
    return;
  }

  if (needs_bracket_operation) { // Then we have type variableName [???];
    swap_word_onwards_inplace(input, *name, bracket_ptr, ENTITIES_NAME_LENGTH);

    // Retrack the pointers to correct position. Variable name is now the last word
    char * tmp = *name;
    while (*tmp != 0) {
      if (*tmp == ' ') *name = tmp + 1;
      tmp++;
    }
  }
  *((*name) - 1) = 0;
  *type = input;
}
#define GET_COUNT(is_header, source_code) ((is_header) ? (source_code->definitions.structs.count) : (source_code->implementations.structs.count))
static int register_structure(const char * filename, callback_args params, struct status_s * status) {
  struct struct_multiline_status_s * data = status->data;

  struct struct_spec * structure;
  if (status->is_header) {
    if (params->source_code->definitions.structs.count == 0) {
      if ((params->source_code->definitions.structs.array = malloc(sizeof(struct struct_spec))) == NULL) {
        perror("malloc struct_spec");
        params->results.fails++;
        return 0;
      }
    } else {
      if ((params->source_code->definitions.structs.array = realloc(params->source_code->definitions.structs.array, sizeof(struct struct_spec) * (params->source_code->definitions.structs.count + 1))) == NULL) {
        perror("realloc struct_spec");
        params->results.fails++;
        return 0;
      }
    }

    structure = params->source_code->definitions.structs.array + params->source_code->definitions.structs.count;
  } else {
    if (params->source_code->implementations.structs.count == 0) {
      if ((params->source_code->implementations.structs.array = malloc(sizeof(struct struct_spec))) == NULL) {
        perror("malloc struct_spec");
        params->results.fails++;
        return 0;
      }
    } else {
      if ((params->source_code->implementations.structs.array = realloc(params->source_code->implementations.structs.array, sizeof(struct struct_spec) * (params->source_code->implementations.structs.count + 1))) == NULL) {
        perror("realloc struct_spec");
        params->results.fails++;
        return 0;
      }
    }

    structure = params->source_code->implementations.structs.array + params->source_code->implementations.structs.count;
  }
  if ((structure->ref = malloc(sizeof(struct reference))) == NULL) {
    perror("malloc struct_spec->ref");
    if (GET_COUNT(status->is_header, params->source_code) == 1) free(structure);
    params->results.fails++;
    return 0;
  }
  if ((structure->members = malloc(sizeof(struct struct_members))) == NULL) {
    perror("malloc struct_spec->members");
    free(structure->ref);
    if (GET_COUNT(status->is_header, params->source_code) == 1) free(structure);
    params->results.fails++;
    return 0;
  }
  structure->members->array = NULL;
  if (data->members_count > 0) {
    if ((structure->members->array = malloc(sizeof(struct struct_member) * data->members_count)) == NULL) {
      perror("malloc struct_spec->members->array");
      free(structure->members);
      free(structure->ref);
      if (GET_COUNT(status->is_header, params->source_code) == 1) free(structure);
      params->results.fails++;
      return 0;
    }

    ulint i = 0;
    while (i < data->members_count) {
      char srcbuf[ENTITIES_NAME_LENGTH];
      normalize_type_string(data->members_array[i], srcbuf, 0);

      char * typebuf = NULL;
      char * namebuf = NULL;
      int free_type = 0;

      compute_struct_member(srcbuf, &typebuf, &namebuf, &free_type);

      if (typebuf == NULL || namebuf == NULL) {
        params->results.fails++;
        free(structure->members->array);
        free(structure->members);
        free(structure->ref);
        if (GET_COUNT(status->is_header, params->source_code) == 1) free(structure);
        if (free_type && typebuf != NULL) free(typebuf);
        return 0;
      }
      STRCP(namebuf, structure->members->array[i].name, ENTITIES_NAME_LENGTH, 1);
      STRCP(typebuf, structure->members->array[i].type.string, ENTITIES_NAME_LENGTH, 1);
      structure->members->array[i].type.type = NoneType;
      structure->members->array[i].type.ptr = NULL;
      structure->members->array[i].type.pointers_level = 0;
      structure->members->array[i].type.arrays.count = 0;
      structure->members->array[i].type.arrays.array = NULL;
      free(data->members_array[i]);

      if (free_type) free(typebuf);
      i++;
    }
  }

  STRCP(data->name, structure->name, ENTITIES_NAME_LENGTH, 1);
  STRCP((char *)filename, structure->ref->filename, PATH_MAX, 1);
  structure->members->count = data->members_count;
  structure->ref->line = data->starts_on_line;

  if (status->is_header) params->source_code->definitions.structs.count++;
  else params->source_code->implementations.structs.count++;

  if (data->members_array != NULL) free(data->members_array);
  free(data);
  status->data = NULL;

  if (params->config->verbose_registration) printf("- Registering structure %s (%zu members)\n", structure->name, structure->members->count);

  return 0;
}
#undef GET_COUNT

int try_extract_struct(char * buffer, const char * path, callback_args params, struct status_s * status) {
  struct struct_multiline_status_s * struct_data = NULL;

  while (*buffer == ' ' && *buffer != 0) buffer++;
  if (starts_with(buffer, "extern") || starts_with(buffer, "static") || starts_with(buffer, "volatile")) { // Skip this word
    while (*buffer != ' ' && *buffer != 0) buffer++;
    if (*buffer == 0) goto incomplete;

    while (*buffer == ' ' && *buffer != 0) buffer++;
    if (*buffer == 0) goto incomplete;
  }
  if (!starts_with(buffer, "struct ")) return 1; // This is just not a struct, skipping (unless some madman defined a macro for struct)

  if ((struct_data = malloc(sizeof(struct struct_multiline_status_s))) == NULL) {
    perror("malloc struct data");
    params->results.fails++;
    return 1;
  }
  status->data = struct_data;

  struct_data->members_count = 0;
  struct_data->members_array = NULL;
  struct_data->reading_members = 0;
  struct_data->reading_member = 0;
  struct_data->starts_on_line = status->line_count;

  // This word is "struct", so skip it
  buffer += 6;
  while (*buffer == ' ' && *buffer != 0) buffer++;
  if (*buffer == 0) goto incomplete;

  // The next word is the name
  ulint i = 0;
  while (*buffer != ' ' && *buffer != '{' && *buffer != ';' && *buffer != 0) {
    if (i == ENTITIES_NAME_LENGTH - 1) {
      fprintf(stderr, "The name of the structure at %s:%lld is too long for the buffer (max: %d caracters)\n", path, status->line_count, ENTITIES_NAME_LENGTH);
      params->results.fails++;
      free(struct_data);
      status->data = NULL;
      return 1;
    }

    struct_data->name[i] = *buffer;
    i++;
    buffer++;
  }
  if (i == 0) goto incomplete;  // Who in their right mind does this ? struct {, struct ;
  struct_data->name[i] = 0;

  // Go on a simple implementation for now : either look up for a ; immediately after the name (on the same line), or a {\n after the name. We will handle the case if an issue is opened on the project
  while (*buffer != 0) {
    if (*buffer == '{') {
      buffer++;
      while (*buffer == ' ' && *buffer != 0) buffer++;
      if (*buffer == '\n') { // Valid, say we are reading multiline and call it a day
        status->reading_multiline_struct = 1;
      } else if (*buffer == '}') { // Also valid, but this is an empty structure
        return register_structure(path, params, status);
      }
    }
    if (*buffer == ' ') {
      buffer++;
      continue;
    }
    if (*buffer == ';') { // Strange but valid, we can register it now : it is something like struct data; which is valid
      return register_structure(path, params, status);
    }
    if (*buffer == '\n') {
      status->reading_multiline_struct = 1;
      return 0; // Let the other function to take the members
    }
    // Anything other than these are unwanted, it is either a variable or some obscure ass definition
    if (struct_data != NULL) {
      if (struct_data->members_array != NULL) free(struct_data->members_array);
      free(struct_data);
      status->data = NULL;
    }
    return 1;
  }
  if (*buffer == 0) { // Then essentially we have struct name and more spaces on the line, we'll throw an error
    goto incomplete;
  }

  if (status->reading_multiline_struct == 0) {
    return register_structure(path, params, status);
  }
  if (struct_data != NULL) {
    if (struct_data->members_array != NULL) free(struct_data->members_array);
    free(struct_data);
  }
  return 0;
incomplete:
  fprintf(stderr, "    /!\\ Incomplete structure definition in %s line %lld\n", path, status->line_count);
  params->results.fails++;
  if (struct_data != NULL) {
    if (struct_data->members_array != NULL) free(struct_data->members_array);
    free(struct_data);
    status->data = NULL;
  }
  return 1;
}

static void register_struct_member(char * start, struct struct_multiline_status_s * data) {
  char * typestart = start;
  while (*start != ';') start++; // Just find  the ; and set it to null, just to register it

  if (data->members_array == NULL) {
    if ((data->members_array = malloc(sizeof(char *))) == NULL) {
      perror("malloc struct member name");
      return;
    }
  } else {
    if ((data->members_array = realloc(data->members_array, sizeof(char *) * (data->members_count + 1))) == NULL) {
      perror("realloc struct member name");
      return; 
    }
  }

  *start = 0;
  if ((data->members_array[data->members_count] = strdup(typestart)) == NULL) {
    // Leave the counter untouched, only if it positive, otherwise free it, or it will be reallocated again and leave memory leaking
    perror("strdup");
    if (data->members_count == 0) free(data->members_array);
    return;
  }
  *start = ';';
  data->members_count++;
}
int try_extract_struct_multiline(char * buffer, const char * path, callback_args params, struct status_s * status) {
  struct struct_multiline_status_s * data = status->data;
  // Just read until end of line ig
  // Keep multiline enabled until we reach }
  char * start = buffer; // Keep track of start of the line
  int registering_something = 0;
  while (*buffer != 0) {
    if (*buffer == '}') {
      status->reading_multiline_struct = 0;
      break;
    }

    if (!registering_something) {
      if (*buffer == ' ') {
        buffer++;
        continue;
      }
      if (*buffer == ';') { // That's some incomplete type, we have : struct something { \   ;}
        goto incomplete;
      }
      // We start registering here, until we meet a ;
      registering_something = 1;
      start = buffer;
    } else {
      if (*buffer == ';') { // Register member and continue
        register_struct_member(start, data);
        registering_something = 0;
        start = buffer + 1; // At least get a valid buffer
      }
      buffer++;
    }
  }
  if (status->reading_multiline_struct == 0) {
    return register_structure(path, params, status);
  }
  return 0;
incomplete:
  fprintf(stderr, "Incomplete structure definition in %s line %lld\n", path, status->line_count);
  if (data->members_array != NULL) free(data->members_array);
  free(data);
  status->data = NULL;
  return 1;
}

#define GET_FCOUNT(header, code) ((header) ? (code->definitions.functions.count) : (code->implementations.functions.count))
#define FREE_DATA(status, data) {\
  if (data->args_count > 0) {\
    while (data->args_count > 0) {\
      free(data->args[data->args_count - 1]);\
      (data->args_count)--;\
    }\
    free(data->args);\
  };\
  free(data);\
  status->data = NULL;\
}
void register_function(const char * path, callback_args params, struct status_s * status) {
  struct function_multiline_status_s * data = status->data;
  // Do some checks before registering, like keywords that could have a parenthesis "if"
  if (streq_with_crumb(data->name, "if", ' ')) {
    FREE_DATA(status, data);
    return;
  }

  struct function_spec * function;
  if (data->doubleptr != NULL) {
    if ((*(data->doubleptr) = malloc(sizeof(struct function_spec))) == NULL) {
      perror("malloc struct function_specs");
      params->results.fails++;
      return;
    }
    function = *(data->doubleptr);
  } else if (status->is_header) {
    if (params->source_code->definitions.functions.count == 0) {
      if ((params->source_code->definitions.functions.array = malloc(sizeof(struct function_spec))) == NULL) {
        perror("malloc struct function_spec");
        params->results.fails++;
        return;
      }
    } else {
      if ((params->source_code->definitions.functions.array = realloc(params->source_code->definitions.functions.array, sizeof(struct function_spec) * (params->source_code->definitions.functions.count + 1))) == NULL) {
        perror("realloc struct function_spec");
        params->results.fails++;
        return;
      }
    }

    function = params->source_code->definitions.functions.array + params->source_code->definitions.functions.count;
    params->source_code->definitions.functions.count++;
  } else {
    if (params->source_code->implementations.functions.count == 0) {
      if ((params->source_code->implementations.functions.array = malloc(sizeof(struct function_spec))) == NULL) {
        perror("malloc struct function_spec");
        params->results.fails++;
        return;
      }
    } else {
      if ((params->source_code->implementations.functions.array = realloc(params->source_code->implementations.functions.array, sizeof(struct function_spec) * (params->source_code->implementations.functions.count + 1))) == NULL) {
        perror("realloc struct function_spec");
        params->results.fails++;
        return;
      }
    }

    function = params->source_code->implementations.functions.array + params->source_code->implementations.functions.count;
    params->source_code->implementations.functions.count++;
  }
  if ((function->ref = malloc(sizeof(struct reference))) == NULL) {
    perror("malloc function->ref");
    if (data->doubleptr != NULL || GET_FCOUNT(status->is_header, params->source_code) == 1) free(function);
    FREE_DATA(status, data);
    params->results.fails++;
    return;
  }
  if ((function->args = malloc(sizeof(struct function_args))) == NULL) {
    perror("malloc function->args");
    free(function->ref);
    if (data->doubleptr != NULL || GET_FCOUNT(status->is_header, params->source_code) == 1) free(function);
    FREE_DATA(status, data);
    params->results.fails++;
    return;
  }
  function->args->count = data->args_count;
  if (data->args_count > 0) {
    if (data->args_count == 1) {
      if (streq_with_crumb(data->args[0], "void", ' ') == 1) { // This function is actually like fn(void), so no argument is defined
        function->args->count = 0;
        function->args->array = NULL;
        goto if_exit;
      }
    }
    if ((function->args->array = malloc(sizeof(struct function_arg) * data->args_count)) == NULL) {
      perror("malloc function->args->array");
      params->results.fails++;
      free(function->ref);
      free(function->args);
      if (GET_FCOUNT(status->is_header, params->source_code) == 1) free(function);
      FREE_DATA(status, data);
      return;
    }

    ulint i = 0;
    while (i < data->args_count) {
      char * input;
      int valid;
      if ((input = normalize_variable_input(data->args[i], ENTITIES_NAME_LENGTH, &valid)) == NULL) {
        if (!valid) fprintf(stderr, "    /!\\ Invalid function argument construction\n");
almost_complete_fail:
        params->results.fails++;
        free(function->ref);
        free(function->args->array);
        free(function->args);
        if (data->doubleptr != NULL || GET_FCOUNT(status->is_header, params->source_code) == 1) free(function);
        FREE_DATA(status, data);
        if (status->is_header) params->source_code->definitions.functions.count--;
        else params->source_code->implementations.functions.count--;
        return;
      }

      char * var_name = input;
      unsigned j = 0;
      char * bracket_ptr = NULL;
      int needs_bracket_operation = 0;
      while (input[j] != 0) {
        if (input[j] == ' ') {
          if (*(input + j + 1) == '[') {
            needs_bracket_operation = 1;
            bracket_ptr = input + j + 1;
            break;
          }
          var_name = input + j + 1;
        }
        if (input[j] == '(' || input[j] == '{') {
          free(input);
          goto almost_complete_fail;
        }
        j++;
      }
      int replaced = 0;
      char * zero = var_name;
      if (var_name == input || *var_name == '*') { // That's the case when we have only 1 word, so it has to be a type
                                                   // Or the case when it is a pointer
        char namebuf[ENTITIES_NAME_LENGTH];
        snprintf(namebuf, ENTITIES_NAME_LENGTH, "__arg_no_%zu", i + 1);
        var_name = namebuf;
        replaced = 1;
      }

      if (needs_bracket_operation && !replaced) { // Then we have type variableName [???];
        swap_word_onwards_inplace(input, var_name, bracket_ptr, ENTITIES_NAME_LENGTH);

        // Retrack the pointers to correct position. Variable name is now the last word
        char * tmp = var_name;
        while (*tmp != 0) {
          if (*tmp == ' ') var_name = tmp + 1;
          tmp++;
        }
      }
      if (!replaced) *(zero - 1) = 0;
      STRCP(var_name, function->args->array[i].name, ENTITIES_NAME_LENGTH, 1);
      STRCP(input, function->args->array[i].type.string, ENTITIES_NAME_LENGTH, 1);
      function->args->array[i].type.type = NoneType;
      function->args->array[i].type.ptr = NULL;
      function->args->array[i].type.pointers_level = 0;
      function->args->array[i].type.arrays.array = NULL;
      function->args->array[i].type.arrays.count = 0;

      free(input);
      i++;
    }
  } else function->args->array = NULL;
if_exit:

  STRCP(data->name, function->name, ENTITIES_NAME_LENGTH, 1);
  STRCP(data->returntype.string, function->returntype.string, ENTITIES_NAME_LENGTH, 1);
  rebase_path(path, params->config->work, params->config->src, function->ref->filename, PATH_MAX);
  function->ref->line = data->starts_on_line;
  function->modifiers = data->modifiers;
  function->returntype.type = data->returntype.type;
  function->returntype.ptr = NULL;
  function->returntype.pointers_level = 0;
  function->returntype.arrays.array = NULL;
  function->returntype.arrays.count = 0;
  function->description = NULL;

  FREE_DATA(status, data);

  if (params->config->verbose_registration) printf("- Registering function %s (%zu arguments)\n", function->name, function->args->count);

  return;
}
#undef GET_FCOUNT
static int word_eq(const char * start, const char * word) {
  while (*start != 0 && *start != ' ' && *word != 0) {
    if (*start != *word) return 0;
    start++;
    word++;
  }
  return (*start == 0 || *start == ' ') && *word == 0; // Consumed both strings
}
#define SKIP_WORD(buffer) while (*(buffer) != ' ') buffer++;

int try_extract_function(char *buffer, const char *path, callback_args args, struct status_s * status) {
  // For now we will focus only on oneline functions declarations/implementations
  // Also, we will ignore functions DECLARATIONS in source files
  while (*buffer == ' ') buffer++;
  if (*buffer == '#') return 0;

  struct function_multiline_status_s * data;
  if ((data = malloc(sizeof(struct function_multiline_status_s))) == NULL) {
    perror("malloc function status");
    args->results.fails++;
    return 1;
  }
  status->data = data;
  data->doubleptr = NULL;
  data->modifiers = 0;
  data->reading_arg = 0;
  data->reading_args = 0;
  data->args_count = 0;
  data->args = NULL;
  data->returntype.type = NoneType;
  data->returntype.ptr = NULL;
  data->returntype.pointers_level = 0;
  data->returntype.arrays.count = 0;
  data->returntype.arrays.array = NULL;
  data->returntype.string[0] = 0;
  data->name[0] = 0;
  data->starts_on_line = status->line_count;
  data->filenameptr = path;
  data->words_counted = 0;
  data->is_declaration = 0;
  data->parameter_index = 0;

  char * start = buffer;
  char * name = buffer;
  char * arg_start = buffer;
  char * arg_check = NULL;
  char * typestart = buffer;
  while (*buffer != 0 && *buffer != '\n') {
    if (IS_CHAR_DEL(start, buffer) || IS_STRING_DEL(start, buffer)) {}
    if (!data->reading_args) {
      if (start == buffer || *(buffer - 1) == ' ') {
        if (word_eq(buffer, "extern")) {
          data->modifiers |= F_MEXTERN;
          SKIP_WORD(buffer);
          typestart = buffer;
          continue;
        }
        if (word_eq(buffer, "static")) {
          data->modifiers |= F_MSTATIC;
          SKIP_WORD(buffer);
          typestart = buffer;
          continue;
        }
        if (word_eq(buffer, "volatile")) {
          data->modifiers |= F_MVOLATI;
          SKIP_WORD(buffer);
          typestart = buffer;
          continue;
        }
      } // So we determined modifiers... now pay attention to :
        // 1. Return type
        // 2. Function name
        // 3. Arguments
        // We still don't know if this is actually a function.
      if (*buffer == ' ') {
        data->words_counted++;
        while (*buffer == ' ') buffer++;
        continue; // Just to get the checks one more time
      }
      if (*buffer == '(') { // We must have at least 1 word, preferably 2. If it's one, then the previous char must not be a space
        if (data->words_counted == 0) goto cleanup_fail;
        if (data->words_counted == 1 && *(buffer - 1) == ' ') goto cleanup_fail; // It's invalid : word ( is not a function (unless on multiline but fuck multiline for now)

        // We can register types/names for now. We will use some normalization
        char * input;
        int valid;
        *buffer = 0; // Just so we don't overflow. I promise I will repair it afterwards
        if ((input = normalize_variable_input(typestart, ENTITIES_NAME_LENGTH * 2, &valid)) == NULL) {
          if (!valid) fprintf(stderr, "   /!\\ Malformed function in %s:%lld\n", path, status->line_count);
          goto cleanup_fail;
        }
        *buffer = '(';

        // For functions it's actually easier since they can't return arrays, we just register then copy
        unsigned i = 0;
        while (input[i] != 0) {
          if (input[i] == ' ') name = input + i + 1;
          i++;
        }

        *(name - 1) = 0;
        STRCP(name, data->name, ENTITIES_NAME_LENGTH, 1);
        STRCP(input, data->returntype.string, ENTITIES_NAME_LENGTH, 1);

        free(input);
        data->reading_args = 1;
        data->reading_arg = 1;
        arg_start = buffer + 1;
        arg_check = buffer + 1;
      }
    } else {
      if (*buffer == ')') {
        data->reading_args = 0;
        data->reading_arg = 0;
        if (arg_check == arg_start) { // We matched maybe 1 argument, maybe 0. Time for backtracking and see  if we reach anything other than a (. If so, we do have an argument
          char * backtrack = buffer - 1;
          while (*backtrack != '(') { // We know we have one
            if (*backtrack != ' ') goto resume;
            backtrack--;
          }
          break; // Cursed aah loop
        }
resume:
        *buffer = 0;

        if (data->args_count == 0) {
          if ((data->args = malloc(sizeof(struct function_arg))) == NULL) {
            perror("malloc function_arg");
            goto cleanup_fail;
          }
        } else {
          if ((data->args = realloc(data->args, (data->args_count + 1) * sizeof(struct function_arg))) == NULL) {
            perror("realloc function_arg");
            goto cleanup_fail;
          }
        }
        if ((data->args[data->args_count] = strdup(arg_start)) == NULL) {
          perror("strdup function argument");
          goto cleanup_fail;
        }
        data->args_count++;
        data->reading_arg = 0;
        data->reading_args = 0;

        *buffer = ')';
        break;
      } else if (*buffer == ',') {
        *buffer = 0;
        if (data->args_count == 0) {
          if ((data->args = malloc(sizeof(struct function_arg))) == NULL) {
            perror("malloc function_arg");
            goto cleanup_fail;
          }
        } else {
          if ((data->args = realloc(data->args, (data->args_count + 1) * sizeof(struct function_arg))) == NULL) {
            perror("realloc function_arg");
            goto cleanup_fail;
          }
        }
        if ((data->args[data->args_count] = strdup(arg_start)) == NULL) {
          perror("strdup function argument");
          goto cleanup_fail;
        }
        data->args_count++;
        *buffer = ',';
        arg_start = buffer + 1;
        data->parameter_index = 0;
      }
    }

    buffer++;
  }

  if (data->name[0] == 0 || data->returntype.string[0] == 0) { // This was not a function after all
    goto cleanup_fail;
  }
  // Quick check on name : if it includes something like a dot it's not a function
  ulint check_index = 0;
  while (data->name[check_index] != 0 && check_index < ENTITIES_NAME_LENGTH) {
    if (data->name[check_index] == '.') goto cleanup_fail;
    check_index++;
  }
  if (*buffer != 0 && !data->reading_args) {
    while (*buffer != '{' && *buffer != ';' && *buffer != 0) buffer++;
    data->is_declaration = *buffer == ';'; // Consider everything else an implementation really
  }
  if (data->is_declaration && !status->is_header) {
    FREE_DATA(status, data);
    return 0; // We matched something, so skip in the main loop
  }
  if (!data->reading_args && !data->reading_arg) {
    if (status->automatic) register_function(path, args, status);
    return 0;
  } else {
    status->reading_function_params = 1;
    return 0;
  }

  int returncode;
cleanup_fail:
  returncode = 1;
  goto cleanup;
cleanup:
  FREE_DATA(status, data);
  return returncode;
}

int try_extract_function_multiline(char * buffer, const char * path, callback_args args, struct status_s * status) {
  status->reading_function_params = 1;
  struct function_multiline_status_s * data = status->data;

  while (*buffer == ' ') buffer++;
  // Now we really are looking for
  // 1. ,
  // 2. )
  // If we have nothing, consider it is on the next line
  while (*buffer != 0) { // Just go
    if (*buffer == ',') {
      *buffer = 0;
      if (data->args_count == 0) {
        if ((data->args = malloc(sizeof(struct function_arg))) == NULL) {
          perror("malloc function_arg");
          goto fail;
        }
      } else {
        if ((data->args = realloc(data->args, (data->args_count + 1) * sizeof(struct function_arg))) == NULL) {
          perror("realloc function_arg");
          goto fail;
        }
      }
      data->multiline_parameter_buffer[data->parameter_index] = 0;
      if ((data->args[data->args_count] = strdup(data->multiline_parameter_buffer)) == NULL) {
        perror("strdup function argument");
        goto fail;
      }
      data->args_count++;
      *buffer = ',';
      data->parameter_index = 0;
    }
    if (*buffer == ')') {
      *buffer = 0;
      if (data->args_count == 0) {
        if ((data->args = malloc(sizeof(struct function_arg))) == NULL) {
          perror("malloc function_arg");
          goto fail;
        }
      } else {
        if ((data->args = realloc(data->args, (data->args_count + 1) * sizeof(struct function_arg))) == NULL) {
          perror("realloc function_arg");
          goto fail;
        }
      }
      data->multiline_parameter_buffer[data->parameter_index] = 0;
      if ((data->args[data->args_count] = strdup(data->multiline_parameter_buffer)) == NULL) {
        perror("strdup function argument");
        goto fail;
      }
      data->args_count++;
      *buffer = ')';
      data->parameter_index = 0;
      data->reading_args = 0;
      data->reading_arg = 0;
      status->reading_function_params = 0;
      break;
    }
    if (data->reading_arg) {
      if (*buffer == ' ') {
        if (data->parameter_index > 0 && data->multiline_parameter_buffer[data->parameter_index - 1] != ' ') data->multiline_parameter_buffer[data->parameter_index++] = ' ';
      } else if (*buffer != '\n' && *buffer != ',') data->multiline_parameter_buffer[data->parameter_index++] = *buffer;
    }

    buffer++;
  }

  if (!(status->reading_function_params)) { // Run some further checks before actually registering it
    if (data->name[0] == 0 || data->returntype.string[0] == 0) { // This was not a function after all (we never know eh)
      FREE_DATA(status, data);
      return 0;
    }
    if (*buffer != 0 && !data->reading_args) {
      while (*buffer != '{' && *buffer != ';' && *buffer != 0) buffer++;
      data->is_declaration = *buffer == ';'; // Consider everything else an implementation really
    }
    if (data->is_declaration && !status->is_header) {
      FREE_DATA(status, data);
      return 0;
    }
    status->data = data;
    if (status->automatic) register_function(path, args, status);
  }
  return 0;
fail:
  FREE_DATA(status, data);
  args->results.fails++;
  status->reading_function_params = 0;
  return 0;
}
#define GET_ECOUNT(header, code) ((header) ? (code->definitions.enums.count) : (code->implementations.enums.count))
#define FREE_ENUMDATA(status, data) {\
  if (data->members_count > 0 && data->members != NULL) free(data->members);\
  free(data);\
  status->data = NULL;\
}
void register_enum(const char * path, callback_args args, struct status_s * status) {
  struct enum_multiline_status_s * data = status->data;

  struct enum_spec * enumerator = NULL;
  if (data->doubleptr != NULL) {
    if ((*(data->doubleptr) = malloc(sizeof(struct enum_spec))) == NULL) {
      perror("malloc enum_spec");
      args->results.fails++;
      return;
    }
    enumerator = *(data->doubleptr);
  } else if (status->is_header) {
    if (args->source_code->definitions.enums.count == 0) {
      if ((args->source_code->definitions.enums.array = malloc(sizeof(struct enum_spec))) == NULL) {
        perror("malloc struct enum_spec");
        goto fail;
      }
    } else {
      if ((args->source_code->definitions.enums.array = realloc(args->source_code->definitions.enums.array, sizeof(struct enum_spec) * (args->source_code->definitions.enums.count + 1))) == NULL) {
        perror("realloc struct enum_spec");
        goto fail;
      }
    }

    enumerator = args->source_code->definitions.enums.array + args->source_code->definitions.enums.count;
    args->source_code->definitions.enums.count++;
  } else {
    if (args->source_code->implementations.enums.count == 0) {
      if ((args->source_code->implementations.enums.array = malloc(sizeof(struct enum_spec))) == NULL) {
        perror("malloc struct enum_spec");
        goto fail;
      }
    } else {
      if ((args->source_code->implementations.enums.array = realloc(args->source_code->implementations.enums.array, sizeof(struct enum_spec) * (args->source_code->implementations.enums.count + 1))) == NULL) {
        perror("realloc struct enum_spec");
        goto fail;
      }
    }

    enumerator = args->source_code->implementations.enums.array + args->source_code->implementations.enums.count;
    args->source_code->implementations.enums.count++;
  }

  enumerator->ref = NULL;
  enumerator->members = NULL;
  if ((enumerator->ref = malloc(sizeof(struct reference))) == NULL) {
    perror("malloc enumerator->ref");
    goto fail_with_free;
  }
  if ((enumerator->members = malloc(sizeof(struct enum_members))) == NULL) {
    perror("malloc enumerator->members");
    goto fail_with_free;
  }
  enumerator->modifiers = data->modifiers;
  enumerator->members->count = data->members_count;
  enumerator->members->array = data->members; // Since these are already malloced, we can just pass the pointer. Just don't free it afterwards
  enumerator->ref->line = data->starts_on_line;
  STRCP(data->name, enumerator->name, ENTITIES_NAME_LENGTH, 1);
  STRCP((char *)path, enumerator->ref->filename, PATH_MAX, 1);

  if (args->config->verbose_registration) printf("- Registering enumerator %s (%zu members)\n", enumerator->name, enumerator->members->count);

  free(data);
  status->data = NULL;
  status->reading_multiline_enum = 0;
  return;
fail_with_free:
  if (enumerator->ref != NULL) free(enumerator->ref);
  if (enumerator->members != NULL) free(enumerator->members);
  if (data->doubleptr != NULL || GET_ECOUNT(status->is_header, args->source_code) == 1) status->is_header ? free(args->source_code->definitions.enums.array) : free(args->source_code->implementations.enums.array);
  if (status->is_header) args->source_code->definitions.enums.count--;
  else args->source_code->implementations.enums.count--;
  goto fail;
fail:
  args->results.fails++;
  goto skip;
skip:
  FREE_ENUMDATA(status, data);
  status->reading_multiline_enum = 0;
  return;
}
#undef GET_ECOUNT
#undef FREE_ENUMDATA
#define SKIP_VOID(p) while (*(p) == ' ') p++;
#define SKIP_BOTH(p) {\
  SKIP_WORD(p)\
  SKIP_VOID(p);\
}

#define INCREMENT_RES(res, val, times) res = res * times + (long long int)val;
static long long int parse_from_hex(char * input, size_t max) {
  char is_negative = 0;
  if (*input == '-') {
    is_negative = 1;
    input++;
  } else if (*input == '+') input++;
  input += 2;

  long long int res = 0;
  while (max > 0 && *input != 0) {
    if (48 <= *input && *input <= 57) INCREMENT_RES(res, input - 48, 16)
    else if (97 <= *input && *input <= 102) INCREMENT_RES(res, input - 97, 16)
    else if (65 <= *input && *input <= 70) INCREMENT_RES(res, input - 65, 16)
    max--;
    input++;
  }
  return res * (is_negative == 1 ? -1 : 1);
}
static long long int parse_from_octal(char * input, size_t max) {
  char is_negative = 0;
  if (*input == '-') {
    is_negative = 1;
    input++;
  } else if (*input == '+') input++;
  input++;

  long long int res = 0;
  while (max > 0 && *input != 0) {
    if (48 <= *input && *input <= 55) INCREMENT_RES(res, input - 48, 8);
    max--;
    input++;
  }

  return res * (is_negative ? -1 : 1);
}
static long long int parse_from_binary(char * input, size_t max) {
  char is_negative = 0;
  if (*input == '-') {
    is_negative = 1;
    input++;
  } else if (*input == '+') input++;

  long long int res = 0;
  while (max > 0 && *input != 0) {
    if (*input == 48 || *input == 49) INCREMENT_RES(res, input - 48, 2);
    max--;
    input++;
  }

  return res * (is_negative ? -1 : 1);
}
static void increment_number(char * input, char * outputbuf, size_t max) {
  if (*input == '0' && *(input + 1) == 'x') { // We have to work with hexadecimal. Pray for us
    long long int value = parse_from_hex(input, max);
    snprintf(outputbuf, max, "0x%llx", value + 1);
  } else if (*input == '0' && *(input + 1) == 'b') { // We have to work with binary
    long long int value = parse_from_binary(input, max);
    *outputbuf = '0';
    *(outputbuf + 1) = 'b';
    int index = 2;
    while (value) {
      if (value & 1) outputbuf[index++] = '1';
      else outputbuf[index++] = '0';
      value >>= 1;
    }
  } else if (*input == '0') { // We are working with octal
    long long int value = parse_from_octal(input, max);
    snprintf(outputbuf, max, "0%llo", value);
  } else { // Decimal
    long long int value = parse_llint(input, NULL);
    snprintf(outputbuf, max, "%lld", value + 1);
  }
}
static int register_enum_member(struct enum_multiline_status_s * data) {
  if (data->members_count == 0) {
    if ((data->members = malloc(sizeof(struct enum_member))) == NULL) {
      perror("malloc struct enum_member");
      return 1;
    }
  } else {
    if ((data->members = realloc(data->members, sizeof(struct enum_member) * (data->members_count + 1))) == NULL) {
      perror("realloc struct enum_member");
      return 1;
    }
  }

  data->value_buffer[data->value_buffer_index] = 0;
  if (data->found_equal == 1) STRCP(data->value_buffer, data->members[data->members_count].value, ENTITIES_NAME_LENGTH, 1)
  else {
    if (data->members_count == 0) {
      data->members[data->members_count].value[0] = '0';
      data->members[data->members_count].value[1] = 0;
    } else increment_number(data->members[data->members_count - 1].value, data->members[data->members_count].value, ENTITIES_NAME_LENGTH);
  }

  data->member_name_buffer[data->member_name_index] = 0;
  STRCP(data->member_name_buffer, data->members[data->members_count].name, ENTITIES_NAME_LENGTH, 1);

  data->members_count++;
  data->found_equal = 0;
  data->member_name_index = 0;
  data->value_buffer_index = 0;
  return 0;
}
int try_extract_enum(char * buffer, const char * path, callback_args args, struct status_s * status) {
  while (*buffer == ' ') buffer++;
  if (*buffer == '#') return 1;

  struct enum_multiline_status_s * data;
  if ((data = malloc(sizeof(struct enum_multiline_status_s))) == NULL) {
    perror("malloc enum_multiline_status_s");
    args->results.fails++;
    return 1;
  }
  data->doubleptr = NULL;
  data->modifiers = 0;
  data->members = NULL;
  data->starts_on_line = status->line_count;
  data->members_count = 0;
  data->member_name_index = 0;
  data->value_buffer_index = 0;
  data->found_equal = 0;
  data->name[0] = 0;
  data->value_buffer[0] = 0;
  data->member_name_buffer[0] = 0;

  char * turtle;
  char * start = buffer;
  if (starts_with(buffer, "enum")) {
    SKIP_BOTH(buffer);
    goto did_checks;
  }
  while (*buffer != 0 && *buffer != '\n') {
    if (starts_with(buffer, "extern")) {
      data->modifiers |= F_MEXTERN;
      SKIP_BOTH(buffer);
      continue;
    }
    if (starts_with(buffer, "volatile")) {
      data->modifiers |= F_MVOLATI;
      SKIP_BOTH(buffer);
      continue;
    }
    if (starts_with(buffer, "static")) {
      data->modifiers |= F_MSTATIC;
      SKIP_BOTH(buffer);
      continue;
    }
    buffer++;
  }
  buffer = start; // Restore from beginning, then, we will position ourself at the correct spot
  if (F_ISTATIC(data->modifiers)) SKIP_BOTH(buffer);
  if (F_IVOLATI(data->modifiers)) SKIP_BOTH(buffer);
  if (F_IEXTERN(data->modifiers)) SKIP_BOTH(buffer);

  if (!starts_with(buffer, "enum")) { // This is just not an enum man
    free(data);
    return 1;
  }
  SKIP_BOTH(buffer);
did_checks: // From there, buffer is expected to be positionned at the beginning of the said name (it might not be the name, but that's where it is supposed to be)
  turtle = buffer;
  while (*turtle != ' ') turtle++;
  if (turtle == buffer) {
    fprintf(stderr, "    /!\\ Invalid enum construction at %s:%lld\n", path, status->line_count);
    free(data);
    return 1;
  }
  *turtle = 0;
  STRCP(buffer, data->name, ENTITIES_NAME_LENGTH, 1);
  *turtle = ' ';

  // Now go through, we expect either '{' or ';'
  SKIP_VOID(turtle);
  if (*turtle != '{' && *turtle != ';') { // We could have a scenario like enum Thing
                                          // { ... but if it's really a problem I will receive an issue
    free(data);
    return 1;
  }
  if (*turtle == ';') { // Simple declaration of enumerator, telling the compiler that it exists
    status->data = data;
    if (status->automatic) register_enum(path, args, status);
    return 1;
  }

  turtle++;
  while (*turtle != 0 && *turtle != '\n') {
    if (*turtle == '}') {
      if (data->member_name_index != 0) {
        if (register_enum_member(data) != 0) {
          if (data->members_count > 0) free(data->members);
          free(data);
          return 1;
        }
      }
      status->data = data;
      if (status->automatic) register_enum(path, args, status);
      return 1;
    }
    if (*turtle == ',') {
      if (data->member_name_index == 0) {
        fprintf(stderr, "    /!\\ Malformed enum members at %s:%lld\n", path, status->line_count);
        if (data->members_count > 0) free(data->members);
        free(data);
        return 0;
      }

      register_enum_member(data);
      turtle++;
      continue;
    }
    if (*turtle == ' ') {
      turtle++;
      continue;
    }
    if (*turtle == '=') {
      if (data->found_equal == 1) {
        fprintf(stderr, "    /!\\ Malformed enum member at %s:%lld\n", path, status->line_count);
        if (data->members_count > 0) free(data->members);
        free(data);
        return 0;
      }
      data->found_equal = 1;
      turtle++;
      continue;
    }
    if (data->found_equal) data->value_buffer[data->value_buffer_index++] = *turtle;
    else data->member_name_buffer[data->member_name_index++] = *turtle;
    turtle++;
  }
  // If we are here, we didn't trigger anything, so we should continue to search
  status->reading_multiline_enum = 1;
  status->data = data;

  return 0;
}

int try_extract_enum_multiline(char *buffer, const char *path, callback_args args, struct status_s * status) {
  struct enum_multiline_status_s * data = status->data;

  while (*buffer != 0 && *buffer != '\n') {
    if (*buffer == '}') {
      if (data->member_name_index != 0) {
        if (register_enum_member(data) != 0) {
          if (data->members_count > 0) free(data->members);
          free(data);
          return 1;
        }
      }
      status->data = data;
      register_enum(path, args, status);
      return 1;
    }
    if (*buffer == ',') {
      if (data->member_name_index == 0) {
        fprintf(stderr, "    /!\\ Malformed enum members at %s:%lld\n", path, status->line_count);
        if (data->members_count > 0) free(data->members);
        free(data);
        return 0;
      }

      register_enum_member(data);
      buffer++;
      continue;
    }
    if (*buffer == ' ') {
      buffer++;
      continue;
    }
    if (*buffer == '=') {
      if (data->found_equal == 1) {
        fprintf(stderr, "    /!\\ Malformed enum member at %s:%lld\n", path, status->line_count);
        if (data->members_count > 0) free(data->members);
        free(data);
        return 0;
      }
      data->found_equal = 1;
      buffer++;
      continue;
    }
    if (data->found_equal) data->value_buffer[data->value_buffer_index++] = *buffer;
    else data->member_name_buffer[data->member_name_index++] = *buffer;
    buffer++;
  }
  // Same, if we are here that means we triggered nothing, so continue searching
  return 0;
}
#undef FREE_DATA

#define GET_TCOUNT(header, code) (header ? (code->definitions.typedefs.count) : (code->implementations.typedefs.count))
#define FREE_ALL(data, status, td) {\
  if (data->buffer != NULL) free(data->buffer);\
  if (td != NULL && td->ref != NULL) free(td->ref);\
  free(data);\
  status->data = NULL;\
  status->reading_multiline_typedef = 0;\
}
#define FREE_IF_NEEDED(status, args, td) {\
  if (status->is_header) args->source_code->definitions.typedefs.count--;\
  else args->source_code->implementations.typedefs.count--;\
  if (GET_TCOUNT(status->is_header, args->source_code) == 1) free(td);\
}

static char * obtain_last_word_from_end(char * end) {
  while (*end != ' ') end--;
  char * word = end + 1;

  while (*end == ' ') end--;
  *(end + 1) = 0;
  *(word - 1) = 0;

  if (*word == ')' || *word == '}') return word + 1;
  return word;
}
#define GET_TDCOUNT(header, code) ((header) ? (code->definitions.typedefs.count) : (code->implementations.typedefs.count))
static int register_typedef(const char * path, callback_args args, struct status_s * status) {
  struct typedef_multiline_status_s * data = status->data;

  struct typedef_spec * td = NULL;
  if (status->is_header) {
    if (args->source_code->definitions.typedefs.count == 0) {
      if ((args->source_code->definitions.typedefs.array = malloc(sizeof(struct typedef_spec))) == NULL) {
        perror("malloc typedefs array");
        FREE_ALL(data, status, td);
        return 1;
      }
    } else {
      if ((args->source_code->definitions.typedefs.array = realloc(args->source_code->definitions.typedefs.array, sizeof(struct typedef_spec) * (args->source_code->definitions.typedefs.count + 1))) == NULL) {
        perror("realloc typedef array");
        FREE_ALL(data, status, td);
        return 1;
      }
    }

    td = args->source_code->definitions.typedefs.array + args->source_code->definitions.typedefs.count;
    args->source_code->definitions.typedefs.count++;
  } else {
    if (args->source_code->implementations.typedefs.count == 0) {
      if ((args->source_code->implementations.typedefs.array = malloc(sizeof(struct typedef_spec))) == NULL) {
        perror("malloc typedefs array");
        FREE_ALL(data, status, td);
        args->results.fails++;
        return 1;
      }
    } else {
      if ((args->source_code->implementations.typedefs.array = realloc(args->source_code->implementations.typedefs.array, sizeof(struct typedef_spec) * (args->source_code->implementations.typedefs.count + 1))) == NULL) {
        perror("realloc typedef array");
        FREE_ALL(data, status, td);
        args->results.fails++;
        return 1;
      }
    }

    td = args->source_code->implementations.typedefs.array + args->source_code->implementations.typedefs.count;
    args->source_code->implementations.typedefs.count++;
  }

  if ((td->ref = malloc(sizeof(struct reference))) == NULL) {
    perror("malloc typedef_spec->ref");
    FREE_ALL(data, status, td);
    FREE_IF_NEEDED(status, args, td);
    args->results.fails++;
    return 1;
  }
  td->definition.ptr = NULL;
  td->definition.type = NoneType;
  td->definition.pointers_level = 0;
  td->definition.arrays.count = 0;
  td->definition.arrays.array = NULL;
  td->definition.string[0] = 0;
  // Before anything : check if it is a function typedef. If so, get the name and keep it, and keep the entire function definition (including the name) so we can later on link it
  int encountered_closing_parenthesis = 0;
  char * cp = data->buffer;
  while (*cp != 0 && *cp != ';') {
    if (encountered_closing_parenthesis) {
      if (*cp == ' ' || *cp == '\n') { // Just regular characters that could be between the final parenthesis and the end
        cp++;
        continue;
      }
      encountered_closing_parenthesis = 0; // Otherwise we reached something else : a letter, a pointer, anything, but that's not the end
    } else if (*cp == ')') encountered_closing_parenthesis = 1;
    cp++;
  }
  if (encountered_closing_parenthesis) { // This is unfortunately a function
                                         // To locate the actual name, we will count until we have no more parenthesis pair (since it is how itw works)
    int pending_pairs = 0;
    int started_registering = 0;
    int only_spaces = 1;
    char * final_opening_parenthesis = NULL;
    char * start_of_name = NULL;
    while (cp > data->buffer && start_of_name == NULL) {
      if (!started_registering) {
        if (*cp == ')') {
          started_registering = 1;
          pending_pairs = 1;
        }
      } else if (pending_pairs > 0) {
        if (*cp == ')') {
          pending_pairs++;
        } else if (*cp == '(') {
          pending_pairs--;
        }

        if (pending_pairs == 0) final_opening_parenthesis = cp;
      } else {
        if (only_spaces && *cp != ' ') only_spaces = 0;
        else if (!only_spaces && *cp == ' ') start_of_name = cp + 1;
      }
      cp--;
    }
    printf("\n");
    if (start_of_name == NULL) {
      fprintf(stderr, "Failed to parse function name for typedef.\n");
      args->results.fails++;
      if (GET_TDCOUNT(status->is_header, args->source_code) == 1) {
        if (status->is_header) {
          free(args->source_code->definitions.typedefs.array);
          args->source_code->definitions.typedefs.array = NULL;
        } else {
          free(args->source_code->implementations.typedefs.array);
          args->source_code->implementations.typedefs.array = NULL;
        }
      }
      if (status->is_header) args->source_code->definitions.typedefs.count--;
      else args->source_code->implementations.typedefs.count--;
      FREE_ALL(data, status, td);
      return 1;
    }
    *final_opening_parenthesis = 0;
    STRCP(start_of_name, td->name, ENTITIES_NAME_LENGTH, 1);
    *final_opening_parenthesis = '(';
    td->is_function = 1;
  } else {
    td->is_function = 0;
    char * name = obtain_last_word_from_end(data->buffer + data->buffer_index); // In that case this is fine
    STRCP(name, td->name, ENTITIES_NAME_LENGTH, 1);
  }
  td->buffer = data->buffer; // Only store the found typedef, we cannot really compute it now, we need to know every other type
  td->ref->line = data->starts_on_line;
  td->modifiers = data->modifiers;
  STRCP((char *)path, td->ref->filename, PATH_MAX, 1);

  if (args->config->verbose_registration) printf("- Registering typedef %s\n", td->name);

  free(data);
  return 0;
}
#undef GET_TDCOUNT
#undef FREE_IF_NEEDED
#undef FREE_ALL
#undef GET_TCOUNT

int try_extract_typedef(char * buffer, const char * path, callback_args params, struct status_s * status) {
  SKIP_VOID(buffer);
  if (*buffer == '#') return 1;

  fnt_modifier modifs = 0;
  if (starts_with(buffer, "volatile")) {
    modifs |= F_MVOLATI;
    SKIP_BOTH(buffer);
  }

  if (!starts_with(buffer, "typedef")) return 1;
  struct typedef_multiline_status_s * data;
  if ((data = malloc(sizeof(struct typedef_multiline_status_s))) == NULL) {
    perror("malloc typedef data");
    params->results.fails++;
    return 1;
  }
  if ((data->buffer = malloc(sizeof(char) * 256)) == NULL) {
    perror("malloc buffer");
    params->results.fails++;
    free(data);
    return 1;
  }
  data->modifiers = modifs;
  data->buffer_size = 256;
  data->buffer_index = 0;
  data->starts_on_line = status->line_count;

  while (*buffer != 0 && *buffer != '\n') {
    if (*buffer == ';') { // We found the end of the typedef
      status->data = data;
      data->buffer[data->buffer_index++] = 0;
      return register_typedef(path, params, status);
    }

    if (*buffer != ' ' || data->buffer_index == 0 || data->buffer[data->buffer_index - 1] != ' ') { // Complex condition, but it is just to not copy 2 consecutive spaces
      if (data->buffer_index == data->buffer_size - 2) { // Keep one for 0
        data->buffer_size *= 2;
        if ((data->buffer = realloc(data->buffer, data->buffer_size)) == NULL) {
          perror("realloc typedef data->buffer");
          free(data->buffer);
          free(data);
          params->results.fails++;
          return 1;
        }
      }
      data->buffer[data->buffer_index++] = *buffer;
    }
    buffer++;
  }
  if (*buffer == '\n') data->buffer[data->buffer_index++] = ' ';
  // If we are here we didn't trigger the end, so just continue on the next lines
  status->reading_multiline_typedef = 1;
  status->data = data;
  return 0;
}
int try_extract_typedef_multiline(char * buffer, const char * path, callback_args params, struct status_s * status) {
  struct typedef_multiline_status_s * data = status->data;

  while (*buffer != 0 && *buffer != '\n') {
    if (*buffer == ';') { // We found the end of the typedef
      data->buffer[data->buffer_index++] = 0;
      status->reading_multiline_typedef = 0;
      return register_typedef(path, params, status);
    }

    if (*buffer != ' ' || data->buffer_index == 0 || data->buffer[data->buffer_index - 1] == ' ') { // Complex condition, but it is just to not copy 2 consecutive spaces
      if (data->buffer_index == data->buffer_size - 2) { // Keep one for 0
        data->buffer_size *= 2;
        if ((data->buffer = realloc(data->buffer, data->buffer_size)) == NULL) {
          perror("realloc typedef data->buffer");
          free(data->buffer);
          free(data);
          params->results.fails++;
          return 1;
        }
      }
      data->buffer[data->buffer_index++] = *buffer;
    }
    buffer++;
  }
  if (*buffer == '\n') data->buffer[data->buffer_index++] = ' ';
  return 0;
}

#undef SKIP_WORD
#undef SKIP_BOTH
