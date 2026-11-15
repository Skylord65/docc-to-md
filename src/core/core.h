#ifndef __CORE_H__
#define __CORE_H__

#include "../reader/reader.h"
#include "config.h"

#if defined(_WIN32)
  #include <stdlib.h>
  #ifndef PATH_MAX
    #define PATH_MAX _MAX_PATH
  #endif
#else
  #include <limits.h>
#endif
#include <unistd.h>

#define IS_STRING_DEL(origin, cptr) (*(cptr) == '"' && (cptr == origin || *(cptr - 1) != '\\'))
#define IS_CHAR_DEL(origin, cptr) (*(cptr) == '\'' && (cptr == origin || *(cptr - 1) != '\\'))

typedef enum {
  Ok = 0,
  UnknownPath = 1,
  ReadError = 2
} recursion_return_t;

struct callback_args_s {
  gen_config_t config;
  source_code_t source_code;

  struct result results;
};
typedef void recursion_callback_t(const char * src, callback_args params);

extern recursion_return_t recursion(const char * src, callback_args params, recursion_callback_t callback);
extern void recursion_callback(const char * src, callback_args params);
extern void remove_comments_callback(const char * src, callback_args params);
extern void register_docstring_callback(const char * src, callback_args params);
/*
 * This function replaces in `path` the source `replace` by the new path `by`.
 * It writes in buffer as the function goes by
 */
extern void rebase_path(const char * path, const char * replace, const char * by, char * buffer, size_t max);
extern int create_config(const char * src, const char * dst, const char * work, const char * rebase_prefix, gen_config_t);
extern int mkdirp(const char * path);
extern int rmdirrf(const char * path);
extern void cleanup(gen_config_t);

struct status_s {
  int in_function;
  int is_header;
  int reading_function_params;
  int reading_multiline_macro;
  int reading_multiline_struct;
  int reading_multiline_enum;
  int reading_multiline_typedef;
  int automatic;

  unsigned long long int line_count;
  void * data;
};
struct macro_multiline_status_s {
  char ** array;
  ulint params_count;
  int is_function;
  ulint content_index;
  char content[MACRO_PLACEHOLDER_LENGTH];
  char name[ENTITIES_NAME_LENGTH];
  int in_string;
  int in_char;
  ulint starts_on_line;
};
/**
 * `char ** member_array` is an array of strings. Temporarily store them as such and compute the types later in `register_structure` call
 */
struct struct_multiline_status_s {
  char name[ENTITIES_NAME_LENGTH];
  ulint starts_on_line;
  int reading_members;
  int reading_member;
  char is_pending;
  ulint members_count;
  struct struct_spec ** doubleptr;
  char ** members_array; // Store them as string, keep the computing when we are sure it is complete
};
struct function_multiline_status_s {
  char name[ENTITIES_NAME_LENGTH];
  entity_type_t returntype;
  ulint starts_on_line;
  int reading_args;
  int reading_arg;
  ulint args_count;
  char ** args; // Store them as string, keep the computing for when we are sure it is acceptable
  fnt_modifier modifiers;
  const char * filenameptr;
  unsigned int words_counted;
  int is_declaration;
  char multiline_parameter_buffer[ENTITIES_NAME_LENGTH * 2];
  unsigned int parameter_index;
  struct function_spec ** doubleptr;
};
struct enum_multiline_status_s {
  char name[ENTITIES_NAME_LENGTH];
  ulint starts_on_line;
  ulint members_count;
  struct enum_member * members;
  fnt_modifier modifiers;
  char member_name_buffer[ENTITIES_NAME_LENGTH];
  unsigned short int member_name_index;
  unsigned short int value_buffer_index;
  char found_equal;
  char value_buffer[ENTITIES_NAME_LENGTH];
  struct enum_spec ** doubleptr;
};
struct typedef_multiline_status_s {
  ulint starts_on_line;
  size_t buffer_size;
  unsigned long int buffer_index;
  char * buffer;
  fnt_modifier modifiers;
  unsigned int pending_braces;
};

extern int normalize_type_string(const char * type, char * typebuf, int with_normalize_newline);
extern int try_extract_variable(char * buffer, const char * path, callback_args args, struct status_s *);
extern int try_extract_macro(char * buffer, const char * path, callback_args args, struct status_s *);
extern int try_extract_macro_multiline(char * buffer, const char * path, callback_args, struct status_s *);
extern int try_extract_struct(char * buffer, const char * path, callback_args args, struct status_s *);
extern int try_extract_struct_multiline(char * buffer, const char * path, callback_args args, struct status_s *);
extern int try_extract_function(char * buffer, const char * path, callback_args args, struct status_s *);
extern int try_extract_function_multiline(char * buffer, const char * path, callback_args args, struct status_s *);
extern int try_extract_enum(char * buffer, const char * path, callback_args, struct status_s *);
extern int try_extract_enum_multiline(char * buffer, const char * path, callback_args args, struct status_s *);
extern int try_extract_typedef(char * buffer, const char * path, callback_args, struct status_s *);
extern int try_extract_typedef_multiline(char * buffer, const char * path, callback_args args, struct status_s *);

extern int linkage(source_code_t code, callback_args args);

// Some registering functions
void register_function(const char * path, callback_args params, struct status_s * status);
void register_enum(const char * path, callback_args args, struct status_s * status);
int register_structure(const char * filename, callback_args params, struct status_s * status);

#endif
