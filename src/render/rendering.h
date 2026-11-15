#ifndef __RENDERING_H__
#define __RENDERING_H__

#include "../core/config.h"
#include "../reader/reader.h"

typedef unsigned long long int string_hash;
extern string_hash hash_string(char * input, size_t max);
extern int render(source_code_t, gen_config_t, struct result *);

#endif
