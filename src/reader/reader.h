#ifndef __READER_H__
#define __READER_H__

#include <linux/limits.h>

#define ENTITIES_NAME_LENGTH 64

typedef unsigned long int ulint;

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
  entity_type_t * type;
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
  entity_type_t * type;
};
struct function_args {
  ulint count;
  struct function_arg * array;
};
struct function_spec {
  char name[ENTITIES_NAME_LENGTH];
  reference_t ref;

  struct function_args * args;
};
struct macro_spec {};
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

struct source_code {
  struct function_indic  functions;
  struct macro_indic     macros;
  struct struct_indic    structs;
  struct typedef_indic   typedefs;
  struct variables_indic variables;
};

typedef struct source_code * source_code_t;

void free_source_code(source_code_t);

#endif
