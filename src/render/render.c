#include "rendering.h"
#include "../macros.h"
#include "../core/core.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define MDARGS_T source_code_t code, gen_config_t config, struct result * result, char * buffer
#define MDARGS_V code, config, result, buffer

#define MDDUMP(...) {\
  size_t bytes_formatted = snprintf(buffer, BUFSIZ, __VA_ARGS__);\
  write(fd, buffer, bytes_formatted);\
}
#define PREPARE int fd;\
  char pathbuf[PATH_MAX];

#define OPEN(var, path, result) {\
  mkdirp(path);\
  if ((var = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP)) == -1) {\
    perror("open markdown");\
    result->fails++;\
    return;\
  }\
}
#define PATHNAME(into, config, ...) {\
  size_t written = snprintf(into, PATH_MAX, "%s", config->dest);\
  snprintf(into + written, PATH_MAX - written - 1, "/" __VA_ARGS__);\
}


typedef enum {
  LocationPrimitive = 0,
  LocationHeader = 1,
  LocationSource = 2,
} TypeLocation;
#define PTR_COMPARISON(keyword, structure, ptr) ((code->definitions.keyword.array <= (struct structure *)ptr) && ((struct structure *)ptr < code->definitions.keyword.array + code->definitions.keyword.count))
static TypeLocation type_location(source_code_t code, entity_type_t type) {
  if ((type.type == TypedefinedFunction || type.type == TypedefinedEnum || type.type == TypedefinedStructure || type.type == PrimitiveType) &&
      code->primitives.array <= (struct primitive_spec *)type.ptr && (struct primitive_spec *)type.ptr < code->primitives.array + code->primitives.count) return LocationPrimitive;
  if (type.type == TypedefType && PTR_COMPARISON(typedefs, typedef_spec, type.ptr)) return LocationHeader;
  if (type.type == FunctionType && PTR_COMPARISON(functions, function_spec, type.ptr)) return LocationHeader;
  if (PTR_COMPARISON(variables, variable, type.ptr)) return LocationHeader;
  if (type.type == EnumeratorType && PTR_COMPARISON(enums, enum_spec, type.ptr)) return LocationHeader;
  if (type.type == StructureType && PTR_COMPARISON(structs, struct_spec, type.ptr)) return LocationHeader;
  if (type.type == MacroType && PTR_COMPARISON(macros, macro_spec, type.ptr)) return LocationHeader;

  return LocationSource; // Assume the rest is in source
}
#undef PTR_COMPARISON

static void get_type_location(entity_type_t type, source_code_t code, char * writebuffer) {
  TypeLocation location = type_location(code, type);
  size_t index;
  char * name;
  if (location == LocationPrimitive) {
    index = sprintf(writebuffer, "primitives.md#primitive-");
    name =((struct primitive_spec *)type.ptr)->name;
  } else {
    char * locationstr = location == LocationHeader ? "headers" : "source";
    char * designation;
    switch (type.type) {
      case TypedefinedEnum:
      case TypedefinedFunction:
      case TypedefinedStructure:
        // These cases should never happen. We will put a string nontherless
        // That's because this is a type for 
        name = "";
        designation = "";
        break;
      case FunctionType:
        name = ((struct function_spec *)type.ptr)->name;
        designation = "function";
        break;
      case TypedefType:
        name = ((struct typedef_spec *)type.ptr)->name;
        designation = "typedef";
        break;
      case StructureType:
        name = ((struct struct_spec *)type.ptr)->name;
        designation = "structure";
        break;
      case PrimitiveType:
        name = ((struct primitive_spec *)type.ptr)->name;
        designation = "primitive";
        break;
      case EnumeratorType:
        name = ((struct enum_spec *)type.ptr)->name;
        designation = "enumerator";
        break;
      case MacroType:
        name = ((struct macro_spec *)type.ptr)->name;
        designation = "macro";
        break;
      case NoneType:
        // That shouldn't happen neither
        name = "N/A";
        designation = "na";
        break;
    }
    index = sprintf(writebuffer, "%s-%ss.md#%s-", locationstr, designation, designation);
  }
  size_t i = 0;
  while (name[i] != 0 && i < ENTITIES_NAME_LENGTH) {
    writebuffer[index + i] = name[i] == ' ' ? '-' : name[i];
    i++;
  }
  writebuffer[index + i] = 0;
}
static void dump_argument_type(int fd, char * buffer, entity_type_t * type) {
  struct primitive_spec * p;
  switch (type->type) {
    case NoneType:
      MDDUMP("%s", type->string);
      break;
    case EnumeratorType:
    case TypedefinedEnum:
      MDDUMP("enum %s", ((struct enum_spec *)type->ptr)->name);
      break;
    case TypedefinedStructure:
    case StructureType:
      MDDUMP("struct %s", ((struct struct_spec *)type->ptr)->name);
      break;
    case TypedefType:
      MDDUMP("%s", ((struct typedef_spec *)type->ptr)->name);
      break;
    case FunctionType:
    case TypedefinedFunction:
      // Todo
      break;
    case PrimitiveType:
      p = type->ptr;
      if (p->is_structure) MDDUMP("struct ");
      MDDUMP("%s", p->name);
      break;
    case MacroType:
      MDDUMP("%s", ((struct macro_spec *)type->ptr)->name);
      break;
  }

  ulint l = 0;
  if (type->pointers_level > 0) MDDUMP(" ");
  while (l < type->pointers_level) {
    MDDUMP("*");
    l++;
  }

  l = 0;
  while (l < type->arrays.count) {
    MDDUMP(" [");
    if (type->arrays.array[l] != NULL) MDDUMP("%s", type->arrays.array[l]);
    MDDUMP("]");
    l++;
  }
}
static void generate_path_pair(const char * filename, gen_config_t config, char * displaybuffer, char * linkbuffer) {
  if (config->rebase_prefix == NULL) {
    STRCP((char *)filename, displaybuffer, PATH_MAX, 1);
    STRCP((char *)filename, linkbuffer, PATH_MAX, 1);
  } else {
    rebase_path(filename, config->srcdirname, "", displaybuffer, PATH_MAX);
    rebase_path(filename, config->srcdirname, config->rebase_prefix, linkbuffer, PATH_MAX);
  }
}
static void generate_functions_page(struct function_indic * functions, source_code_t code, gen_config_t config, struct result * result, char * buffer, int is_header) {
  PREPARE;
  PATHNAME(pathbuf, config, "%s-functions.md", is_header ? "headers" : "source");
  OPEN(fd, pathbuf, result);

  MDDUMP("# %s functions\n\n## Defined in this file\n\nThe following **%zu** functions are referenced here\n\n", is_header ? "Header" : "Source", functions->count);

  ulint j = 0;
  while (j < functions->count) {
    MDDUMP("- [`%s`](#function-%s)\n", functions->array[j].name, functions->array[j].name);
    j++;
  }

  j = 0;
  while (j < functions->count) {
    struct function_spec * fnt = functions->array + j;
    MDDUMP("\n### function %s\n\n```c\n", fnt->name);
    dump_argument_type(fd, buffer, &(fnt->returntype));
    MDDUMP(" %s(", fnt->name);

    ulint x = 0;
    while (x < fnt->args->count) {
      dump_argument_type(fd, buffer, &(fnt->args->array[x].type));
      MDDUMP(" %s%s", fnt->args->array[x].name, x + 1 < fnt->args->count ? ", " : ");");
      x++;
    }

    char displaypath[PATH_MAX];
    char linkpath[PATH_MAX];
    generate_path_pair(fnt->ref->filename, config, displaypath, linkpath);
    if (config->gitline) MDDUMP("\n```\nDefined in [%s:%zu](%s#L%zu)\n", displaypath, fnt->ref->line, linkpath, fnt->ref->line)
    else MDDUMP("\n```\nDefined in [%s:%zu](%s)\n", displaypath, fnt->ref->line, linkpath);

    if (fnt->description != NULL) MDDUMP("#### Note\n\n```\n%s```\n", fnt->description);

    MDDUMP("#### Return type\n\n[`");
    dump_argument_type(fd, buffer, &(fnt->returntype));
    // Recycling displaypath
    get_type_location(fnt->returntype, code, displaypath);
    MDDUMP("`](%s)\n#### Arguments\n\n%s", displaypath, fnt->args->count == 0 ? "*No arguments*" : "");

    x = 0;
    while (x < fnt->args->count) {
      MDDUMP("%zu. **%s** : [`", x + 1, fnt->args->array[x].name);
      dump_argument_type(fd, buffer, &(fnt->args->array[x].type));
      get_type_location(fnt->args->array[x].type, code, displaypath);
      MDDUMP("`](%s)\n", displaypath);
      x++;
    }
    j++;
  }


  close(fd);
}
static void write_primitive_ref(struct primitive_spec * primitive, char * output) {
  ulint i = 0;
  while (i < ENTITIES_NAME_LENGTH && primitive->name[i] != 0) {
    if (primitive->name[i] != ' ') output[i] = primitive->name[i];
    else output[i] = '-';
    i++;
  }
  output[i] = 0;
}
#define PRIMITIVE_DUMP(p, TEST_MACRO, text) if (TEST_MACRO(p->source)) MDDUMP("- " text "");
static void generate_primitives(MDARGS_T, struct primitives_indic * primitives) {
  PREPARE;
  PATHNAME(pathbuf, config, "primitives.md");
  OPEN(fd, pathbuf, result);

  MDDUMP("# Primitives\nThis file contains all the used primitives in the project\n\n## Defined in this file\nThe following **%zu** primitives are used\n", primitives->count);

  char namebuf[ENTITIES_NAME_LENGTH + 1] = {0};
  ulint x = 0;
  while (x < primitives->count) {
    write_primitive_ref(primitives->array + x, namebuf);
    MDDUMP("- [`%s`](#primitive-%s)\n", primitives->array[x].name, namebuf);
    x++;
  }

  x = 0;
  while (x < primitives->count) {
    struct primitive_spec * primitive = primitives->array + x;
    MDDUMP("### primitive %s\n\n```c\n%s%s\n```\nComes from :\n", primitive->name, primitive->is_structure ? "struct " : "", primitive->name);

    PRIMITIVE_DUMP(primitive, P_TGLIBC, "GLibC");  
    PRIMITIVE_DUMP(primitive, P_TSTDINT, "stdint.h");
    PRIMITIVE_DUMP(primitive, P_TFCNTL, "fcntl.h");
    PRIMITIVE_DUMP(primitive, P_TDIRENT, "dirent.h");
    PRIMITIVE_DUMP(primitive, P_TLIBSTRINGPGREP, "stringpgrep.h");
    PRIMITIVE_DUMP(primitive, P_TSTDBOOL, "stdbool.h");
    PRIMITIVE_DUMP(primitive, P_TSTDDEF, "stddef.h");
    PRIMITIVE_DUMP(primitive, P_TSTDIO, "stdio.h");
    PRIMITIVE_DUMP(primitive, P_TSTDLIB, "stdlib.h");
    PRIMITIVE_DUMP(primitive, P_TSTDTIME, "time.h");
    PRIMITIVE_DUMP(primitive, P_TSYSDIR, "sys/dir.h");
    PRIMITIVE_DUMP(primitive, P_TSYSSTAT, "sys/stat.h");
    PRIMITIVE_DUMP(primitive, P_TSYSTYPES, "sys/types.h");
    PRIMITIVE_DUMP(primitive, P_TUNISTD, "unistd.h");

    MDDUMP("\n");
    x++;
  }

  close(fd);
}
#undef PRIMITIVE_DUMP
static void generate_structures_page(MDARGS_T, struct struct_indic * structures, int is_header) {
  PREPARE;
  PATHNAME(pathbuf, config, "%s-structures.md", is_header ? "headers" : "source");
  OPEN(fd, pathbuf, result);

  MDDUMP("# %s structures\n\nThis file contains all the declared structures for the %s\n\n## Defined in this file\n\nThe following **%zu** structures are referenced here\n\n", is_header ? "Headers" : "Source", is_header ? "headers files" : "source files", structures->count);

  ulint i = 0;
  while (i < structures->count) {
    struct struct_spec * strc = structures->array + i;
    MDDUMP("- [`%s`](#structure-%s)\n", strc->name, strc->name);

    i++;
  }

  char displaybuffer[PATH_MAX];
  char linkbuffer[PATH_MAX];
  i = 0;
  while (i < structures->count) {
    struct struct_spec * strc = structures->array + i;
    MDDUMP("### Structure %s\n\n```c\nstruct %s %s\n", strc->name, strc->name, strc->members->count > 0 ? "{" : "{};");

    ulint j = 0;
    while (j < strc->members->count) {
      struct struct_member * member = strc->members->array + j;
      MDDUMP("    ");
      dump_argument_type(fd, buffer, &(member->type));
      MDDUMP(" %s;\n", member->name);
      j++;
    }

    generate_path_pair(strc->ref->filename, config, displaybuffer, linkbuffer);
    if (config->gitline) MDDUMP("%s```\nDefined in [%s:%zu](%s#L%zu)\n", strc->members->count > 0 ? "};\n" : "", displaybuffer, strc->ref->line, linkbuffer, strc->ref->line)
    else MDDUMP("%s```\nDefined in [%s:%zu](%s)\n", strc->members->count > 0 ? "};\n" : "}", displaybuffer, strc->ref->line, linkbuffer);

    if (strc->members->count > 0) {
      MDDUMP("\n\n#### Members\n");
      j = 0;
      while (j < strc->members->count) {
        struct struct_member * member = strc->members->array + j;
        get_type_location(member->type, code, displaybuffer);
        MDDUMP("- **%s** : [`", member->name);
        dump_argument_type(fd, buffer, &(member->type));
        MDDUMP("`](%s)\n", displaybuffer);

        j++;
      }
    }
    i++;
  }

  close(fd);
}

static void generate_homepage(MDARGS_T) {
  PREPARE;
  PATHNAME(pathbuf, config, "Home.md");
  OPEN(fd, pathbuf, result);

  MDDUMP("# Homepage\n\n## Summary\n\nThis documentation is split between the informations found in header files and source files.\n| Headers | Implementation |\n|:---:|:---:|\n");
  MDDUMP("| [`functions (%zu)`](headers-functions.md) | [`Functions (%zu)`](source-functions.md) |\n| [`structures (%zu)`](headers-structures.md) | [`structures (%zu)`](source-structures.md)\n> See also the [`%zu primitives`](primitives.md)",
      code->definitions.functions.count,
      code->implementations.functions.count,
      code->definitions.structs.count,
      code->implementations.structs.count,
      code->primitives.count
  );

  close(fd);
}

/**
 * Returns 1 if failed 0 otherwise
 */
int render(source_code_t code, gen_config_t config, struct result * result) {
  size_t before = result->fails;
  char buffer[BUFSIZ];
  generate_homepage(MDARGS_V);

  generate_functions_page(&(code->definitions.functions), code, config, result, buffer, 1);
  generate_functions_page(&(code->implementations.functions), code, config, result, buffer, 0);
  generate_structures_page(MDARGS_V, &(code->definitions.structs), 1);
  generate_structures_page(MDARGS_V, &(code->implementations.structs), 0);
  generate_primitives(MDARGS_V, &(code->primitives));
  return result->fails > before;
}

#undef MDDUMP
#undef MDARGS_T
#undef MDARGS_V
#undef OPEN
#undef PREPARE
#undef PATHNAME
