#ifndef __CORE_H__
#define __CORE_H__

#include "../reader/reader.h"

#include <linux/limits.h>
#include <unistd.h>

typedef enum {
  Ok = 0,
  UnknownPath = 1,
  ReadError = 2
} recursion_return_t;

struct gen_config {
  char src[PATH_MAX];
  char dest[PATH_MAX];
  char work[PATH_MAX];
};
typedef struct gen_config * gen_config_t;
struct result {
  unsigned long int success;
  unsigned long int fails;
};
struct callback_args_s {
  gen_config_t config;
  source_code_t source_code;

  struct result results;
};
typedef struct callback_args_s * callback_args;
typedef void recursion_callback_t(const char * src, callback_args params);

extern recursion_return_t recursion(const char * src, callback_args params, recursion_callback_t callback);
extern void recursion_callback(const char * src, callback_args params);
extern void remove_comments_callback(const char * src, callback_args params);

extern int create_config(const char * src, const char * dst, const char * work, gen_config_t);
extern int mkdirp(const char * path);
extern void cleanup(gen_config_t);

struct status_s {
  int in_function;
  int is_header;
  int reading_function_params;
  int reading_multiline_macro;

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
};

extern int normalize_type_string(const char * type, char * typebuf);
extern int try_extract_variable(char * buffer, const char * path, callback_args args, struct status_s *);
extern int try_extract_macro(char * buffer, const char * path, callback_args args, struct status_s *);
extern int try_extract_macro_multiline(char * buffer, const char * path, callback_args, struct status_s *);

#endif
