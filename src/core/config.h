#if defined(_WIN32)
  #include <stdlib.h>
  #ifndef PATH_MAX
    #define PATH_MAX _MAX_PATH
  #endif
#else
  #include <limits.h>
#endif
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stddef.h>

struct gen_config {
  char src[PATH_MAX];
  char dest[PATH_MAX];
  char work[PATH_MAX];
  char * srccp;
  char * srcdirname;
  int verbose_registration;
  char * rebase_prefix;
  char gitline;
  char reference_files_section;
  char prelinkage;
};
typedef struct gen_config * gen_config_t;

struct result {
  size_t success;
  size_t fails;
  size_t links;
  size_t files_written;
};
struct callback_args_s;
typedef struct callback_args_s * callback_args;

#endif
