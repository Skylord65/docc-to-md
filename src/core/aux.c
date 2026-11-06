#include "core.h"
#include "../parsers/stringlib.h"
#include "../macros.h"
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>

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
      .ref = &(struct reference){
        .line = status->line_count + 1
      },
      .type = &(entity_type_t){
        .ptr = NULL,
        .type = NoneType,
      }
    };
  }
  struct variable var = params->source_code->definitions.variables.array[params->source_code->definitions.variables.count];
  STRCP((char *)path, var.ref->filename, PATH_MAX, 1);
  STRCP(word_before_equal, var.name, ENTITIES_NAME_LENGTH, 1);
  STRCP(typebuf, var.type->string, ENTITIES_NAME_LENGTH, 1);

  params->source_code->definitions.variables.count++;

  return 0;
}
