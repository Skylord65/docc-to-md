#include "../macros.h"
#include "core.h"
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>

int try_extract_variable(char * buffer, const char * path, callback_args params, struct status_s * status) {
  if (*buffer == '#') return 1;

  char * ptr = (char *)buffer;
  int has_equal;
  char * word_before_equal = buffer;
  char * type = buffer;
  char * equals = NULL;

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

  if (*ptr == ')') return 1; // This is the end of a function
  // Find the first word backwards
  while (*ptr != ' ' && ptr != buffer) {
    if (*ptr == '(') return 1; // This is the start of a function in the name
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

  printf("Variable type is : %s and variable name is %s\n", type, word_before_equal);

  if (params->source_code->variables.count == 0) {
    if ((params->source_code->variables.array = malloc(sizeof(struct variable))) == NULL) {
      perror("malloc");
      params->results.fails++;
      return 1;
    }
  } else if ((params->source_code->variables.array = realloc(params->source_code->variables.array, sizeof(struct variable) * (params->source_code->variables.count + 1))) == NULL) {
    perror("realloc");
    params->results.fails++;
    return 1;
  }

  params->source_code->variables.array[params->source_code->variables.count] = (struct variable){
    .ref = &(struct reference){
      .line = status->line_count + 1
    },
    .type = NULL
  };
  struct variable var = params->source_code->variables.array[params->source_code->variables.count];
  STRCP((char *)path, var.ref->filename, PATH_MAX, 1);
  STRCP(word_before_equal, var.name, ENTITIES_NAME_LENGTH, 1);

  params->source_code->variables.count++;

  return 0;
}
