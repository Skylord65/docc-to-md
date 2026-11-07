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
  if (streq("extern", first_word) || streq("static", first_word) || streq("volatine", first_word)) {
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
int normalize_type_string(const char * type, char * typebuf) {
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
int try_extract_variable(char * buffer, const char * path, callback_args params, struct status_s * status) {
  if (*buffer == '#') return 1;

  char * ptr = (char *)buffer;
  char * word_before_equal = buffer;
  char * type = buffer;
  char * equals = NULL;
  char typebuf[ENTITIES_NAME_LENGTH];

  while (*ptr != '\n' && (*ptr != '=' && *ptr != ';')) ptr++;
  if (*ptr != '=' && *ptr != ';') return 1; // In case we have something like : unsigned long int i;, treat ';' as '=' (does the  same thing)
  equals = ptr;

  if (*(ptr - 1) == ' ') {
    ptr--;
    while (*ptr == ' ' && ptr != buffer) ptr--;
  } else ptr--;
  if (ptr == buffer) {
    fprintf(stderr, "    /!\\ Incomplete variable at %s:%lld\n", path, status->line_count);
    return 1;
  }

  if (*ptr == ')' || *ptr == '}') return 1; // This is the end of a function/structure/typedef
  // Find the first word backwards
  while (*ptr != ' ' && ptr != buffer) {
    if (*ptr == '(' || *ptr == '{') return 1; // This is the start of a function/structure/typedef in the name
    ptr--;
  }
  if (ptr == buffer) {
    fprintf(stderr, "    /!\\ Incomplete variable at %s:%lld\n", path, status->line_count);
    return 1;
  }
  word_before_equal = ptr + 1;

  while (*ptr == ' ' && ptr != buffer) ptr--;
  if (ptr == buffer) {
    fprintf(stderr, "    /!\\ Incomplete variable at %s:%lld\n", path, status->line_count);
    return 1;
  }

  *(ptr + 1) = 0;
  *equals = 0;

  if (check_variable_type_name_against_keywords(type) == 1) return 1;
  normalize_type_string(type, typebuf);

  printf("Variable type is : %s and variable name is %s\n", typebuf, word_before_equal);

  if (status->is_header) {
    if (params->source_code->definitions.variables.count == 0) {
      if ((params->source_code->definitions.variables.array = malloc(sizeof(struct variable))) == NULL) {
        perror("malloc");
        params->results.fails++;
        return 1;
      }
    } else if ((params->source_code->definitions.variables.array = realloc(params->source_code->definitions.variables.array, sizeof(struct variable) * (params->source_code->definitions.variables.count + 1))) == NULL) {
      perror("realloc");
      params->results.fails++;
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
    }
    if ((var->type = malloc(sizeof(entity_type_t))) == NULL) {
      perror("malloc var->type");
      free(var->ref);
      free(var);
      return 0;
    }
    var->ref->line = status->line_count;
    rebase_path(path, params->config->work, params->config->src, var->ref->filename, PATH_MAX);
    STRCP(word_before_equal, var->name, ENTITIES_NAME_LENGTH, 1);
    STRCP(typebuf, var->type->string, ENTITIES_NAME_LENGTH, 1);

    params->source_code->definitions.variables.count++;
  }
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
  // Start recording the following as macro placeholder
  while (*buffer != '\n') { // A macro ends at the end of the line anyway, if we don't have an end of line, screw it, who writes macro over 2048 caracters anyway ?
    if (IS_STRING_DEL(origin, buffer) && !(*in_char)) *in_string = !(*in_string);
    if (IS_CHAR_DEL(origin, buffer) && !(*in_string)) *in_char = !(*in_char);

    if (*buffer == '\\' && (*(buffer + 1) == '\n' || *(buffer + 1) == ' ') && !(*in_string) && !(*in_char)) { // We have a \ that is used, not as an escape char but as a line break
      status->reading_multiline_macro = 1;
    }

    ((struct macro_multiline_status_s *)status->data)->content[((struct macro_multiline_status_s *)status->data)->content_index] = *buffer;
    ((struct macro_multiline_status_s *)status->data)->content_index++;
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

int try_extract_macro_multiline(char * buffer, const char * path, callback_args params, struct status_s * status) {
  // This is pretty  straightforward : Just copy until we reach either end of line, or line break (\) followed by some spaces/end of lines
  struct macro_multiline_status_s * data = status->data;
  const char * origin = buffer;

  // If we made it here, that's because there is a newline, so, logically, add it
  data->content[(data->content_index)++] = '\n';
  status->reading_multiline_macro = 0; // Cancel, unless we reach later a new line to manage
  while (*buffer != '\n') {
    if (IS_STRING_DEL(origin, buffer) && !data->in_char) data->in_string = !data->in_string;
    if (IS_CHAR_DEL(origin, buffer) && !data->in_string) data->in_char = !data->in_char;
    if (*buffer == '\\' && (*(buffer + 1) == '\n' || *(buffer + 1) == ' ') && !data->in_string && !data->in_char) status->reading_multiline_macro = 1;

    data->content[data->content_index] = *buffer;
    data->content_index++;
    buffer++;
  }

  if (status->reading_multiline_macro == 0) return register_macro(params, status);
  return 0;
}

static void compute_struct_member(char * value, char ** type, char ** name) {
  char * last_space = NULL;
  char * first_space = NULL;
  *type = value;
  *name = NULL;

  while (*value != 0) {
    if (*value == ' ') {
      first_space = value;
      while (*value == ' ') value++;
      *name = value;
      last_space = value - 1;
    }
    value++;
  }

  *last_space = 0;
  *first_space = 0;
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
      normalize_type_string(data->members_array[i], srcbuf);

      char * typebuf = NULL;
      char * namebuf = NULL;

      compute_struct_member(srcbuf, &typebuf, &namebuf);

      STRCP(namebuf, structure->members->array[i].name, ENTITIES_NAME_LENGTH, 1);
      STRCP(typebuf, structure->members->array[i].type.string, ENTITIES_NAME_LENGTH, 1);
      structure->members->array[i].type.type = NoneType;
      structure->members->array[i].type.ptr = NULL;
      free(data->members_array[i]);
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
