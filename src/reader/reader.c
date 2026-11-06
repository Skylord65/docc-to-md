#include "reader.h"
#include <stdlib.h>

void free_source_code(source_code_t code) {
  if (code->variables.count > 0) {
    free(code->variables.array);
  }
}
