#ifndef COMPAT_WIN32_LIBGEN_H
#define COMPAT_WIN32_LIBGEN_H
#include <string.h>

static inline int uwu_is_sep(char c) { return c == '/' || c == '\\'; }

static inline char *basename(char *path) {
  static char dot[] = ".";
  if (!path || !*path) return dot;
  size_t len = strlen(path);
  while (len > 1 && uwu_is_sep(path[len - 1])) path[--len] = '\0';
  if (len == 1 && uwu_is_sep(path[0])) return path;
  char *p = path + len;
  while (p > path && !uwu_is_sep(p[-1])) p--;
  return p;
}

static inline char *dirname(char *path) {
  static char dot[] = ".";
  if (!path || !*path) return dot;
  size_t len = strlen(path);
  while (len > 1 && uwu_is_sep(path[len - 1])) len--;
  while (len > 0 && !uwu_is_sep(path[len - 1])) len--;
  if (len == 0) return dot;
  while (len > 1 && uwu_is_sep(path[len - 1])) len--;
  path[len] = '\0';
  return path;
}
#endif