#include "macros.h"
#include "reader/reader.h"
#include "core/core.h"
#include "parsers/args.h"

#include <stdio.h>

#define ARG_MISSING(arg) {\
  fprintf(stderr, "\x1b[33m%s\x1b[0m is missing, specify it using : \x1b[90m%s", (arg).name, (arg).name);\
  switch ((arg).type) {\
    case String:\
      printf(" \"some value\"");\
      break;\
    case Int:\
      printf(" 2");\
      break;\
    case Presence:\
      break;\
  }\
  printf("\x1b[0m\n");\
  return 1;\
}

int main(int argc, char * argv[]) {
  struct arg_input args[] = {
    { "--src", "Source directory", String, 0, 0, NULL },
    { "--dest", "Destination directory", String, 0, 0, NULL },
    { "--version", "Displays version and exits", Presence, 0, 0, NULL },
    { "--help", "Displays help and exits", Presence, 0, 0, NULL }
  };
  int args_size = sizeof(args) / sizeof(struct arg_input);

  find_all(argc, argv, args, args_size);

  if (args[3].found) {
    display_args(args_size, args);
    return 0;
  }
  if (args[2].found) {
    printf(PROG_VERSION "\n");
    return 0;
  }

  if (!args[0].found) ARG_MISSING(args[0]);
  if (!args[1].found) ARG_MISSING(args[1]);

  struct gen_config config;
  if (create_config(args[0].str_result, args[1].str_result, &config) != 0) {
    fprintf(stderr, "Failed to parse config\n");
    return 1;
  }

  struct source_code source_code = {};
  recursion(config.src, &(struct callback_args_s){
      .config = &config,
      .source_code = &source_code,
      .results = (struct result){
        .success = 0,
        .fails = 0
      }
  }, recursion_callback);
}
