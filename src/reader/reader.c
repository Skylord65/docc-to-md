#include "reader.h"
#include <stdlib.h>

void free_source_code(source_code_t code) {
  if (code->definitions.variables.count > 0) {
    free(code->definitions.variables.array);
  }
}
