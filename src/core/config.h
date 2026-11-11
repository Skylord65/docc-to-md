#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <linux/limits.h>

struct gen_config {
  char src[PATH_MAX];
  char dest[PATH_MAX];
  char work[PATH_MAX];
  int verbose_registration;
};
typedef struct gen_config * gen_config_t;

struct result {
  unsigned long int success;
  unsigned long int fails;
  unsigned long int links;
};
struct callback_args_s;
typedef struct callback_args_s * callback_args;

#endif
