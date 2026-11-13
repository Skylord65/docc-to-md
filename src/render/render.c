#include "rendering.h"
#include "../macros.h"
#include "../core/core.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
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
static void dump_function_type(int fd, char * buffer, struct function_spec * fnt) {
  dump_argument_type(fd, buffer, &(fnt->returntype));
  MDDUMP(" %s(", fnt->name);
  ulint x = 0;
  while (x < fnt->args->count) {
    dump_argument_type(fd, buffer, &(fnt->args->array[x].type));
    MDDUMP(" %s%s", fnt->args->array[x].name, x + 1 < fnt->args->count ? ", " : ")");
    x++;
  }
}
static void generate_functions_page(struct function_indic * functions, source_code_t code, gen_config_t config, struct result * result, char * buffer, int is_header) {
  PREPARE;
  PATHNAME(pathbuf, config, "%s-functions.md", is_header ? "headers" : "source");
  OPEN(fd, pathbuf, result);

  MDDUMP("# %s functions\n\n## Defined in this file\n\nThe following **%zu** functions are referenced here\n\n", is_header ? "Header" : "Source", functions->count);

  ulint j = 0;
  while (j < functions->count) {
    MDDUMP("- [%s](#function-%s)\n", functions->array[j].name, functions->array[j].name);
    j++;
  }

  j = 0;
  while (j < functions->count) {
    struct function_spec * fnt = functions->array + j;
    MDDUMP("\n### function %s\n\n```c\n", fnt->name);

    dump_function_type(fd, buffer, fnt);

    char displaypath[PATH_MAX];
    char linkpath[PATH_MAX];
    generate_path_pair(fnt->ref->filename, config, displaypath, linkpath);
    if (config->gitline) MDDUMP(";\n```\nDefined in [%s:%zu](%s#L%zu)\n", displaypath, fnt->ref->line, linkpath, fnt->ref->line)
    else MDDUMP(";\n```\nDefined in [%s:%zu](%s)\n", displaypath, fnt->ref->line, linkpath);

    if (fnt->description != NULL) MDDUMP("#### Note\n\n```\n%s```\n", fnt->description);

    MDDUMP("#### Return type\n\n[`");
    dump_argument_type(fd, buffer, &(fnt->returntype));
    // Recycling displaypath
    get_type_location(fnt->returntype, code, displaypath);
    MDDUMP("`](%s)\n#### Arguments\n\n%s", displaypath, fnt->args->count == 0 ? "*No arguments*" : "");

    ulint x = 0;
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
static void generate_primitives(gen_config_t config, struct result *result, char *buffer, struct primitives_indic *primitives) {
  PREPARE;
  PATHNAME(pathbuf, config, "primitives.md");
  OPEN(fd, pathbuf, result);

  MDDUMP("# Primitives\nThis file contains all the used primitives in the project\n\n## Defined in this file\nThe following **%zu** primitives are used\n", primitives->count);

  char namebuf[ENTITIES_NAME_LENGTH + 1] = {0};
  ulint x = 0;
  while (x < primitives->count) {
    write_primitive_ref(primitives->array + x, namebuf);
    MDDUMP("- [%s](#primitive-%s)\n", primitives->array[x].name, namebuf);
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
static void dump_structure_signature(int fd, char * buffer, struct struct_spec * strc, char dump_name) {
  if (dump_name == 1) MDDUMP("struct %s {%s", strc->name, strc->members->count > 0 ? "\n" : "")
  else MDDUMP("struct {%s", strc->members->count > 0 ? "\n" : "");

  ulint x = 0;
  while (x < strc->members->count) {
    struct struct_member * member = strc->members->array + x;
    MDDUMP("    ");
    dump_argument_type(fd, buffer, &(member->type));
    MDDUMP(";\n");
    x++;
  }
  MDDUMP("}");
}
static void generate_structures_page(MDARGS_T, struct struct_indic * structures, int is_header) {
  PREPARE;
  PATHNAME(pathbuf, config, "%s-structures.md", is_header ? "headers" : "source");
  OPEN(fd, pathbuf, result);

  MDDUMP("# %s structures\n\nThis file contains all the declared structures for the %s\n\n## Defined in this file\n\nThe following **%zu** structures are referenced here\n\n", is_header ? "Headers" : "Source", is_header ? "headers files" : "source files", structures->count);

  ulint i = 0;
  while (i < structures->count) {
    struct struct_spec * strc = structures->array + i;
    MDDUMP("- [%s](#structure-%s)\n", strc->name, strc->name);

    i++;
  }

  char displaybuffer[PATH_MAX];
  char linkbuffer[PATH_MAX];
  i = 0;
  while (i < structures->count) {
    struct struct_spec * strc = structures->array + i;
    MDDUMP("### Structure %s\n\n```c\n", strc->name);

    dump_structure_signature(fd, buffer, strc, 1);

    generate_path_pair(strc->ref->filename, config, displaybuffer, linkbuffer);
    if (config->gitline) MDDUMP(";\n```\nDefined in [%s:%zu](%s#L%zu)\n", displaybuffer, strc->ref->line, linkbuffer, strc->ref->line)
    else MDDUMP(";\n```\nDefined in [%s:%zu](%s)\n", displaybuffer, strc->ref->line, linkbuffer);

    if (strc->members->count > 0) {
      MDDUMP("\n\n#### Members\n");
      ulint j = 0;
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
static void dump_enumerator_signature(int fd, char * buffer, struct enum_spec * enumerator, const char dump_name) {
  MDDUMP("enum%s {%s", dump_name == 1 ? enumerator->name : "", enumerator->members->count > 0 ? "\n" : "");

  size_t max = 0;
  ulint x = 0;
  while (x < enumerator->members->count) {
    size_t len = strlen(enumerator->members->array[x].name);
    if (len > max) max = len;
    x++;
  }
  char template[32];
  sprintf(template, "    %%-%zus = %%s%%s\n", max);
  x = 0;
  while (x < enumerator->members->count) {
    MDDUMP(template, enumerator->members->array[x].name, enumerator->members->array[x].value, x + 1 < enumerator->members->count ? "," : "");
    x++;
  }

  MDDUMP("}");
}
static void dump_typedef_definition(struct typedef_spec * td, int fd, char * buffer) {
  if (td->definition.ptr == NULL) return;

  switch (td->definition.type) {
    case NoneType:
    case MacroType:
    case TypedefType: // If someone does typedef typedef I kill them
      break;
    case PrimitiveType:
      dump_argument_type(fd, buffer, &(td->definition));
      MDDUMP(" %s;", td->name);
      break;
    case TypedefinedFunction:
    case FunctionType:
      dump_function_type(fd, buffer, (struct function_spec *)td->definition.ptr);
      break;
    case TypedefinedEnum:
    case EnumeratorType:
      dump_enumerator_signature(fd, buffer, (struct enum_spec *)td->definition.ptr, 0);
      MDDUMP(" %s", td->name);
      break;
    case StructureType:
    case TypedefinedStructure:
      dump_structure_signature(fd, buffer, (struct struct_spec *)td->definition.ptr, 0);
      MDDUMP(" %s", td->name);
      break;
  }
}
static void generate_typedefs_page(MDARGS_T, struct typedef_indic * tds, int is_header) {
  PREPARE;
  PATHNAME(pathbuf, config, "%s-typedefs.md", is_header ? "headers" : "source");
  OPEN(fd, pathbuf, result);

  ulint i = 0;
  MDDUMP("# %s typedefs\n\nThis file contains the typedefs that are written in the %s files\n\n## Defined in this file\n\nThe following **%zu** typedefs are referenced here\n", is_header ? "Headers" : "source", is_header ? "headers" : "source", tds->count);

  while (i < tds->count) {
    struct typedef_spec * td = tds->array + i;
    MDDUMP("- [%s](#typedef-%s)\n", td->name, td->name);
    i++;
  }

  char linkpath[PATH_MAX];
  char displaypath[PATH_MAX];
  i = 0;
  while (i < tds->count) {
    struct typedef_spec * td = tds->array + i;
    MDDUMP("### typedef %s\n```c\ntypedef ", td->name);

    dump_typedef_definition(td, fd, buffer);

    generate_path_pair(td->ref->filename, config, displaypath, linkpath);
    if (config->gitline) MDDUMP(";\n```\nDefined in [%s:%zu](%s#L%zu)\n", displaypath, td->ref->line, linkpath, td->ref->line)
    else MDDUMP(";\n```\nDefined in [%s:%zu](%s)\n", displaypath, td->ref->line, linkpath);
    i++;
  }

  close(fd);
}

static void write_macro_ref(struct macro_spec * macro, char * buf) {
  unsigned int index = 0;
  while (macro->name[index] != 0 && index < ENTITIES_NAME_LENGTH) {
    if (65 <= macro->name[index] && macro->name[index] <= 90) buf[index] = macro->name[index] + 32;
    else buf[index] = macro->name[index];
    index++;
  }
  buf[index++] = 0;
}
static void generate_macros_page(MDARGS_T, struct macro_indic * macros, int is_header) {
  PREPARE;
  PATHNAME(pathbuf, config, "%s-macros.md", is_header ? "headers" : "source");
  OPEN(fd, pathbuf, result);

  MDDUMP("# %s macros\n\nThis file contains the macros that are written in the %s files\n\n## Defined in this file\n\nThe following **%zu** macros are referenced in this file\n", is_header ? "Headers" : "Source", is_header ? "headers" : "source", macros->count);

  char linkpath[PATH_MAX];
  char displaypath[PATH_MAX];
  ulint i = 0;
  while (i < macros->count) {
    struct macro_spec * macro = macros->array + i;
    write_macro_ref(macro, linkpath);
    MDDUMP("- [%s](#macro-%s)\n", macro->name, linkpath);
    i++;
  }
  i = 0;
  while (i < macros->count) {
    struct macro_spec * macro = macros->array + i;
    MDDUMP("### macro %s\n```c\n#define %s", macro->name, macro->name);

    if (macro->is_function) {
      MDDUMP("(");
      ulint x = 0;
      while (x < macro->args->count) {
        MDDUMP("%s%s", macro->args->array[x], x + 1 < macro->args->count ? ", " : "");
        x++;
      }
      MDDUMP(")")
    }
    MDDUMP(" %s", macro->placeholder);

    generate_path_pair(macro->ref->filename, config, displaypath, linkpath);
    if (config->gitline) MDDUMP("\n```\nDefined in [%s:%zu](%s#L%zu)\n", displaypath, macro->ref->line, linkpath, macro->ref->line)
    else MDDUMP("\n```\nDefined in [%s:%zu](%s)\n", displaypath, macro->ref->line, linkpath);
    i++;
  }

  close(fd);
}
static void generate_homepage(MDARGS_T) {
  PREPARE;
  PATHNAME(pathbuf, config, "Home.md");
  OPEN(fd, pathbuf, result);

  MDDUMP("# Homepage\n\n## Summary\n\nThis documentation is split between the informations found in header files and source files.\n| Headers | Implementation |\n|:---:|:---:|\n");
  MDDUMP("| [`functions (%zu)`](headers-functions.md) | [`Functions (%zu)`](source-functions.md) |\n| [`structures (%zu)`](headers-structures.md) | [`structures (%zu)`](source-structures.md)\n| [`typedefs (%zu)`](headers-typedefs.md) | [`typedefs (%zu)`](source-typedefs) |\n| [`Macros (%zu)`](headers-macros.md) | [`Macros (%zu)`](source-macros.md) |\n> See also the [`%zu primitives`](primitives.md)",
      code->definitions.functions.count,
      code->implementations.functions.count,
      code->definitions.structs.count,
      code->implementations.structs.count,
      code->definitions.typedefs.count,
      code->implementations.typedefs.count,
      code->definitions.macros.count,
      code->implementations.macros.count,
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
  generate_typedefs_page(MDARGS_V, &(code->definitions.typedefs), 1);
  generate_typedefs_page(MDARGS_V, &(code->implementations.typedefs), 0);
  generate_macros_page(MDARGS_V, &(code->definitions.macros), 1);
  generate_macros_page(MDARGS_V, &(code->implementations.macros), 0);
  generate_primitives(config, result, buffer, &(code->primitives));
  return result->fails > before;
}

#undef MDDUMP
#undef MDARGS_T
#undef MDARGS_V
#undef OPEN
#undef PREPARE
#undef PATHNAME
