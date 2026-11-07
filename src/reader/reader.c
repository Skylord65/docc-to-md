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
void free_source_code(source_code_t code) {
  free_variables(&(code->definitions.variables));
  free_macro_array(&(code->definitions.macros));
  free_macro_array(&(code->implementations.macros));

  free_structures_array(&(code->implementations.structs));
  free_structures_array(&(code->definitions.structs));
}

#define DUMP_WRITE(buffer, fd, instruction) {\
  size_t bytes = instruction;\
  write(fd, buffer, bytes);\
  write(fd, "\n", 1);\
}
#define ARGS_T char * buffer, int fd
#define ARGS_V buffer, fd

static void dump_macros(ARGS_T, struct macro_indic * macros) {
  ulint i = 0;
  while (i < macros->count) {
    struct macro_spec * macro = macros->array + i;
    DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "Defined in %s:%lu \n#define %s %s", macro->ref->filename, macro->ref->line, macro->name, macro->placeholder));
    DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "------- Arguments :"));

    if (!macro->is_function || macro->args->count == 0) DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "   [None]"))
    else {
      ulint j = 0;
      while (j < macro->args->count) {
        DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "   - %s", macro->args->array[j]));
        j++;
      }
    }
    DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, " "));
    i++;
  }
}
static void dump_variables(ARGS_T, struct variables_indic * variables) {
  ulint i = 0;
  while (i < variables->count) {
    struct variable * variable = variables->array + i;
    DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "Defined in %s:%lu\n  Type: %s\n  Name: %s\n", variable->ref->filename, variable->ref->line, variable->type->string, variable->name))
    i++;
  }
}
static void dump_structures(ARGS_T, struct struct_indic * structures) {
  ulint i = 0;
  while (i < structures->count) {
    struct struct_spec * structure = structures->array + i;
    DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "/Structure %s\nDefined in %s:%lu\nstruct %s {\n", structure->name, structure->ref->filename, structure->ref->line, structure->name));

    ulint j = 0;
    while (j < structure->members->count) {
      DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, " %s %s", structure->members->array[j].name, structure->members->array[j].type.string))
      j++;
    }
    DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ,  "}\n------ Members :"));
    j = 0;
    while (j < structure->members->count) {
      DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, " - [%s], type : %s\n", structure->members->array[j].name, structure->members->array[j].type.string));
      j++;
    }
    if (j == 0) DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "[ NONE ]"));
    i++;
    DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, " "));
  }
}
int dump_source_code(const source_code_t code, int fd) {
  char buffer[BUFSIZ];

  DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "// Headers //"));
  DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "//// Variables"));
  if (code->definitions.variables.count > 0) dump_variables(ARGS_V, &(code->definitions.variables));

  DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "//// Macros"));
  if (code->definitions.macros.count > 0) dump_macros(ARGS_V, &(code->definitions.macros));
  
  DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "//// Structures"));
  if (code->definitions.structs.count > 0) dump_structures(ARGS_V, &(code->definitions.structs));


  DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "\n\n// Core code //"));
  DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "//// Macros"));
  if (code->implementations.macros.count > 0) dump_macros(ARGS_V, &(code->implementations.macros));

  DUMP_WRITE(buffer, fd, snprintf(buffer, BUFSIZ, "//// Structures"));
  if (code->definitions.structs.count > 0) dump_structures(ARGS_V, &(code->definitions.structs));

  return 0;
}

#undef ARGS_T
#undef ARGS_V
