#include "core.h"
#include "../parsers/stringlib.h"
#include "../macros.h"
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IS_STRING_DEL(origin, cptr) (*(cptr) == '"' && (cptr == origin || *(cptr - 1) != '\\'))
#define IS_CHAR_DEL(origin, cptr) (*(cptr) == '\'' && (cptr == origin || *(cptr - 1) != '\\'))

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
    STRCP(var_name, var->name, ENTITIES_NAME_LENGTH, 1);
    STRCP(input, var->type->string, ENTITIES_NAME_LENGTH, 1);

    params->source_code->definitions.variables.count++;
  }
  free(input);
  return 0;
}

#define GET_MCOUNT(header, code) ((header) ? (code->definitions.macros.count) : (code->implementations.macros.count))
static int register_macro(callback_args params, struct status_s * status) {
  struct macro_multiline_status_s * data = (status->data);
  data->content[data->content_index] = 0;

  printf("Registering macro : %s with : %s\n", data->name, data->content);
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
    if (GET_MCOUNT(status->is_header, params->source_code)) free(macro);
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
  rebase_path(data->filenameptr, params->config->work, params->config->src, macro->ref->filename, PATH_MAX);

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
  data->filenameptr = path;

  char * parenthesis = NULL;
  char * word = buffer;
  while (*buffer != ' ' && *buffer != '\n') {
    if (*buffer == '(') {
      ((struct macro_multiline_status_s *)status->data)->is_function = 1;
      *buffer = 0;
      parenthesis = buffer;
    }

    buffer++;
  }
  if (*buffer == '\n') {
    fprintf(stderr, "    /!\\ Incomplete macro definition in %s:%lld\n", path, status->line_count);
    free(status->data);
    status->data = NULL;
    return 1;
  }

  if (parenthesis != NULL) *parenthesis = 0;
  *buffer = 0;
  STRCP(word, ((struct macro_multiline_status_s *)status->data)->name, ENTITIES_NAME_LENGTH, 1);
  *buffer = ' ';
  if (parenthesis != NULL) *parenthesis = '(';

  if (((struct macro_multiline_status_s *)status->data)->is_function) {
    *parenthesis = '(';

    // Read arguments for macro
    char * i = parenthesis + 1;
    char * word = parenthesis + 1;

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
        if (((struct macro_multiline_status_s *)status->data)->params_count == 0) {
          if ((((struct macro_multiline_status_s *)status->data)->array = malloc(sizeof(char *))) == NULL) {
            perror("malloc");
            free(status->data);
            status->data = NULL;
            return 1;
          }
        } else {
          if ((((struct macro_multiline_status_s *)status->data)->array = realloc(((struct macro_multiline_status_s *)status->data)->array, sizeof(char *) * (((struct macro_multiline_status_s *)status->data)->params_count + 1))) == NULL) {
            perror("realloc");
            free(((struct macro_multiline_status_s *)status->data)->array);
            free(status->data);
            status->data = NULL;
            return 1;
          }
        }

        if ((((struct macro_multiline_status_s *)status->data)->array[((struct macro_multiline_status_s *)status->data)->params_count] = strdup(word)) == NULL) {
          perror("strdup");
          free(((struct macro_multiline_status_s *)status->data)->array);
          free(status->data);
          status->data = NULL;
          return 1;
        };
        ((struct macro_multiline_status_s *)status->data)->params_count++;
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
    *buffer = ' ';
  }

  while (*buffer == ' ' && *buffer != '\n') buffer++;
  if (*buffer == '\n') {
    if (((struct macro_multiline_status_s *)status->data)->array != NULL) free(((struct macro_multiline_status_s *)status->data)->array);
    fprintf(stderr, "    /!\\ Incomplete macro definition in %s:%lld\n", path, status->line_count);
    free(status->data);
    status->data = NULL;
    return 1;
  }

  char * origin = buffer;
  int * in_string = &(((struct macro_multiline_status_s *)status->data)->in_string);
  int * in_char = &(((struct macro_multiline_status_s *)status->data)->in_char);
  int dontregister = 0;
  // Start recording the following as macro placeholder
  while (*buffer != '\n') { // A macro ends at the end of the line anyway, if we don't have an end of line, screw it, who writes macro over 2048 caracters anyway ?
    if (IS_STRING_DEL(origin, buffer) && !(*in_char)) *in_string = !(*in_string);
    if (IS_CHAR_DEL(origin, buffer) && !(*in_string)) *in_char = !(*in_char);

    if (*buffer == '\\' && (*(buffer + 1) == '\n' || *(buffer + 1) == ' ') && !(*in_string) && !(*in_char)) { // We have a \ that is used, not as an escape char but as a line break
      status->reading_multiline_macro = 1;
      dontregister = 1;
    }

    if (!dontregister) {
      ((struct macro_multiline_status_s *)status->data)->content[((struct macro_multiline_status_s *)status->data)->content_index] = *buffer;
      ((struct macro_multiline_status_s *)status->data)->content_index++;
    } else dontregister = 0;
    buffer++;
  }

  if (status->reading_multiline_macro == 0) {
    if (register_macro(params, status) != 0) {
      fprintf(stderr, "Unable to register macro for file %s at line %lld\n", path, status->line_count);
      free(status->data);
      status->data = NULL;
      return 1;
    }
  }

  return 0;
}

int try_extract_macro_multiline(char * buffer, callback_args params, struct status_s * status) {
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

  if (status->reading_multiline_macro == 0) return register_macro(params, status);
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
    if (GET_COUNT(status->is_header, params->source_code) == 0) free(structure);
    params->results.fails++;
    return 0;
  }
  if ((structure->members = malloc(sizeof(struct struct_members))) == NULL) {
    perror("malloc struct_spec->members");
    free(structure->ref);
    if (GET_COUNT(status->is_header, params->source_code) == 0) free(structure);
    params->results.fails++;
    return 0;
  }
  structure->members->array = NULL;
  if (data->members_count > 0) {
    if ((structure->members->array = malloc(sizeof(struct struct_member) * data->members_count)) == NULL) {
      perror("malloc struct_spec->members->array");
      free(structure->members);
      free(structure->ref);
      if (GET_COUNT(status->is_header, params->source_code) == 0) free(structure);
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
        if (GET_COUNT(status->is_header, params->source_code) == 0) free(structure);
        if (free_type && typebuf != NULL) free(typebuf);
        return 0;
      }
      STRCP(namebuf, structure->members->array[i].name, ENTITIES_NAME_LENGTH, 1);
      STRCP(typebuf, structure->members->array[i].type.string, ENTITIES_NAME_LENGTH, 1);
      structure->members->array[i].type.type = NoneType;
      structure->members->array[i].type.ptr = NULL;
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
static void register_function(const char * path, callback_args params, struct status_s * status) {
  struct function_multiline_status_s * data = status->data;

  struct function_spec * function;
  if (status->is_header) {
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
    if (GET_FCOUNT(status->is_header, params->source_code) == 0) free(function);
    FREE_DATA(status, data);
    return;
  }
  if ((function->args = malloc(sizeof(struct function_args))) == NULL) {
    perror("malloc function->args");
    free(function->ref);
    if (GET_FCOUNT(status->is_header, params->source_code) == 0) free(function);
    FREE_DATA(status, data);
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
      if (GET_FCOUNT(status->is_header, params->source_code) == 0) free(function);
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
        if (GET_FCOUNT(status->is_header, params->source_code) == 0) free(function);
        FREE_DATA(status, data);
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
complete_fail:
          free(input);
          goto almost_complete_fail;
        }
        j++;
      }
      if (var_name == input) {
        fprintf(stderr, "    /!\\ Incomplete variable definition at %s:%lld (no type)\n", path, status->line_count);
        goto complete_fail;
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
      STRCP(var_name, function->args->array[i].name, ENTITIES_NAME_LENGTH, 1);
      STRCP(input, function->args->array[i].type.string, ENTITIES_NAME_LENGTH, 1);
      function->args->array[i].type.type = NoneType;
      function->args->array[i].type.ptr = NULL;

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

  FREE_DATA(status, data);

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

  struct function_multiline_status_s * data;
  if ((data = malloc(sizeof(struct function_multiline_status_s))) == NULL) {
    perror("malloc function status");
    args->results.fails++;
    return 1;
  }
  status->data = data;
  data->modifiers = 0;
  data->reading_arg = 0;
  data->reading_args = 0;
  data->args_count = 0;
  data->args = NULL;
  data->returntype.type = NoneType;
  data->returntype.ptr = NULL;
  data->returntype.string[0] = 0;
  data->name[0] = 0;
  data->starts_on_line = status->line_count;
  data->filenameptr = path;
  data->words_counted = 0;
  data->is_declaration = 0;

  char * start = buffer;
  char * name = buffer;
  char * arg_start = buffer;
  char * arg_check = NULL;
  char * typestart = buffer;
  while (*buffer != 0 && *buffer != '\n') {
    if (!data->reading_args) {
      if (start == buffer || (*buffer - 1) == ' ') {
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
      }
    }

    buffer++;
  }

  if (data->name[0] == 0 || data->returntype.string[0] == 0) { // This was not a function after all
    goto cleanup_fail;
  }
  if (*buffer != 0 && !data->reading_args) {
    while (*buffer != '{' && *buffer != ';' && *buffer != 0) buffer++;
    data->is_declaration = *buffer == ';'; // Consider everything else an implementation really
  }
  if (data->is_declaration && !status->is_header) {
    FREE_DATA(status, data);
    return 0; // We matched something, so skip in the main loop
  }
  register_function(path, args, status);
  return 0;

  int returncode;
cleanup_fail:
  returncode = 1;
  goto cleanup;
cleanup:
  FREE_DATA(status, data);
  return returncode;
}

#undef SKIP_WORD
#undef FREE_DATA
