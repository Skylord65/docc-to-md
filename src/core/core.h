#ifndef __CORE_H__
#define __CORE_H__

#include "../reader/reader.h"

#include <linux/limits.h>

typedef enum {
  Ok = 0,
  UnknownPath = 1,
  ReadError = 2
} recursion_return_t;

struct gen_config {
  char src[PATH_MAX];
  char dest[PATH_MAX];
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

extern int create_config(const char * src, const char * dst, gen_config_t);

struct status_s {
  int in_function;
  int is_header;
  int reading_function_params;

  int in_multine_comment;

  unsigned long long int line_count;
};
extern int try_extract_variable(char * buffer, const char * path, callback_args args, struct status_s *);

#endif
