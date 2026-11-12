#ifndef __READER_H__
#define __READER_H__

#include "../core/config.h"
#if defined(_WIN32)
  #include <stdlib.h>
  #ifndef PATH_MAX
    #define PATH_MAX _MAX_PATH
  #endif
#else
  #include <limits.h>
#endif
#include <stddef.h>

#define ENTITIES_NAME_LENGTH 64
#define MACRO_PLACEHOLDER_LENGTH 2048

typedef size_t ulint;
typedef char fnt_modifier;

#define F_MSTATIC 0x00000001
#define F_MVOLATI 0x00000010
#define F_MEXTERN 0x00000100

#define F_ISTATIC(x) (((x) & F_MSTATIC) == F_MSTATIC)
#define F_IVOLATI(x) (((x) & F_MVOLATI) == F_MVOLATI)
#define F_IEXTERN(x) (((x) & F_MEXTERN) == F_MEXTERN)

#define F_INONE(x) (!F_ISTATIC(x) && !F_IEXTERN(x) && !F_IVOLATI(x))

struct reference {
  char filename[PATH_MAX];
  ulint line;
};
typedef struct reference * reference_t;

// Type type
typedef enum {
  NoneType             = 0,
  StructureType        = 1,
  FunctionType         = 2,
  TypedefType          = 3,
  MacroType            = 4,
  PrimitiveType        = 5,
  EnumeratorType       = 6,
  TypedefinedFunction  = 7,
  TypedefinedStructure = 8,
  TypedefinedEnum      = 9
} TypeTypeIndicator;

struct arrays_spec {
  ulint count;
  char ** array;
};
typedef struct {
  TypeTypeIndicator type;
  char string[ENTITIES_NAME_LENGTH];
  void * ptr;
  unsigned int pointers_level;
  struct arrays_spec arrays;
} entity_type_t;

// Members types
struct struct_member {
  char name[ENTITIES_NAME_LENGTH];
  entity_type_t type;
};
struct struct_members {
  ulint count;
  struct struct_member * array;
};
struct struct_spec {
  reference_t ref;
  char name[ENTITIES_NAME_LENGTH];
  struct struct_members * members;
};
struct enum_member {
  char name[ENTITIES_NAME_LENGTH];
  char value[ENTITIES_NAME_LENGTH];
};
struct enum_members {
  ulint count;
  struct enum_member * array;
};
struct enum_spec {
  char name[ENTITIES_NAME_LENGTH];
  struct enum_members * members;
  reference_t ref;
  fnt_modifier modifiers;
};
struct typedef_spec {
  reference_t ref;
  char * buffer;
  char name[ENTITIES_NAME_LENGTH];
  entity_type_t definition;
  fnt_modifier modifiers;
  char is_function;
};
struct function_arg {
  char name[ENTITIES_NAME_LENGTH];
  entity_type_t type;
};
struct function_args {
  ulint count;
  struct function_arg * array;
};
struct function_spec {
  char name[ENTITIES_NAME_LENGTH];
  entity_type_t returntype;
  fnt_modifier modifiers;
  reference_t ref;

  struct function_args * args;
  char * description;
};
struct macro_args {
  ulint count;
  char ** array;
};
struct macro_spec {
  char name[ENTITIES_NAME_LENGTH];
  char placeholder[MACRO_PLACEHOLDER_LENGTH];
  int is_function;
  struct macro_args * args;
  reference_t ref;
};
typedef enum {
  GlibC          = 0b00000000000001,
  Stdio          = 0b00000000000010,
  Stdbool        = 0b00000000000100,
  Stdint         = 0b00000000001000,
  Unistd         = 0b00000000010000,
  Fcntl          = 0b00000000100000,
  SysStat        = 0b00000001000000,
  SysDir         = 0b00000010000000,
  LibStringpGrep = 0b00000100000000,
  Stdtime        = 0b00001000000000,
  Stddef         = 0b00010000000000,
  SysTypes       = 0b00100000000000,
  Stdlib         = 0b01000000000000,
  Dirent         = 0b10000000000000,
} PrimitiveSource;

#define P_TGLIBC(x) ((x & GlibC) == GlibC)
#define P_TSTDIO(x) ((x & Stdio) == Stdio)
#define P_TSTDBOOL(x) ((x & Stdbool) == Stdbool)
#define P_TSTDINT(x) ((x & Stdint) == Stdint)
#define P_TUNISTD(x) ((x & Unistd) == Unistd)
#define P_TFCNTL(x) ((x & Fcntl) == Fcntl)
#define P_TSYSSTAT(x) ((x & SysStat) == SysStat)
#define P_TSYSDIR(x) ((x & SysDir) == SysDir)
#define P_TLIBSTRINGPGREP(x) ((x & LibStringpGrep) == LibStringpGrep)
#define P_TSTDTIME(x) ((x & Stdtime) == Stdtime)
#define P_TSTDDEF(x) ((x & Stddef) == Stddef)
#define P_TSYSTYPES(x) ((x & SysTypes) == SysTypes)
#define P_TSTDLIB(x) ((x & Stdlib) == Stdlib)
#define P_TDIRENT(x) ((x & Dirent) == Dirent)

struct primitive_spec {
  char name[ENTITIES_NAME_LENGTH];
  PrimitiveSource source;
  char is_structure;
};
struct variable {
  reference_t ref;
  entity_type_t * type;
  char name[ENTITIES_NAME_LENGTH];
};

struct variables_indic {
  ulint count;
  struct variable * array;
};
struct function_indic {
  ulint count;
  struct function_spec * array;
};
struct macro_indic {
  ulint count;
  struct macro_spec * array;
};
struct struct_indic {
  ulint count;
  struct struct_spec * array;
};
struct typedef_indic {
  ulint count;
  struct typedef_spec * array;
};
struct enum_indic {
  ulint count;
  struct enum_spec * array;
};
struct primitives_indic {
  ulint count;
  struct primitive_spec * array;
};

struct source_code_defs {
  struct function_indic  functions;
  struct macro_indic     macros;
  struct struct_indic    structs;
  struct typedef_indic   typedefs;
  struct variables_indic variables;
  struct enum_indic      enums;
};
struct source_code_impls {
  struct function_indic  functions;
  struct macro_indic     macros;
  struct struct_indic    structs;
  struct typedef_indic   typedefs;
  struct enum_indic      enums;
};

struct source_code {
  struct source_code_impls implementations;
  struct source_code_defs definitions;

  struct primitives_indic primitives;
};

typedef struct source_code * source_code_t;

void init_source_code(source_code_t);
void free_source_code(source_code_t);
int dump_source_code(const source_code_t, int fd);
void summarise_parsing(source_code_t);
void summarise_linking(const callback_args);

#endif
