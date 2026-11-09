#include "reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
    free(variables->array[i].type);
    free(variables->array[i].ref);
    i++;
  }
  free(variables->array);
}
static void free_function(struct function_spec * function) {
  if (function->args->count > 0 && function->args->array != NULL) free(function->args->array);
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
void free_source_code(source_code_t code) {
  free_variables(&(code->definitions.variables));
  free_macro_array(&(code->definitions.macros));
  free_macro_array(&(code->implementations.macros));
  free_functions(&(code->implementations.functions));
  free_functions(&(code->definitions.functions));
  free_enums(&(code->definitions.enums));
  free_enums(&(code->implementations.enums));

  free_structures_array(&(code->implementations.structs));
  free_structures_array(&(code->definitions.structs));
}

#define ARGS_T char * buffer, int fd
#define ARGS_V buffer, fd
#define DUMP(...) {\
  size_t bytes = snprintf(buffer, BUFSIZ, __VA_ARGS__);\
  write(fd, buffer, bytes);\
  write(fd, "\n", 1);\
}

static void dump_macros(ARGS_T, struct macro_indic * macros) {
  ulint i = 0;
  while (i < macros->count) {
    struct macro_spec * macro = macros->array + i;
    DUMP("Defined in %s:%lu \n#define %s %s", macro->ref->filename, macro->ref->line, macro->name, macro->placeholder);
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
  ulint i = 0;
  while (i < variables->count) {
    struct variable * variable = variables->array + i;
    DUMP("Defined in %s:%lu\n  Type: %s\n  Name: %s\n", variable->ref->filename, variable->ref->line, variable->type->string, variable->name)
    i++;
  }
}
static void dump_structures(ARGS_T, struct struct_indic * structures) {
  ulint i = 0;
  while (i < structures->count) {
    struct struct_spec * structure = structures->array + i;
    DUMP("/Structure %s\nDefined in %s:%lu\nstruct %s {", structure->name, structure->ref->filename, structure->ref->line, structure->name);

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
  DUMP("Defined in %s:%lu\n  NAME: %s\n  Modifiers :", function->ref->filename, function->ref->line, function->name);
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
  ulint i = 0;
  while (i < functions->count) {
    dump_function(buffer, fd, functions->array + i);
    i++;
  }
}
static void dump_enum(ARGS_T, struct enum_spec * enumerator) {
  DUMP("/Enumerator %s\nDefined in %s:%lu\nenum {", enumerator->name, enumerator->ref->filename, enumerator->ref->line);
  ulint j = 0;
  while (j < enumerator->members->count) {
    if (j == enumerator->members->count - 1) DUMP("  %s = %lld", enumerator->members->array[j].name, enumerator->members->array[j].value)
    else DUMP("  %s = %lld%c", enumerator->members->array[j].name, enumerator->members->array[j].value, ',');
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
  ulint i = 0;
  while (i < enums->count) {
    dump_enum(ARGS_V, enums->array + i);
    i++;
  }
}
int dump_source_code(const source_code_t code, int fd) {
  char buffer[BUFSIZ];

  DUMP("// Headers //");
  DUMP("//// Variables");
  if (code->definitions.variables.count > 0) dump_variables(ARGS_V, &(code->definitions.variables));

  DUMP("//// Macros");
  if (code->definitions.macros.count > 0) dump_macros(ARGS_V, &(code->definitions.macros));

  DUMP("//// Structures");
  if (code->definitions.structs.count > 0) dump_structures(ARGS_V, &(code->definitions.structs));

  DUMP("//// Functions");
  dump_functions(ARGS_V, &(code->definitions.functions));

  DUMP("//// Enums");
  dump_enums(ARGS_V, &(code->definitions.enums));


  DUMP("\n\n// Core code //");
  DUMP("//// Macros");
  if (code->implementations.macros.count > 0) dump_macros(ARGS_V, &(code->implementations.macros));

  DUMP("//// Structures");
  if (code->implementations.structs.count > 0) dump_structures(ARGS_V, &(code->implementations.structs));

  DUMP("//// Functions");
  dump_functions(ARGS_V, &(code->implementations.functions));

  DUMP("//// Enums");
  dump_enums(ARGS_V, &(code->implementations.enums));

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

  printf("------------------------------------\n- %lu variables registered\n- %lu functions registered (%lu headers + %lu source)\n- %lu macros (%lu headers + %lu source) :\
\n  - %lu functions (%lu headers + %lu source)\n  - %lu variables (%lu headers + %lu source)\n- %lu structures registered (%lu headers + %lu source)\n",
    code->definitions.variables.count,
    code->definitions.functions.count + code->implementations.functions.count,
    code->definitions.functions.count,
    code->implementations.functions.count,
    code->definitions.macros.count + code->implementations.macros.count,
    code->definitions.macros.count,
    code->implementations.macros.count,
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
}

#undef ARGS_T
#undef ARGS_V
#undef DUMP
