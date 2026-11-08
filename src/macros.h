#ifndef __MACROS_H__
#define __MACROS_H__

#define PROG_VERSION "0.0.1"

#define STRCP(src, dest, max, with_zero) {\
  char * s = src;\
  char * d = dest;\
  unsigned long i = max;\
\
  while (*s != 0 && i > 0) {\
    i--;\
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
