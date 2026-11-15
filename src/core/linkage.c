#include "core.h"
#include "../reader/reader.h"
#include "../macros.h"
#include "../parsers/stringlib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define GOTO_NEXTWORD(buffer) {\
  while (*(buffer) != ' ') buffer++;\
  while (*(buffer) == ' ') buffer++;\
}

struct register_primitive_data {
  char buffer[ENTITIES_NAME_LENGTH];
  unsigned int buffer_index;
  PrimitiveSource source;
  char is_constant;
  char is_structure;
};
/**
 * This function takes a pointer to a supposed type string where it only remains pointers and arrays to register, and modifies 'type'
 *
 * return 0 if everything alright, 1 otherwise
 */
static int compute_rest_of_type(char * input, entity_type_t * type) {
  char buf[ENTITIES_NAME_LENGTH];
  int bufindex = 0;
  int writeinbuf = 0;

  while (*input != 0) {
    if (*input == '*') type->pointers_level++;
    else if (!writeinbuf) {
      if (*input == '[') writeinbuf = 1;
    } else {
      if (*input == ']') {
        if ((type->arrays.array = realloc(type->arrays.array, sizeof(char *) * (type->arrays.count + 1))) == NULL) {
          perror("realloc compute_rest_of_type");
          return 1;
        }
        if (bufindex == 0) type->arrays.array[type->arrays.count] = NULL;
        else {
          buf[bufindex++] = 0;
          if ((type->arrays.array[type->arrays.count] = strdup(buf)) == NULL) {
            perror("strdup compute_rest_of_type");
            return 1;
          }
        }
        type->arrays.count++;
        bufindex = 0;
        writeinbuf = 0;
      } else buf[bufindex++] = *input;
    }
    input++;
  }
  return 0;
}
static int register_primitive(source_code_t code, struct register_primitive_data * data) {
  if ((code->primitives.array = realloc(code->primitives.array, sizeof(struct primitive_spec) * (code->primitives.count + 1))) == NULL) {
    perror("realloc primitive array");
    return 1;
  }
  struct primitive_spec * spec = code->primitives.array + code->primitives.count;
  code->primitives.count++;

  spec->source = data->source;
  spec->is_structure = data->is_structure;
  STRCP(data->buffer, spec->name, ENTITIES_NAME_LENGTH, 1);

  return 0;
}
struct keyword_element {
  char name[ENTITIES_NAME_LENGTH];
  PrimitiveSource source;
};
static int word_eq(const char * sentence, const char * word) {
  while (*sentence != 0 && *word != 0) {
    if (*sentence != *word) return 0;
    sentence++;
    word++;
  }
  return *word == 0 && (*sentence == ' ' || *sentence == 0);
}
static struct enum_spec * try_parse_enumerator(char * input, source_code_t code, char ** restptr) {
  if (starts_with(input, "extern") || starts_with(input, "const")) GOTO_NEXTWORD(input);
  if (starts_with(input, "extern") || starts_with(input, "const")) GOTO_NEXTWORD(input); // Twice because we would have extern const

  if (!starts_with(input, "enum")) return NULL;
  GOTO_NEXTWORD(input);

  char * end = input;
  while (*end != ' ' && *end != 0) end++;
  int changed = *end == ' ';
  *end = 0;
  *restptr = end;

  ulint i = 0;
  while (i < code->definitions.enums.count) {
    if (streq(code->definitions.enums.array[i].name, input)) {
      if (changed) *end = ' ';
      return code->definitions.enums.array + i;
    }
    i++;
  }
  i = 0;
  while (i < code->implementations.structs.count) {
    if (streq(code->implementations.structs.array[i].name, input)) {
      if (changed) *end = ' ';
      return code->implementations.enums.array + i;
    }
    i++;
  }
  if (changed) *end = ' ';

  return NULL;
}
static struct struct_spec * try_parse_structure(char * input, source_code_t code, char ** restptr) {
  if (starts_with(input, "extern") || starts_with(input, "const")) GOTO_NEXTWORD(input);
  if (starts_with(input, "extern") || starts_with(input, "const")) GOTO_NEXTWORD(input); // Twice because we would have extern const

  if (!starts_with(input, "struct")) return NULL;
  GOTO_NEXTWORD(input);

  char * end = input;
  while (*end != ' ' && *end != 0) end++;
  int changed = *end == ' ';
  *end = 0;
  *restptr = end;

  ulint i = 0;
  while (i < code->definitions.structs.count) {
    if (streq(code->definitions.structs.array[i].name, input)) {
      if (changed) *end = ' ';
      return code->definitions.structs.array + i;
    }
    i++;
  }
  i = 0;
  while (i < code->implementations.structs.count) {
    if (streq(code->implementations.structs.array[i].name, input)) {
      if (changed) *end = ' ';
      return code->implementations.structs.array + i;
    }
    i++;
  }
  if (changed) *end = ' ';

  return NULL;
}
static struct typedef_spec * identify_typedef(char * input, source_code_t code, char ** restptr) {
  if (starts_with(input, "extern") || starts_with(input, "const")) GOTO_NEXTWORD(input);
  if (starts_with(input, "extern") || starts_with(input, "const")) GOTO_NEXTWORD(input); // Twice because we would have extern const

  char * end = input;
  while (*end != ' ' && *end != 0) end++;
  int changed = *end == ' ';
  *end = 0;
  *restptr = end;
  ulint i = 0;
  while (i < code->definitions.typedefs.count) {
    if (streq(code->definitions.typedefs.array[i].name, input)) {
      if (changed) *end = ' ';
      return code->definitions.typedefs.array + i;
    }
    i++;
  }
  i = 0;
  while (i < code->implementations.typedefs.count) {
    if (streq(code->implementations.typedefs.array[i].name, input)) {
      if (changed) *end = ' ';
      return code->implementations.typedefs.array + i;
    }
    i++;
  }
  if (changed) *end = ' ';
  return NULL;
}
static struct primitive_spec * try_parse_primitive_structure(char * input, source_code_t code, char ** restptr) {
  if (starts_with(input, "extern") || starts_with(input, "const")) GOTO_NEXTWORD(input);
  if (starts_with(input, "extern") || starts_with(input, "const")) GOTO_NEXTWORD(input); // Twice because we would have extern const

  if (!starts_with(input, "struct")) return NULL;
  GOTO_NEXTWORD(input);

  char * end = input;
  while (*end != ' ' && *end != 0) end++;
  int changed = *end == ' ';
  *end = 0;
  *restptr = end;

  ulint i = 0;
  struct keyword_element predefined[] = {
    { "random_data", Stdlib }, { "drand48_data", Stdlib },
    { "dirent", Dirent }, { "dirent64", Dirent },
    { "tm", Stdtime }, { "sigevent", Stdtime },
    { "timespec", Stdtime }, { "itimerspec", Stdtime },
    { "stat", SysStat }, { "Stringpgrep_table_element", LibStringpGrep },
    { "Stringpgrep_table", LibStringpGrep }, { "Stringpgrep_profiles", LibStringpGrep }
  };
  ulint size = sizeof(predefined) / sizeof(struct keyword_element);

  struct keyword_element * element = NULL;
  while (i < size && element == NULL) {
    if (word_eq(input, predefined[i].name)) element = predefined + i;
    i++;
  }
  if (element == NULL) {
    if (changed) *end = ' ';
    return NULL;
  }

  // Look if the found structure exists in the registered primitives
  ulint j = 0;
  struct primitive_spec * matched = NULL;
  while (j < code->primitives.count && matched == NULL) {
    struct primitive_spec * spec = code->primitives.array + j;
    if (spec->is_structure == 1 && streq(spec->name, element->name)) matched = code->primitives.array + j;
    j++;
  }

  if (matched == NULL) { // This doesn't exist, thus we will create it and append it to the array
    if ((code->primitives.array = realloc(code->primitives.array, (code->primitives.count + 1) * sizeof(struct primitive_spec))) == NULL) {
      perror("realloc code->primitives for structure");
      if (changed) *end = ' ';
      return NULL;
    }

    matched = code->primitives.array + code->primitives.count;
    code->primitives.count++;

    matched->is_structure = 1;
    matched->source = element->source;
    STRCP(element->name, matched->name, ENTITIES_NAME_LENGTH, 1);
  }
  if (changed) *end = ' ';
  return matched;
}
/**
 * Returns 0 if a type has been successfully registered, 1 otherwise
 *
 * It DOES NOT call register_primitive
 */
static int try_parse_primitive(char * input, struct register_primitive_data * data, char ** restptr) {
  if (starts_with(input, "const")) {
    data->is_constant = 1;
    GOTO_NEXTWORD(input);
  } else data->is_constant = 0;

  if (starts_with(input, "struct") || starts_with(input, "enum")) return 1;
  // This for long ass list of words
  struct keyword_element fatass_list[] = {
    { "signed", GlibC }, { "unsigned", GlibC },
    { "int", GlibC }, { "float", GlibC },
    { "double", GlibC }, { "long", GlibC },
    { "short", GlibC }, { "char", GlibC },
    { "void", GlibC }, { "FILE", Stdio },
    { "va_list", Stdio }, { "off_t", Stdio },
    { "off64_t", Stdio }, { "fpos_t", Stdio },
    { "fpos64_t", Stdio }, { "size_t", Stddef },
    { "ssize_t", SysTypes }, { "bool", Stdbool },
    { "tm", Stdtime }, { "time_t", Stdtime },
    { "clock_t", Stdtime }, { "div_t", Stdlib },
    { "ldiv_t", Stdlib }, { "lldiv_t", Stdlib },
    { "int_fast8_t", Stdint }, { "int_fast16_t", Stdint },
    { "int_fast32_t", Stdint }, { "int_fast64_t", Stdint },
    { "uint_fast8_t", Stdint }, { "uint_fast16_t", Stdint },
    { "uint_fast32_t", Stdint }, { "uint_fast64_t", Stdint },
    { "intptr_t", Stdint }, { "uintptr_t", Stdint },
    { "intmax_t", Stdint }, { "uintmax_t", Stdint },
    { "u_char", SysTypes }, { "u_short", SysTypes },
    { "u_int", SysTypes }, { "u_long", SysTypes },
    { "quad_t", SysTypes }, { "u_quad_t", SysTypes },
    { "fsid_t", SysTypes }, { "loff_t", SysTypes },
    { "ino_t", SysTypes }, { "ino64_t", SysTypes },
    { "dev_t", SysTypes }, { "gid_t", SysTypes },
    { "mode_t", SysTypes }, { "nlink_t", SysTypes },
    { "uid_t", SysTypes }, { "off_t", SysTypes },
    { "off64_t", SysTypes }, { "pid_t", SysTypes },
    { "id_t", SysTypes }, { "daddr_t", SysTypes },
    { "caddr_t", SysTypes }, { "key_t", SysTypes },
    { "useconds_t", SysTypes }, { "suseconds_t", SysTypes },
    { "ulong", SysTypes }, { "ushort", SysTypes },
    { "u_int", SysTypes }, { "u_int8_t", SysTypes },
    { "u_int16_t", SysTypes }, { "u_int32_t", SysTypes },
    { "u_int64_t", SysTypes }, { "register_t", SysTypes },
    { "blksize_t", SysTypes }, { "blkcnt_t", SysTypes },
    { "fsblkcnt_t", SysTypes }, { "fsfilcnt_t", SysTypes },
    { "blkcnt64_t", SysTypes }, { "fsblkcnt64_t", SysTypes },
    { "fsfilcnt64_t", SysTypes }, { "Stringpgrep_rc", LibStringpGrep },
    { "Stringpgrep_profile_flags", LibStringpGrep }, { "Stringpgrep_profile_steps", LibStringpGrep },
    { "Stringpgrep_table_element", LibStringpGrep }, { "Stringpgrep_profile", LibStringpGrep },
    { "Stringpgrep_profiles", LibStringpGrep }, { "DIR", Dirent },
  };
  char * cp = input;
  unsigned int size = sizeof(fatass_list) / sizeof(struct keyword_element);
  while (*input != 0) {
    if (*input == ' ') input++;
    unsigned int j = 0;
    while (j < size) {
      if (word_eq(input, fatass_list[j].name)) {
        data->source |= fatass_list[j].source;
        if (data->buffer_index > 0) data->buffer[data->buffer_index++] = ' ';
        while (*input != ' ' && *input != 0) data->buffer[data->buffer_index++] = *(input++);
        while (*input == ' ') input++;
        break;
      }
      j++;
    }

    if (*input == '*' || *input == '[') {
      *restptr = input - 1;
      break;
    }
    if (*input == 0) break;
    if (cp == input || *(input - 1) != ' ') input++;
  }
  if (*input == 0) *restptr = input;

  data->buffer[data->buffer_index++] = 0;
  return 0;
}
static struct macro_spec * identify_macro(char * input, source_code_t code) {
  ulint i = 0;
  while (i < code->definitions.macros.count) {
    if (word_eq(input, code->definitions.macros.array[i].name)) return code->definitions.macros.array + i;
    i++;
  }
  i = 0;
  while (i < code->implementations.macros.count) {
    if (word_eq(input, code->implementations.macros.array[i].name)) return code->implementations.macros.array + i;
    i++;
  }
  return NULL;
}
static void init_primitive_data(struct register_primitive_data * p) {
  p->buffer_index = 0;
  p->source = 0;
  p->is_constant = 0;
  p->is_structure = 0;
  p->buffer[0] = 0;
}
static int assign_type(source_code_t code, char * input, entity_type_t * type) {
  struct register_primitive_data data;

  init_primitive_data(&data);

  char * input_rest = NULL;
  void * res;
  if ((res = identify_macro(input, code)) != NULL) {
    if (type != NULL) {
      type->ptr = res;
      type->type = MacroType;
    }
  } else if ((res = try_parse_structure(input, code, &input_rest)) != NULL) {
    if (type != NULL) {
      type->ptr = res;
      type->type = StructureType;
      if (compute_rest_of_type(input_rest, type) != 0) return 1;
    }
    return 0;
  } else if ((res = identify_typedef(input, code, &input_rest)) != NULL) {
    if (type != NULL) {
      type->ptr = res;
      type->type = TypedefType;
      if (compute_rest_of_type(input_rest, type) != 0) return 1;
    }
    return 0;
  } else if ((res = try_parse_primitive_structure(input, code, &input_rest)) != NULL) {
    if (type != NULL) {
      type->ptr = res;
      type->type = PrimitiveType;
      if (compute_rest_of_type(input_rest, type) != 0) return 1;
    }
    return 0;
  } else if ((res = try_parse_enumerator(input, code, &input_rest)) != NULL) {
    if (type != NULL) {
      type->ptr = res;
      type->type = EnumeratorType;
      if (compute_rest_of_type(input_rest, type) != 0) {
        return 1;
      }
    }
    return 0;
  } else if (try_parse_primitive(input, &data, &input_rest) == 0) {
    if (type != NULL && compute_rest_of_type(input_rest, type) != 0) return 1;

    // Find the already registered primitive if it exists
    ulint i = 0;
    while (i < code->primitives.count) {
      struct primitive_spec * spec = code->primitives.array + i;
      if (
          spec->source == data.source &&
          spec->is_structure == data.is_structure && // That sould be set to 0 though
          streq(data.buffer, spec->name)
         ) { // Then they are the same
        if (type != NULL) {
          type->ptr = spec;
          type->type = PrimitiveType;
        }
        return 0;
      }
      i++;
    }

    if (register_primitive(code, &data) != 0) return 1;
    // The added primitive is automatically the last
    if (type != NULL) {
      type->ptr = code->primitives.array + (code->primitives.count - 1);
      type->type = PrimitiveType;
    }
  } else {
    return 1;
  }

  return 0; // Everything went fine
}
static int link_function(struct function_spec * function, source_code_t code, callback_args args) {
  if (assign_type(code, function->returntype.string, &(function->returntype)) != 0) {
    fprintf(stderr, "Cannot assign return type of function %s (return type string is %s)\n", function->name, function->returntype.string);
    args->results.fails++;
    return 1;
  }

  ulint j = 0;
  while (j < function->args->count) {
    struct function_arg * arg = function->args->array + j;
    // Once again, we have edge cases : if the argument is unamed. It can only mean it is a structure or an enum, since we made sure that would be the case, and if that's the case, the type of the argument is struct or enum.
    // The strategy is : check if that's the case, if so, we snprintf it into a buffer and use the buffer instead of the .type.string
    char typebuf[ENTITIES_NAME_LENGTH + 8] = {0};
    char * input = arg->type.string;
    if (streq("struct", arg->type.string)) {
      snprintf(typebuf, ENTITIES_NAME_LENGTH + 8, "struct %s", arg->name); // We get struct nameOfStruct
      input = typebuf;
    } else if (streq("enum", arg->type.string)) {
      snprintf(typebuf, ENTITIES_NAME_LENGTH + 8, "enum %s", arg->name); // We get eunm NameOfEnum
      input = typebuf;
    }
    if (input != arg->type.string) { // Also replace name of argument and type
      STRCP(typebuf, arg->type.string, ENTITIES_NAME_LENGTH, 1);
      snprintf(arg->name, ENTITIES_NAME_LENGTH, "__arg_no_%zu", j + 1);
    }
    if (assign_type(code, input, &(arg->type)) != 0) {
      fprintf(stderr, "Cannot assign type of argument number %zu of function %s (argument is %s, its type is %s)\n", j + 1, function->name, arg->name, arg->type.string);
      args->results.fails++;
      return 1;
    }
    j++;
  }
  args->results.links += 1 + function->args->count;
  return 0;
}
static int link_functions(struct function_indic * functions, source_code_t code, callback_args args) {
  ulint i = 0;
  while (i < functions->count) {
    link_function(functions->array + i, code, args);
    i++;
  }

  return 0;
}
static int link_struct(struct struct_spec * strc, source_code_t code, callback_args args) {
  ulint j = 0;
  while (j < strc->members->count) {
    if (assign_type(code, strc->members->array[j].type.string, &(strc->members->array[j].type)) != 0) {
      fprintf(stderr, "Cannot assign type to member number %zu (%s) of structure %s\n", j + 1, strc->members->array[j].name, strc->name);
      args->results.fails++;
      return 1;
    }
    j++;
  }
  args->results.links += strc->members->count;
  return 0;
}
static int link_structs(struct struct_indic * structs, source_code_t code, callback_args args) {
  ulint i = 0;
  while (i < structs->count) {
    link_struct(structs->array + i, code, args);
    i++;
  }
  return 0;
}
static int link_variables(struct variables_indic * variables, source_code_t code, callback_args args) {
  ulint i = 0;
  while (i < variables->count) {
    struct variable * var = variables->array + i;
    if (assign_type(code, var->type->string, var->type) != 0) {
      fprintf(stderr, "Cannot assign type to variable %s (type : %s)\n", var->name, var->type->string);
      args->results.fails++;
      return 1;
    }
    i++;
  }
  args->results.links += variables->count;
  return 0;
}
/**
 * Here we may want to not throw errors on type not assigned, we will assume that is because it is a typedefined into the typedef, like 
 * typedef enum {
 *  A,
 *  B
 * }; or with structures
 *
 * We want to assign them if the types exists and are like pointers, like :
 * struct filestream;
 * typedef struct filestream * stream_p;
 */
static int link_typedef(struct typedef_spec * td, source_code_t code, callback_args args) {
  struct register_primitive_data data;
  init_primitive_data(&data);

  // Work the buffer to give the correct input to assign_type
  char * bufptr = td->buffer;
  char * input_rest;
  GOTO_NEXTWORD(bufptr); // We have typedef as first word
                         // Find the last space and replace it by a 0
  char * last_space = NULL;
  char * ptrcp = bufptr;
  while (*ptrcp != 0) {
    if (*ptrcp == ' ') last_space = ptrcp;
    ptrcp++;
  }
  if (last_space != NULL) *last_space = 0;
  if (!td->is_function && try_parse_primitive(bufptr, &data, &input_rest) == 0) {
    if (last_space != NULL) *last_space = ' ';
    if (compute_rest_of_type(input_rest, &(td->definition)) != 0) {
      return 1;
    }

    // Find the already registered primitive if it exists
    ulint i = 0;
    while (i < code->primitives.count) {
      struct primitive_spec * spec = code->primitives.array + i;
      if (
          spec->source == data.source &&
          spec->is_structure == data.is_structure && // That sould be set to 0 though
          streq(data.buffer, spec->name)
         ) { // Then they are the same
        td->definition.ptr = spec;
        td->definition.type = PrimitiveType;
        return 0;
      }
      i++;
    }
    return 1;
  }
  if (last_space != NULL) *last_space = ' ';


  if (td->is_function) { // Don't search, just parse it somehow
    struct status_s status = {
      .reading_function_params = 0,
      .data = NULL,
      .automatic = 0
    };
    if (try_extract_function(td->buffer, "", args, &status) == 0 && status.data == NULL) {
      fprintf(stderr, "Cannot parse function type of %s for typedef %s\n", td->buffer, td->name);
      args->results.fails++;
      return 1;
    }

    ((struct function_multiline_status_s *)(status.data))->doubleptr = (struct function_spec **)(&td->definition.ptr);
    size_t before = args->results.fails;
    register_function("", args, &status);
    if (args->results.fails > before) {
      fprintf(stderr, "Failed to register function %s for typedef %s\n", td->buffer, td->name);
      args->results.fails++;
      return 1;
    }

    args->results.links++;
    td->definition.type = TypedefinedFunction;
    return 0;
  }
  // First check if it could be defined here : a keyword followed by something else than {
  char * ptr = td->buffer;
  if (word_eq(ptr, "typedef")) GOTO_NEXTWORD(ptr);
  if (word_eq(ptr, "enum")) {
    if (*ptr != '{') { // Here we go and try
      struct status_s status = {
        .reading_multiline_enum = 0,
        .data = NULL,
        .automatic = 0
      };
      if (try_extract_enum(ptr, "", args, &status) == 0 || status.data == NULL) {
        fprintf(stderr, "Cannot parse enumerator type of %s for typedef %s\n", td->buffer, td->name);
        args->results.fails++;
        return 1;
      }

      char * last_bracket = NULL;
      char * cp = ptr;
      while (*cp != 0) {
        if (*cp == '}') last_bracket = cp;
        cp++;
      }

      ((struct enum_multiline_status_s *)(status.data))->doubleptr = (struct enum_spec **)(&td->definition.ptr);
      size_t before = args->results.fails;
      register_enum("", args, &status);
      if (args->results.fails > before) {
        fprintf(stderr, "Failed to register enum type %s for typedef %s\n", td->buffer, td->name);
        args->results.fails++;
        return 1;
      }

      args->results.links++;
      td->definition.type = TypedefinedEnum;

      *last_bracket = 0;
      compute_rest_of_type(last_bracket + 1, &(td->definition));
      *last_bracket = '}';
      return 0;
    }
  }
  if (word_eq(ptr, "struct")) {
    char * ptrcp = ptr;
    GOTO_NEXTWORD(ptrcp);
    if (*ptrcp == '{') {
      struct status_s status = {
        .reading_multiline_struct = 0,
        .data = NULL,
        .automatic = 0
      };
      // We need to remove trailing newlines
      char * copy;
      if ((copy = malloc(sizeof(char) * (strlen(td->buffer) + 1))) == NULL) {
        perror("malloc working copy of structure");
        args->results.fails++;
        return 1;
      }
      STRCP("struct ", copy, 7, 0);
      ulint i = 0;
      ulint writeindex = 7;
      while (ptrcp[i] != 0) {
        if (ptrcp[i] != '\n') copy[writeindex++] = ptrcp[i];
        else copy[writeindex++] = ' ';
        i++;
      }
      copy[writeindex++] = 0;
      if (try_extract_struct(copy, "", args, &status) != 0 && status.data == NULL) {
        fprintf(stderr, "Cannot parse structure type of %s for typedef %s\n", td->buffer, td->name);
        args->results.fails++;
        free(copy);
        return 1;
      }
      STRCP(td->name, ((struct struct_spec *)status.data)->name, ENTITIES_NAME_LENGTH, 1);

      char * last_bracket = NULL;
      char * cp = ptr;
      while (*cp != 0) {
        if (*cp == '}') last_bracket = cp;
        cp++;
      }

      ((struct struct_multiline_status_s *)(status.data))->doubleptr = (struct struct_spec **)(&td->definition.ptr);
      size_t before = args->results.fails;
      register_structure("", args, &status);
      if (args->results.fails > before) {
        fprintf(stderr, "Failed to register structure type %s for typedef %s\n", td->buffer, td->name);
        args->results.fails++;
        free(copy);
        return 1;
      }
      args->results.links++;
      td->definition.type = TypedefinedStructure;

      *last_bracket = 0;
      compute_rest_of_type(last_bracket + 1, &(td->definition));
      *last_bracket = '}';

      free(copy);
      return 0;
    }
  }
  *last_space = 0;
  if (assign_type(code, ptr, &(td->definition)) == 0) {
    if (last_space != NULL) *last_space = ' ';
    return 0;
  }
  if (last_space != NULL) *last_space = ' ';
  td->definition.type = NoneType;

  return 0;
}
static int link_typedefs(struct typedef_indic * typedefs, source_code_t code, callback_args args) {
  ulint i = 0;
  while (i < typedefs->count) {
    link_typedef(typedefs->array + i, code, args);
    i++;
  }
  return 0;
}
#define CHECK(instruction) {\
  if (instruction != 0) return 1;\
}
static int link_typedefs_result(struct typedef_indic * typedefs, source_code_t code, callback_args args) {
  ulint i = 0;
  while (i < typedefs->count) {
    struct typedef_spec * td = typedefs->array + i;
    if (td->definition.type == TypedefinedStructure) CHECK(link_struct(td->definition.ptr, code, args))
    else if (td->definition.type == TypedefinedFunction) CHECK(link_function(td->definition.ptr, code, args))
    i++;
  }
  return 0;
}
#undef CHECK

static int prepare_function(struct function_spec * function, source_code_t code, callback_args args) {
  if (assign_type(code, function->returntype.string, NULL) != 0) {
    fprintf(stderr, "Cannot assign return type of function %s (return type string is %s)\n", function->name, function->returntype.string);
    args->results.fails++;
    return 1;
  }

  ulint j = 0;
  while (j < function->args->count) {
    struct function_arg * arg = function->args->array + j;
    // Once again, we have edge cases : if the argument is unamed. It can only mean it is a structure or an enum, since we made sure that would be the case, and if that's the case, the type of the argument is struct or enum.
    // The strategy is : check if that's the case, if so, we snprintf it into a buffer and use the buffer instead of the .type.string
    char typebuf[ENTITIES_NAME_LENGTH + 8] = {0};
    char * input = arg->type.string;
    if (streq("struct", arg->type.string)) {
      snprintf(typebuf, ENTITIES_NAME_LENGTH + 8, "struct %s", arg->name); // We get struct nameOfStruct
      input = typebuf;
    } else if (streq("enum", arg->type.string)) {
      snprintf(typebuf, ENTITIES_NAME_LENGTH + 8, "enum %s", arg->name); // We get eunm NameOfEnum
      input = typebuf;
    }
    if (input != arg->type.string) { // Also replace name of argument and type
      STRCP(typebuf, arg->type.string, ENTITIES_NAME_LENGTH, 1);
      snprintf(arg->name, ENTITIES_NAME_LENGTH, "__arg_no_%zu", j + 1);
    }
    if (assign_type(code, input, NULL) != 0) {
      fprintf(stderr, "Cannot assign type of argument number %zu of function %s (argument is %s, its type is %s)\n", j + 1, function->name, arg->name, arg->type.string);
      args->results.fails++;
      return 1;
    }
    j++;
  }
  args->results.links += 1 + function->args->count;
  return 0;
}
static int prepare_functions(struct function_indic * functions, source_code_t code, callback_args args) {
  ulint i = 0;
  while (i < functions->count) {
    prepare_function(functions->array + i, code, args);
    i++;
  }

  return 0;
}
static int prepare_struct(struct struct_spec * strc, source_code_t code, callback_args args) {
  ulint j = 0;
  while (j < strc->members->count) {
    if (assign_type(code, strc->members->array[j].type.string, NULL) != 0) {
      fprintf(stderr, "Cannot assign type to member number %zu (%s) of structure %s\n", j + 1, strc->members->array[j].name, strc->name);
      args->results.fails++;
      return 1;
    }
    j++;
  }
  args->results.links += strc->members->count;
  return 0;
}
static int prepare_structs(struct struct_indic * structs, source_code_t code, callback_args args) {
  ulint i = 0;
  while (i < structs->count) {
    prepare_struct(structs->array + i, code, args);
    i++;
  }
  return 0;
}
static int prepare_variables(struct variables_indic * variables, source_code_t code, callback_args args) {
  ulint i = 0;
  while (i < variables->count) {
    struct variable * var = variables->array + i;
    if (assign_type(code, var->type->string, NULL) != 0) {
      fprintf(stderr, "Cannot assign type to variable %s (type : %s)\n", var->name, var->type->string);
      args->results.fails++;
      return 1;
    }
    i++;
  }
  args->results.links += variables->count;
  return 0;
}
static int prepare_typedefs(struct typedef_indic * tds, source_code_t code) {
  struct register_primitive_data data;
  init_primitive_data(&data);

  char * input_rest = NULL;
  ulint i = 0;
  while (i < tds->count) {
    init_primitive_data(&data);
    struct typedef_spec * td = tds->array + i;
    char * input = td->buffer;
    GOTO_NEXTWORD(input);
    char * last_space = NULL;
    char * ptr = input;
    char * star = NULL;
    while (*ptr != 0) {
      if (*ptr == ' ') last_space = ptr;
      else if (*ptr == '*' && star == NULL) star = ptr - 1;
      ptr++;
    }
    if (last_space != NULL) *last_space = 0;
    if (star != NULL) *star = 0;

    if (!td->is_function && try_parse_primitive(input, &data, &input_rest) == 0) {
      if (last_space != NULL) *last_space = ' ';
      if (star != NULL) *star = ' '; // Yes I know it is counter intuitive
      // Find the already registered primitive if it exists
      ulint j = 0;
      while (j < code->primitives.count) {
        struct primitive_spec * spec = code->primitives.array + j;
        if (
            spec->source == data.source &&
            spec->is_structure == data.is_structure && // That sould be set to 0 though
            streq(data.buffer, spec->name)
        ) { // Then they are the same
          if (last_space != NULL) *last_space = ' ';
          break;
        }
        j++;
      }

      if (j == code->primitives.count && register_primitive(code, &data) != 0) return 1;
      // The added primitive is automatically the last
    }
    if (last_space != NULL) *last_space = ' ';
    if (star != NULL) *star = ' ';
    i++;
  }
  return 0;
}

static int prepare_linkage(source_code_t code, callback_args args) {
  prepare_functions(&(code->definitions.functions), code, args);
  prepare_functions(&(code->implementations.functions), code, args);
  prepare_structs(&(code->definitions.structs), code, args);
  prepare_structs(&(code->implementations.structs), code, args);
  prepare_variables(&(code->definitions.variables), code, args);
  prepare_typedefs(&(code->definitions.typedefs), code);
  prepare_typedefs(&(code->implementations.typedefs), code);
  return 0;
}
int linkage(source_code_t code, callback_args args) {
  size_t before = args->results.fails;
  prepare_linkage(code, args);
  link_functions(&(code->definitions.functions), code, args);
  link_functions(&(code->implementations.functions), code, args);
  link_structs(&(code->definitions.structs), code, args);
  link_structs(&(code->implementations.structs), code, args);
  link_typedefs(&(code->definitions.typedefs), code, args);
  link_typedefs(&(code->implementations.typedefs), code, args);
  link_variables(&(code->definitions.variables), code, args);

  link_typedefs_result(&(code->definitions.typedefs), code, args);
  link_typedefs_result(&(code->implementations.typedefs), code, args);

  return args->results.fails <= before;
}

static void minimize(const char * input, char * output, size_t max) {
  while (*input != 0 && max > 0) {
    if (65 <= *input && *input <= 90) *output = *input + 32;
    else *output = *input;
    max--;
  }
}
static int sort_function(const void * a, const void * b) {
  char abuf[ENTITIES_NAME_LENGTH + 1];
  char bbuf[ENTITIES_NAME_LENGTH + 1];
  minimize(((struct function_spec *)a)->name, abuf, ENTITIES_NAME_LENGTH);
  minimize(((struct function_spec *)b)->name, bbuf, ENTITIES_NAME_LENGTH);

  return strcmp(abuf, bbuf);
}
static int sort_structure(const void * a, const void * b) {
  char abuf[ENTITIES_NAME_LENGTH + 1];
  char bbuf[ENTITIES_NAME_LENGTH + 1];

  minimize(((struct struct_spec *)a)->name, abuf, ENTITIES_NAME_LENGTH);
  minimize(((struct struct_spec *)b)->name, bbuf, ENTITIES_NAME_LENGTH);

  return strcmp(abuf, bbuf);
}
static int sort_macro(const void * a, const void * b) {
  char abuf[ENTITIES_NAME_LENGTH + 1];
  char bbuf[ENTITIES_NAME_LENGTH + 1];

  minimize(((struct macro_spec *)a)->name, abuf, ENTITIES_NAME_LENGTH);
  minimize(((struct macro_spec *)b)->name, bbuf, ENTITIES_NAME_LENGTH);

  return strcmp(abuf, bbuf);
}
static int sort_variable(const void * a, const void * b) {
  char abuf[ENTITIES_NAME_LENGTH + 1];
  char bbuf[ENTITIES_NAME_LENGTH + 1];

  minimize(((struct variable *)a)->name, abuf, ENTITIES_NAME_LENGTH);
  minimize(((struct variable *)b)->name, bbuf, ENTITIES_NAME_LENGTH);

  return strcmp(abuf, bbuf);
}
static int sort_enum(const void * a, const void * b) {
  char abuf[ENTITIES_NAME_LENGTH + 1];
  char bbuf[ENTITIES_NAME_LENGTH + 1];

  minimize(((struct enum_spec *)a)->name, abuf, ENTITIES_NAME_LENGTH);
  minimize(((struct enum_spec *)b)->name, bbuf, ENTITIES_NAME_LENGTH);

  return strcmp(abuf, bbuf);
}
static int sort_typedef(const void * a, const void * b) {
  char abuf[ENTITIES_NAME_LENGTH + 1];
  char bbuf[ENTITIES_NAME_LENGTH + 1];

  minimize(((struct typedef_spec *)a)->name, abuf, ENTITIES_NAME_LENGTH);
  minimize(((struct typedef_spec *)b)->name, bbuf, ENTITIES_NAME_LENGTH);

  return strcmp(abuf, bbuf);
}
int prelinkage(source_code_t code, callback_args args) {
  // Sorting
  qsort(code->implementations.functions.array, code->implementations.functions.count, sizeof(struct function_spec), sort_function);
  qsort(code->definitions.functions.array, code->definitions.functions.count, sizeof(struct function_spec), sort_function);
  qsort(code->definitions.structs.array, code->definitions.structs.count, sizeof(struct struct_spec), sort_structure);
  qsort(code->implementations.structs.array, code->implementations.structs.count, sizeof(struct struct_spec), sort_structure);
  qsort(code->definitions.typedefs.array, code->definitions.typedefs.count, sizeof(struct typedef_spec), sort_typedef);
  qsort(code->implementations.typedefs.array, code->implementations.typedefs.count, sizeof(struct typedef_spec), sort_typedef);
  qsort(code->definitions.macros.array, code->definitions.macros.count, sizeof(struct macro_spec), sort_macro);
  qsort(code->implementations.macros.array, code->implementations.macros.count, sizeof(struct macro_spec), sort_macro);
  qsort(code->definitions.enums.array, code->definitions.enums.count, sizeof(struct enum_spec), sort_enum);
  qsort(code->implementations.enums.array, code->implementations.enums.count, sizeof(struct enum_spec), sort_enum);
  qsort(code->definitions.variables.array, code->definitions.variables.count, sizeof(struct variable), sort_variable);

  args->results.success++;
  return 0;
}
#undef GOTO_NEXTWORD
