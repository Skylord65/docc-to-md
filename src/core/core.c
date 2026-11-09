#include "core.h"
#include "../macros.h"
#include "../parsers/stringlib.h"

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
          if (buffer[index] == '\n') {
            writebuffer[write_count] = '\n';
            write_count++;
          }
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
    .reading_multiline_struct = 0,
    .reading_multiline_enum = 0,
    .reading_multiline_typedef = 0,
    .line_count = 1,
    .data = NULL,
  };

  char buffer[LINE_BUFFER_SIZE];
  errno = 0;
  while (fgets(buffer, LINE_BUFFER_SIZE, stream) != NULL) {
    if (is_line_empty(buffer)) goto last_bit;
    else if (status.reading_multiline_macro) try_extract_macro_multiline(buffer, src, params, &status);
    else if (status.reading_multiline_struct) try_extract_struct_multiline(buffer, src, params, &status);
    else if (status.reading_function_params) try_extract_function_multiline(buffer, src, params, &status);
    else if (status.reading_multiline_enum) try_extract_enum_multiline(buffer, src, params, &status);
    else if (status.reading_multiline_typedef) try_extract_typedef_multiline(buffer, src, params, &status);
    else {
      if (try_extract_macro(buffer, src, params, &status) == 0) goto last_bit;
      if (try_extract_struct(buffer, src, params, &status) == 0) goto last_bit;
      if (try_extract_function(buffer, src, params, &status) == 0) goto last_bit;
      if (try_extract_typedef(buffer, src, params, &status) == 0) goto last_bit;
      if (try_extract_enum(buffer, src, params, &status) == 0) goto last_bit;
      if (status.is_header && try_extract_variable(buffer, src, params, &status) == 0) goto last_bit;
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

static int rmdirrec(const char * path) {
  DIR * dir;
  if ((dir = opendir(path)) == NULL) {
    perror("opendir rmdirrec");
    return errno;
  }

  struct dirent * entry;
  struct stat sb;

  errno = 0;
  while ((entry = readdir(dir)) != NULL) {
    if (IS_SYS_PATH(entry->d_name)) continue;

    char * buffer;
    if ((buffer = malloc(PATH_MAX)) == NULL) {
      perror("malloc");
      goto fail;
    }

    snprintf(buffer, PATH_MAX, "%s/%s", path, entry->d_name);
    if (stat(buffer, &sb) == -1) {
      perror("stat");
      free(buffer);
      goto fail;
    }

    if (S_ISDIR(sb.st_mode)) {
      int code;
      if ((code = rmdirrec(buffer)) != 0) {
        errno = code;
        free(buffer);
        goto fail;
      }
    free(buffer);
    } else {
      int code = remove(buffer);
      free(buffer);
      if (code != 0) goto fail;
    }
  }
  if (errno != 0) {
    perror("readdir");
fail:
    closedir(dir);
    return errno;
  }

  closedir(dir);
  return rmdir(path);
}
#undef IS_SYS_PATH

int rmdirrf(const char * path) {
  return rmdirrec(path);
}
int create_config(const char * src, const char * dst, const char * work, gen_config_t configp) {
  if (realpath(src, configp->src) == NULL) {
    perror("realpath");
    return 1;
  }
  if (mkdirp(dst) != 0) {
    perror("mkdirp destination");
    return 1;
  }
  if (realpath(dst, configp->dest) == NULL) {
    perror("realpath");
    return 1;
  }
  if (rmdirrf(configp->dest) != 0) {
    perror("rm -rf dest");
    return 1;
  }
  if (mkdirp(configp->dest) != 0) {
    perror("mkdir -p confip->dest");
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
  if (rmdirrf(configp->work) == -1) {
    perror("rm work dir for purge");
    return 1;
  }
  if (mkdirp(configp->work) != 0) {
    perror("mkdirp work back");
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

void register_doccumments_callback(const char * src, callback_args params) {
  FILE * stream;
  if ((stream = fopen(src, "rb")) == NULL) {
    perror("fopen");
    params->results.fails++;
    return;
  }
  char buffer[LINE_BUFFER_SIZE];
  int is_headerfile = is_header(src);
  int in_string = 0;
  int in_char = 0;

  int in_multiline_comment = 0;
  long multiline_start = 0;
  unsigned long linecount = 0;

  errno = 0;
  while ((fgets(buffer, LINE_BUFFER_SIZE, stream)) != NULL) {
    linecount++;
    char * ptr = buffer;
    while (*ptr == ' ') ptr++;

    if (!in_multiline_comment) {
      if (IS_STRING_DEL(buffer, ptr) && !in_char) in_string = !in_string;
      else if (IS_CHAR_DEL(buffer, ptr) && !in_string) in_char = !in_char;
      else {
        if (*ptr == '/' && *(ptr + 1) == '*') {
          in_multiline_comment = 1;
          multiline_start = ftell(stream) - strlen(buffer);
          continue;
        }
      }
    } else {
      if (*ptr != '*') { // This is not a doccomment, abort mission
        in_multiline_comment = 0;
        continue;
      }
      if (*(ptr + 1) == '/') {
        in_multiline_comment = 0;
        // Now look if there is any function that is registered here or 
        struct function_indic * indicator = is_headerfile ? &(params->source_code->definitions.functions) : &(params->source_code->implementations.functions);
        struct function_spec * function = NULL;
        ulint i = 0;
        while (i < indicator->count) {
          if ((indicator->array[i].ref->line == linecount || indicator->array[i].ref->line == linecount + 1) && streq(indicator->array[i].ref->filename, (char *)src)) {// We do have a function (on this line or on another one), mark it, and proceed
            function = indicator->array + i;
            break;
          }
          i++;
        }

        if (function == NULL) continue;
        long jump_back = ftell(stream);

        fseek(stream, multiline_start, SEEK_SET);
        unsigned long int description_index = 0;
        size_t description_size = 1024;

        if ((function->description = malloc(description_size)) == NULL) {
          perror("malloc function->description");
          params->results.fails++;
          fclose(stream);
          return;
        }

        errno = 0;
        while (fgets(buffer, LINE_BUFFER_SIZE, stream) != NULL && multiline_start < jump_back) {
          char * bufptr = buffer;
          int started = 0;
          while (*bufptr != 0) {
            if (description_index == description_size - 1) {
              if ((function->description = realloc(function->description, description_size * 2)) == NULL) {
                perror("realloc function->description");
                params->results.fails++;
                free(function->description);
                return;
              }
            }
            if (!started) {
              if (*bufptr != ' ' && *bufptr != '*' && *bufptr != '/') {
                started = 1;
                function->description[description_index] = *bufptr;
                description_index++;
              }
            } else {
              if (*(bufptr - 1) != '*' || *bufptr != '/') {
                function->description[description_index] = *bufptr;
                description_index++;
              }
            }
            multiline_start++;
            bufptr++;
          }
        }
        if (errno != 0) {
          perror("fgets description");
          free(function->description);
        }
        if (description_index > 0 && function->description[description_index - 1] == '\n') function->description[description_index] = 0;
        else function->description[description_index] = 0;

        // Health check on the description : if we have only spaces and newlines, we can remove it
        description_index = 0;
        int healthy = 0;
        while (function->description[description_index] != 0 && !healthy) {
          if (function->description[description_index] != ' ' && function->description[description_index] != '\n') healthy = 1;
          description_index++;
        }
        if (!healthy) {
          free(function->description);
          function->description = NULL;
        }
        if (params->config->verbose_registration) printf("- Registering comment doc for function %s\n", function->name);
        linecount++; // Just to make things clear : I have absolutely no idea why I need to increment this but I know I need to. That's how it is.
        continue;
      }
    }
  }
  if (errno != 0) {
    perror("fgets");
    params->results.fails++;
    fclose(stream);
    return;
  }

  fclose(stream);
}

void cleanup(gen_config_t config) {
  rmdirrf(config->work);
}
