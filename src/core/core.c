#include "core.h"
#include "../macros.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
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

static void calculate_work_filename(const char * src, const callback_args params, char * buffer) {
  unsigned i = 0;
  unsigned bufindex = 0;
  while (src[i] == params->config->src[i] && i < PATH_MAX) i++;

  while (params->config->work[bufindex] != 0 && bufindex < PATH_MAX) {
    buffer[bufindex] = params->config->work[bufindex];
    bufindex++;
  }

  while (src[i] != 0 && i < PATH_MAX) {
    buffer[bufindex] = src[i];
    bufindex++;
    i++;
  }
  buffer[bufindex] = 0;
}
void remove_comments_callback(const char * src, callback_args params) {
  int fd;
  int outfd;
  char outbuf[PATH_MAX];
  calculate_work_filename(src, params, outbuf);
  if (mkdirp(outbuf) != 0) {
    perror("mkdirp for comments");
    params->results.fails++;
    return;
  }

  if ((fd = open(src, O_RDONLY)) == -1) {
    perror("open");
    params->results.fails++;
    return;
  }
  if ((outfd = open(outbuf, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
    perror("open output file for comments");
    params->results.fails++;
    close(fd);
    return;
  }

  int in_multine_comment = 0;
  int ignore_rest_of_line = 0;

  char buffer[BUFSIZ];
  char writebuffer[BUFSIZ];

  unsigned int index = 0;
  unsigned int write_count = 0;
  ssize_t rbytes;
  while ((rbytes = read(fd, buffer, BUFSIZ)) > 0) {
    index = 0;
    write_count = 0;

    while (index < rbytes) {
      if (ignore_rest_of_line == 1) {
        if (buffer[index] == '\n') {
          ignore_rest_of_line = 0;
          writebuffer[write_count] = '\n';
          write_count++;
          index++;
          continue;
        } else {
          index++;
          continue;
        }
      }
      if (in_multine_comment) {
        if (buffer[index] == '*' && buffer[index + 1] == '/') {
          in_multine_comment = 0;
          index+=2;
          continue;
        } else {
          index++;
          continue;
        }
      }

      if (buffer[index] == '/') {
        if (buffer[index + 1] == '/') {
          ignore_rest_of_line = 1;
          index += 2;
          continue;
        }
        if (buffer[index + 1] == '*') {
          in_multine_comment = 1;
          index += 2;
          continue;
        }
      }

      writebuffer[write_count] = buffer[index];
      write_count++;
      index++;
    }

    if (write_count > 0) {
      if (write(outfd, writebuffer, write_count) == -1) {
        perror("write for comments");
        params->results.fails++;
        goto close;
      }
    }
  }
  if (rbytes == -1) {
    perror("read for comments");
    params->results.fails++;
    goto close;
  }

close:
  close(fd);
  close(outfd);
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
    .reading_function_params = 0,
    .reading_multiline_macro = 0,
    .reading_multiline_struct = 0
  };

  char buffer[LINE_BUFFER_SIZE];
  errno = 0;
  while (fgets(buffer, LINE_BUFFER_SIZE, stream) != NULL) {
    if (is_line_empty(buffer)) continue;

    if (status.reading_multiline_macro) {
      try_extract_macro_multiline(buffer, params, &status);
      goto last_bit;
    }

    if (!status.reading_multiline_struct || !status.reading_multiline_macro || !status.reading_function_params) {
      if (status.is_header) {
        if (try_extract_variable(buffer, src, params, &status) == 0) goto last_bit;
      }
      if (try_extract_macro(buffer, src, params, &status) == 0) goto last_bit;
      if (try_extract_struct(buffer, src, params, &status) == 0) goto last_bit;
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
}
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
    closedir(dir);
    return ReadError;
  }

  closedir(dir);
  return Ok;
}

#undef IS_SYS_PATH

int create_config(const char * src, const char * dst, const char * work, gen_config_t configp) {
  if (realpath(src, configp->src) == NULL) {
    perror("realpath");
    return 1;
  }
  if (realpath(dst, configp->dest) == NULL) {
    perror("realpath");
    return 1;
  }
  STRCP((char *)work, configp->work, PATH_MAX, 1);
  if (mkdirp(configp->work) != 0) {
    perror("mkdirp");
    return 1;
  }
  if (realpath(work, configp->work) == NULL) {
    perror("realpath");
    return 1;
  }
  return 0;
}
int mkdirp(const char * path) {
  char buffer_[PATH_MAX];
  char * buffer = buffer_;

  STRCP((char *)path, buffer, PATH_MAX, 1);

  unsigned i = 0;
  char * word = buffer;
  char * last_slash = buffer;
  while (*buffer != 0 && i < PATH_MAX) {
    if (*buffer == '/' && i > 0) {
      last_slash = buffer;
      *buffer = 0;

      if (mkdir(word, 0777) == -1) {
        if (errno != EEXIST) return errno;
      }

      *buffer = '/';
    }

    buffer++;
    i++;
  }

  char * dot = NULL;
  while (*last_slash != 0 && i < PATH_MAX) {
    if (*last_slash == '.') dot = last_slash;
    i++;
    last_slash++;
  }

  if (dot != NULL && *(dot + 1) != 0) return 0; // This is a file, ending in . something
  // It is not a file, create from word
  if (mkdir(word, 0777) == -1 && errno != EEXIST) return errno;
  return 0;
}

void cleanup(gen_config_t config) {
  rmdir(config->work);
}
