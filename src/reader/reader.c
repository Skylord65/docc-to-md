#include "reader.h"
#include <stdio.h>
#include <stdlib.h>

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
void free_source_code(source_code_t code) {
  if (code->definitions.variables.count > 0) {
    free(code->definitions.variables.array);
  }
  free_macro_array(&(code->definitions.macros));
  free_macro_array(&(code->implementations.macros));

  free_structures_array(&(code->implementations.structs));
  free_structures_array(&(code->definitions.structs));
}
