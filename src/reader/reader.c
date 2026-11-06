#include "reader.h"
#include <stdlib.h>

void free_source_code(source_code_t code) {
  if (code->definitions.variables.count > 0) {
    free(code->definitions.variables.array);
  }
  if (code->implementations.macros.count > 0) {
    ulint i = 0;
    while (i < code->implementations.macros.count) {
      if (code->implementations.macros.array[i].is_function) {
        if (code->implementations.macros.array[i].args->count > 0) {
          ulint j = 0;
          while (j < code->implementations.macros.array[i].args->count) {
            free(code->implementations.macros.array[i].args->array[j]);
            j++;
          }

          free(code->implementations.macros.array[i].args->array);
          free(code->implementations.macros.array[i].args);
        }
      }
      i++;
    }
    free(code->implementations.macros.array);
  }
  if (code->definitions.macros.count > 0) {
    ulint i = 0;
    while (i < code->definitions.macros.count) {
      if (code->definitions.macros.array[i].is_function) {
        if (code->definitions.macros.array[i].args->count > 0) {
          ulint j = 0;
          while (j < code->definitions.macros.array[i].args->count) {
            free(code->definitions.macros.array[i].args->array[j]);
            j++;
          }

          free(code->definitions.macros.array[i].args->array);
          free(code->definitions.macros.array[i].args);
        }
      }
      i++;
    }
    free(code->definitions.macros.array);
  }
}
