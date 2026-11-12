#include "rendering.h"
#include <stdio.h>
#include <unistd.h>

#define MDARGS_T source_code_t code, gen_config_t config, struct result * result, char * buffer
#define MDARGS_V code, config, result, buffer

#define MDDUMP(buffer, ...) {\
  size_t bytes_formatted = snprintf(buffer, BUFSIZ, __VA_ARGS__);\
  write(fd, buffer, bytes_formatted);\
}
#define PREPARE int fd;\
  char pathbuf[PATH_MAX];

#define OPEN(var, path, result) {\
  if ((var = open(path, O_WRONLY | O_CREAT | O_TRUNC)) == -1) {\
    perror("open markdown");\
    result->fails++;\
    return;\
  }\
}
#define PATHNAME(into, config, ...) {\
  size_t written = snprintf(into, PATH_MAX, "%s", config->dest);\
  snprintf(into + written, PATH_MAX - written - 1, "/" __VA_ARGS__);\
}

static void generate_homepage(MDARGS_T) {
  PREPARE;
}

int render(source_code_t code, gen_config_t config, struct result * result) {
  char buffer[BUFSIZ];
  generate_homepage(MDARGS_V);
  return 0;
}

#undef MDDUMP
#undef MDARGS_T
#undef MDARGS_V
#undef OPEN
#undef PREPARE
#undef PATHNAME
