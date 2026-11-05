#include "core.h"

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <sys/stat.h>

#define IS_SYS_PATH(str) (*str != 0 && ((*str == '.' && *(str + 1) == 0) || (*str == '.' && *(str + 1) == '.' && *(str + 2) == 0)))
#define LINE_BUFFER_SIZE 2048

static void concat(char * target, const char * a, const char * b) {
  while (*a != 0) {
    *target = *a;
    target++;
    a++;
  }
  *target = '/';
  target++;
  while (*b != 0) {
    *target = *b;
    target++;
    b++;
  }
  *target = 0;
}

static int is_header(const char * name) {
  const char * last_dot = NULL;
  while (*name != 0) {
    if (*name == '.') last_dot = name;
    name++;
  }

  return last_dot != NULL && *(last_dot + 1) == 'h' && *(last_dot + 2) == 0;
}

static int is_line_empty(const char * line) {
  while (*line == ' ') line++;
  return *line == 0 || *line == '\n';
}

void recursion_callback(const char * src, callback_args params) {
  FILE * stream;
  if ((stream = fopen(src, "rb")) == NULL) {
    perror("fopen");
    params->results.fails++;
    return;
  }

  struct status_s status = {
    .is_header = is_header(src),
    .in_function = 0,
    .reading_function_params = 0
  };

  char buffer[LINE_BUFFER_SIZE];
  errno = 0;
  while (fgets(buffer, LINE_BUFFER_SIZE, stream) != NULL) {
    if (is_line_empty(buffer)) continue;
    if (buffer[0] == '/' && buffer[1] == '/') {
      status.line_count++;
      continue;
    }

    if (status.is_header) { // We can try to read variables
      if (try_extract_variable(buffer, src, params, &status) == 0) goto last_bit;
    }

last_bit:
    status.line_count++;
  }
  if (errno != 0) {
    fprintf(stderr, "Something went wrong during reading of a line\n");
    params->results.fails++;
  } else {
    params->results.success++;
  }

  fclose(stream);
};
recursion_return_t recursion(const char * input, callback_args params, recursion_callback_t callback) {
  DIR * dir;
  if ((dir = opendir(input)) == NULL) {
    perror("opendir");
    return UnknownPath;
  }

  struct dirent * entry;
  struct stat sb;
  errno = 0;
  while ((entry = readdir(dir)) != NULL) {
    if (IS_SYS_PATH(entry->d_name)) continue;

    char * newpath;
    if ((newpath = malloc(strlen(entry->d_name) + strlen(input) + 2)) == NULL) {
      perror("malloc");
      errno = 0;
      continue;
    }
    concat(newpath, input, entry->d_name);

    if (stat(newpath, &sb) == -1) {
      perror("stat");
      free(newpath);
      errno = 0;
      continue;
    }

    if (S_ISDIR(sb.st_mode)) {
      recursion(newpath, params, callback);
    } else if (S_ISREG(sb.st_mode)) {
      callback(newpath, params);
    }

    free(newpath);
  }
  if (errno != 0) {
    perror("readdir");
    return ReadError;
  }

  closedir(dir);
  return Ok;
}

#undef IS_SYS_PATH

int create_config(const char * src, const char * dst, gen_config_t configp) {
  if (realpath(src, configp->src) == NULL) {
    perror("realpath");
    return 1;
  }
  if (realpath(src, configp->dest) == NULL) {
    perror("realpath");
    return 1;
  }
  return 0;
}
