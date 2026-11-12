#include "reader.h"
#include "../core/core.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void free_type(entity_type_t * type) {
  if (type->arrays.count > 0) {
    ulint i = 0;
    while (i < type->arrays.count && type->arrays.array != NULL) {
      if (type->arrays.array[i] != NULL) free(type->arrays.array[i]);
      i++;
    }
    free(type->arrays.array);
  }
}
static void free_macro(struct macro_spec * macro) {
  if (macro->is_function) {
    if (macro->args->count > 0) {
      ulint i = 0;
      while (i < macro->args->count) {
        free(macro->args->array[i]);
        i++;
      }
      free(macro->args->array);
      free(macro->args);
    }
  }

  free(macro->ref);
}
static void free_macro_array(struct macro_indic * macros) {
  if (macros->count > 0) {
    ulint i = 0;
    while (i < macros->count) {
      free_macro(&(macros->array[i]));
      i++;
    }

    free(macros->array);
  }
}
static void free_structure(struct struct_spec * structure) {
  if (structure->members->count > 0 && structure->members->array != NULL) {
    ulint i = 0;
    while (i < structure->members->count) {
      free_type(&(structure->members->array[i].type));
      i++;
    }
    free(structure->members->array);
  }
  free(structure->members);
  if (structure->ref != NULL) free(structure->ref);
}
static void free_structures_array(struct struct_indic * structures) {
  if (structures->count > 0) {
    ulint i = 0;
    while (i < structures->count) {
      free_structure(structures->array + i);
      i++;
    }
    free(structures->array);
  }
}
static void free_variables(struct variables_indic * variables) {
  ulint i = 0;
  while (i < variables->count) {
    if (variables->array[i].type != NULL) free_type(variables->array[i].type);
    free(variables->array[i].type);
    free(variables->array[i].ref);
    i++;
  }
  free(variables->array);
}
static void free_function(struct function_spec * function) {
  if (function->args != NULL && function->args->count > 0 && function->args->array != NULL) {
    ulint i = 0;
    while (i < function->args->count) {
      free_type(&(function->args->array[i].type));
      i++;
    }
    free(function->args->array);
  }
  if (function->description != NULL) free(function->description);
  free(function->args);
  free(function->ref);
}
static void free_functions(struct function_indic * functions) {
  ulint i = 0;
  while (i < functions->count) {
    free_function(functions->array + i);
    i++;
  }
  if (functions->count > 0 && functions->array != NULL) free(functions->array);
}
static void free_enum(struct enum_spec * enumerator) {
  if (enumerator->members->count > 0) free(enumerator->members->array);

  free(enumerator->ref);
  free(enumerator->members);
}
static void free_enums(struct enum_indic * enumerators) {
  if (enumerators->count > 0) {
    ulint i = 0;
    while (i < enumerators->count) {
      free_enum(enumerators->array + i);
      i++;
    }
    free(enumerators->array);
  }
}
static void free_typedef(struct typedef_spec * td) {
  if (td->ref != NULL) free(td->ref);
  if (td->definition.type == TypedefinedStructure) {
    free_structure(td->definition.ptr);
    free(td->definition.ptr);
  } else if (td->definition.type == TypedefinedEnum) {
    free_enum(td->definition.ptr);
    free(td->definition.ptr);
  } else if (td->definition.type == TypedefinedFunction) {
    free_function(td->definition.ptr);
    free(td->definition.ptr);
  }
  free_type(&(td->definition));
  if (td->buffer != NULL) free(td->buffer);
}
static void free_typedefs(struct typedef_indic * tds) {
  ulint i = 0;
  while (i < tds->count) {
    free_typedef(tds->array + i);
    i++;
  }
  free(tds->array);
}
static void free_primitives(struct primitives_indic * primitives) {
  if (primitives->count > 0) free(primitives->array);
}
void free_source_code(source_code_t code) {
  free_variables(&(code->definitions.variables));
  free_macro_array(&(code->definitions.macros));
  free_macro_array(&(code->implementations.macros));
  free_functions(&(code->implementations.functions));
  free_functions(&(code->definitions.functions));
  free_enums(&(code->definitions.enums));
  free_enums(&(code->implementations.enums));
  free_typedefs(&(code->definitions.typedefs));
  free_typedefs(&(code->implementations.typedefs));

  free_structures_array(&(code->implementations.structs));
  free_structures_array(&(code->definitions.structs));

  free_primitives(&(code->primitives));
}
#define INIT(code, word) code->definitions.word.array = NULL;\
code->definitions.word.count = 0;\
code->implementations.word.array = NULL;\
code->implementations.word.count = 0;

void init_source_code(source_code_t code) {
  code->primitives.array = NULL;
  code->primitives.count = 0;
  code->definitions.variables.count = 0;
  code->definitions.variables.array = NULL;
  INIT(code, structs);
  INIT(code, typedefs);
  INIT(code, enums);
  INIT(code, functions);
  INIT(code, macros);
}
#undef INIT

#define ARGS_T char * buffer, int fd
#define ARGS_V buffer, fd
#define DUMP(...) {\
  size_t bytes = snprintf(buffer, BUFSIZ, __VA_ARGS__);\
  write(fd, buffer, bytes);\
  write(fd, "\n", 1);\
}

static void dump_macros(ARGS_T, struct macro_indic * macros) {
  DUMP("////MACROS")
  ulint i = 0;
  while (i < macros->count) {
    struct macro_spec * macro = macros->array + i;
    DUMP("Defined in %s:%zu \n#define %s %s", macro->ref->filename, macro->ref->line, macro->name, macro->placeholder);
    DUMP("------- Arguments :");

    if (!macro->is_function || macro->args->count == 0) DUMP("   [None]")
    else {
      ulint j = 0;
      while (j < macro->args->count) {
        DUMP("   - %s", macro->args->array[j]);
        j++;
      }
    }
    DUMP(" ");
    i++;
  }
}
static void dump_variables(ARGS_T, struct variables_indic * variables) {
  DUMP("////VARIABLES")
  ulint i = 0;
  while (i < variables->count) {
    struct variable * variable = variables->array + i;
    DUMP("Defined in %s:%zu\n  Type: %s\n  Name: %s\n", variable->ref->filename, variable->ref->line, variable->type->string, variable->name)
    i++;
  }
}
static void dump_structures(ARGS_T, struct struct_indic * structures) {
  DUMP("////STRUCTURES")
  ulint i = 0;
  while (i < structures->count) {
    struct struct_spec * structure = structures->array + i;
    DUMP("/Structure %s\nDefined in %s:%zu\nstruct %s {", structure->name, structure->ref->filename, structure->ref->line, structure->name);

    ulint j = 0;
    while (j < structure->members->count) {
      DUMP(" %s %s", structure->members->array[j].type.string, structure->members->array[j].name)
      j++;
    }
    DUMP( "}\n------ Members :");
    j = 0;
    while (j < structure->members->count) {
      DUMP(" - [%s], type : %s", structure->members->array[j].name, structure->members->array[j].type.string);
      j++;
    }
    if (j == 0) DUMP("[ NONE ]");
    i++;
    DUMP(" ");
  }
}
static void dump_function(ARGS_T, struct function_spec * function) {
  DUMP("Defined in %s:%zu\n  NAME: %s\n  Modifiers :", function->ref->filename, function->ref->line, function->name);
  if (function->description != NULL) DUMP("--------\n%s\n--------", function->description)

  if (F_INONE(function->modifiers)) DUMP("    - [NONE]")
  else {
    if (F_ISTATIC(function->modifiers)) DUMP("   - static");
    if (F_IEXTERN(function->modifiers)) DUMP("   - extern");
    if (F_IVOLATI(function->modifiers)) DUMP("   - volatile");
  }

  DUMP(" Arguments : ");
  if (function->args->count == 0) DUMP("    - [NONE]")
  else {
    ulint i = 0;
    while (i < function->args->count) {
      struct function_arg * arg = function->args->array + i;
      DUMP("    - %s : %s", arg->name, arg->type.string);
      i++;
    }
  }

  DUMP("  Returns : %s\n", function->returntype.string);
}
static void dump_functions(ARGS_T, struct function_indic * functions) {
  DUMP("////FUNCTIONS")
  ulint i = 0;
  while (i < functions->count) {
    dump_function(buffer, fd, functions->array + i);
    i++;
  }
}
static void dump_enum(ARGS_T, struct enum_spec * enumerator) {
  DUMP("/Enumerator %s\nDefined in %s:%zu\nenum {", enumerator->name, enumerator->ref->filename, enumerator->ref->line);
  ulint j = 0;
  while (j < enumerator->members->count) {
    if (j == enumerator->members->count - 1) DUMP("  %s = %s", enumerator->members->array[j].name, enumerator->members->array[j].value)
    else DUMP("  %s = %s%c", enumerator->members->array[j].name, enumerator->members->array[j].value, ',');
    j++;
  }
  DUMP("}");
  DUMP("Modifiers : ");
  if (F_INONE(enumerator->modifiers)) DUMP("  [NONE]")
  else {
    if (F_ISTATIC(enumerator->modifiers)) DUMP("   - static");
    if (F_IEXTERN(enumerator->modifiers)) DUMP("   - extern");
    if (F_IVOLATI(enumerator->modifiers)) DUMP("   - volatile");
  }
  DUMP(" ");
}
static void dump_enums(ARGS_T, struct enum_indic * enums) {
  DUMP("////ENUMERATORS")
  ulint i = 0;
  while (i < enums->count) {
    dump_enum(ARGS_V, enums->array + i);
    i++;
  }
}
static void dump_typedef(ARGS_T, struct typedef_spec * spec) {
  if (spec->buffer != NULL) DUMP("/Typedef %s\nDefined in %s:%zu\ntypedef %s %s\n", spec->name, spec->ref->filename, spec->ref->line, spec->buffer, spec->name);
}
static void dump_typedefs(ARGS_T, struct typedef_indic * indic) {
  DUMP("////Typedefs")
  ulint i = 0;
  while (i < indic->count) {
    dump_typedef(ARGS_V, indic->array + i);
    i++;
  }
}
#define PRIMITIVE_DUMP(p, TEST_MACRO, text) if (TEST_MACRO(p->source)) DUMP(" - " text);
static void dump_primitive(ARGS_T, struct primitive_spec * primitive) {
  char namebuf[ENTITIES_NAME_LENGTH * 2];
  int index = 0;
  if (primitive->is_structure) {
    sprintf(namebuf, "struct ");
    index += 7;
  }
  ulint nameindex = 0;
  while (primitive->name[nameindex] != 0 && index < ENTITIES_NAME_LENGTH) {
    namebuf[index] = primitive->name[nameindex];
    index++;
    nameindex++;
  }
  namebuf[index++] = 0;
  DUMP("/Primitive %s\nContains elements of :", namebuf);
  PRIMITIVE_DUMP(primitive, P_TGLIBC, "GLibC");  
  PRIMITIVE_DUMP(primitive, P_TSTDINT, "stdint.h");
  PRIMITIVE_DUMP(primitive, P_TFCNTL, "fcntl.h");
  PRIMITIVE_DUMP(primitive, P_TDIRENT, "dirent.h");
  PRIMITIVE_DUMP(primitive, P_TLIBSTRINGPGREP, "stringpgrep.h");
  PRIMITIVE_DUMP(primitive, P_TSTDBOOL, "stdbool.h");
  PRIMITIVE_DUMP(primitive, P_TSTDDEF, "stddef.h");
  PRIMITIVE_DUMP(primitive, P_TSTDIO, "stdio.h");
  PRIMITIVE_DUMP(primitive, P_TSTDLIB, "stdlib.h");
  PRIMITIVE_DUMP(primitive, P_TSTDTIME, "time.h");
  PRIMITIVE_DUMP(primitive, P_TSYSDIR, "sys/dir.h");
  PRIMITIVE_DUMP(primitive, P_TSYSSTAT, "sys/stat.h");
  PRIMITIVE_DUMP(primitive, P_TSYSTYPES, "sys/types.h");
  PRIMITIVE_DUMP(primitive, P_TUNISTD, "unistd.h");

  DUMP(" ");
}
#undef PRIMITIVE_DUMP
static void dump_primitives(ARGS_T, const struct primitives_indic * indic) {
  DUMP("////PRIMITIVES");
  ulint i = 0;
  while (i < indic->count) {
    dump_primitive(ARGS_V, indic->array + i);
    i++;
  }
}

int dump_source_code(const source_code_t code, int fd) {
  char buffer[BUFSIZ];

  DUMP("// Headers //");
  dump_primitives(ARGS_V, &(code->primitives));
  dump_variables(ARGS_V, &(code->definitions.variables));
  dump_macros(ARGS_V, &(code->definitions.macros));
  dump_structures(ARGS_V, &(code->definitions.structs));
  dump_functions(ARGS_V, &(code->definitions.functions));
  dump_enums(ARGS_V, &(code->definitions.enums));
  dump_typedefs(ARGS_V, &(code->definitions.typedefs));

  DUMP("\n\n// Core code //");
  dump_macros(ARGS_V, &(code->implementations.macros));
  dump_structures(ARGS_V, &(code->implementations.structs));
  dump_functions(ARGS_V, &(code->implementations.functions));
  dump_enums(ARGS_V, &(code->implementations.enums));
  dump_typedefs(ARGS_V, &(code->implementations.typedefs));

  return 0;
}

void summarise_parsing(source_code_t code) {
  ulint header_var_macro = 0;
  ulint source_var_macro = 0;
  ulint header_fnt_macro = 0;
  ulint source_fnt_macro = 0;
  ulint i = 0;
  while (i < code->definitions.macros.count) {
    if (code->definitions.macros.array[i].is_function) header_fnt_macro++;
    else header_var_macro++;
    i++;
  }
  i = 0;
  while (i < code->implementations.macros.count) {
    if (code->implementations.macros.array[i].is_function) source_fnt_macro++;
    else source_var_macro++;
    i++;
  }

  printf("------------------------------------\n- %zu variables registered\n- %zu functions registered (%zu headers + %zu source)\n- %zu macros (%zu headers + %zu source) :",
    code->definitions.variables.count,
    code->definitions.functions.count + code->implementations.functions.count,
    code->definitions.functions.count,
    code->implementations.functions.count,
    code->definitions.macros.count + code->implementations.macros.count,
    code->definitions.macros.count,
    code->implementations.macros.count
  );
  printf("\n  - %zu functions (%zu headers + %zu source)\n  - %zu variables (%zu headers + %zu source)\n- %zu structures registered (%zu headers + %zu source)",
    header_fnt_macro + source_fnt_macro,
    header_fnt_macro,
    source_fnt_macro,
    header_var_macro + source_var_macro,
    header_var_macro,
    source_var_macro,
    code->definitions.structs.count + code->implementations.structs.count,
    code->definitions.structs.count,
    code->implementations.structs.count
  );
  printf("\n- %zu enumerators registered (%zu headers + %zu source)\n- %zu typedefs registered (%zu headers + %zu source)\n------------------------------------\n",
    code->definitions.enums.count + code->implementations.enums.count,
    code->definitions.enums.count,
    code->implementations.enums.count,
    code->definitions.typedefs.count + code->implementations.typedefs.count,
    code->definitions.typedefs.count,
    code->implementations.typedefs.count
  );
}

void summarise_linking(const callback_args args) {
  if (!(args->config->verbose_registration)) return;
  printf("------------------------------------\n%lu links made\n------------------------------------\n", args->results.links);
}

#undef ARGS_T
#undef ARGS_V
#undef DUMP
