#ifndef __MACROS_H__
#define __MACROS_H__

#define PROG_VERSION "1.0.1"

#define STRCP(src, dest, max, with_zero) {\
  char * s = src;\
  char * d = dest;\
  size_t __insider_variable_index = max;\
\
  while (*s != 0 && __insider_variable_index > 0) {\
    __insider_variable_index--;\
    *d = *s;\
    s++;\
    d++;\
  }\
\
  if (with_zero) *d = 0;\
}

#define MEMCPY(src, dest, size) {\
  char * s = (char *)src;\
  char * d = (char *)dest;\
  while (size > 0) {\
    *d = *s;\
    d++;\
    s++;\
  }\
}


#endif
