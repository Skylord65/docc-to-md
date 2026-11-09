#ifndef __READER_H__
#define __READER_H__

#include <linux/limits.h>

#define ENTITIES_NAME_LENGTH 64
#define MACRO_PLACEHOLDER_LENGTH 2048

typedef unsigned long int ulint;
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
  NoneType = 0,
  StructureType = 1,
  FunctionType = 2,
  TypedefType = 3,
  MacroType = 4,
  PrimitiveType = 5
} TypeTypeIndicator;

typedef struct {
  TypeTypeIndicator type;
  char string[ENTITIES_NAME_LENGTH];
  void * ptr;
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
struct typedef_spec {
  reference_t ref;
  char name[ENTITIES_NAME_LENGTH];
  entity_type_t * definition;
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

struct source_code_defs {
  struct function_indic  functions;
  struct macro_indic     macros;
  struct struct_indic    structs;
  struct typedef_indic   typedefs;
  struct variables_indic variables;
};
struct source_code_impls {
  struct function_indic  functions;
  struct macro_indic     macros;
  struct struct_indic    structs;
  struct typedef_indic   typedefs;
};

struct source_code {
  struct source_code_impls implementations;
  struct source_code_defs definitions;
};

typedef struct source_code * source_code_t;

void free_source_code(source_code_t);
int dump_source_code(const source_code_t, int fd);

#endif
