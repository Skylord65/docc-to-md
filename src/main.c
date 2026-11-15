#include "macros.h"
#include "reader/reader.h"
#include "core/core.h"
#include "parsers/args.h"
#include "render/rendering.h"

#include <fcntl.h>
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

void dump(source_code_t code, struct arg_input enable, struct arg_input path) {
  if (!enable.found) return;
  if (!path.found || path.str_result == NULL) {
    fprintf(stderr, "Cannot dump because no file name is specified\n");
    return;
  }

  int fd;
  if ((fd = open(path.str_result, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP)) == -1) {
    perror("open dump file");
    return;
  }

  dump_source_code(code, fd);

  close(fd);
}
int main(int argc, char * argv[]) {
  struct arg_input args[] = {
    { "--src", "Source directory", String, 0, 0, NULL },
    { "--dest", "Destination directory", String, 0, 0, NULL },
    { "--work", "Work directory", String, 0, 0, "/tmp/docc-to-md" },
    { "--version", "Displays version and exits", Presence, 0, 0, NULL },
    { "--help", "Displays help and exits", Presence, 0, 0, NULL },
    { "-d", "Dump parsed source code to a file. Needs --dump to specify file name", Presence, 0, 0, NULL },
    { "--dump", "Specify the file name for the parsed code", String, 0, 0, NULL },
    { "--verbose-registration", "Enable logs in standard output for registration of elements when parsing", Presence, 0, 0, NULL },
    { "--enable-docstring", "Enable the registration of docstring for various types", Presence, 0, 0, NULL },
    { "--rebase-path", "Some text to append before the name of the paths. If not given, absolute path will remain. If given, the display name will be rebased and show from root of project, and link will be precedeed by this", String, 0, 0, NULL },
    { "--git-line", "Add the matching line like git would do in file reference", Presence, 0, 0, NULL },
    { "--reference-files", "Add a section of referenced files in the produced documentation", Presence, 0, 0, NULL },
    { "--pre-linkage", "Enable the pre-linkage step. Enhance the produced documentation", Presence, 0, 0, NULL }
  };
  int args_size = sizeof(args) / sizeof(struct arg_input);

  find_all(argc, argv, args, args_size);

  if (args[4].found) {
    display_args(args_size, args);
    return 0;
  }
  if (args[3].found) {
    printf(PROG_VERSION "\n");
    return 0;
  }

  if (!args[0].found) ARG_MISSING(args[0]);
  if (!args[1].found) ARG_MISSING(args[1]);

  struct gen_config config;
  if (create_config(args[0].str_result, args[1].str_result, args[2].str_result, args[9].str_result, &config) != 0) {
    fprintf(stderr, "Failed to parse config\n");
    return 1;
  }
  config.verbose_registration = args[7].found;
  config.gitline = args[10].found;
  config.reference_files_section = args[11].found;
  config.prelinkage = args[12].found;

  struct callback_args_s callback_args = {
    .config = &config,
    .source_code = &(struct source_code){0},
    .results = (struct result){
      .success = 0,
      .fails = 0,
      .links = 0
    }
  };
  init_source_code(callback_args.source_code);
  recursion(config.src, &callback_args, remove_comments_callback);
  if (callback_args.results.fails > 0) {
    fprintf(stderr, "Failed to remove comments flawlessly.\n");
    cleanup(&config);
    return 1;
  }
  recursion(config.work, &callback_args, recursion_callback);
  if (args[8].found) recursion(config.src, &callback_args, register_docstring_callback);
  if (callback_args.results.fails > 0) {
    fprintf(stderr, "Failed to parse content (%zu) errors\n", callback_args.results.fails);
    cleanup(&config);
    free_source_code(callback_args.source_code);
    return 1;
  }
  if (args[7].found) summarise_parsing(callback_args.source_code);
  if (config.prelinkage && prelinkage(callback_args.source_code, &callback_args) != 0) {
    fprintf(stderr, "Failed the prelinkage phase\n");
    cleanup(&config);
    free_source_code(callback_args.source_code);
    return 1;
  }
  if (linkage(callback_args.source_code, &callback_args) != 0) {}
  summarise_linking(&callback_args);

  dump(callback_args.source_code, args[5], args[6]);

  if (render(callback_args.source_code, &config, &(callback_args.results)) != 0) {
    fprintf(stderr, "Failed to generate documentation\n");
    cleanup(&config);
    free_source_code(callback_args.source_code);
    return 1;
  }

  cleanup(&config);
  free_source_code(callback_args.source_code);
  return 0;
}
