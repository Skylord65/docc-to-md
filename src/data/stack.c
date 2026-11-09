#include "stack.h"

#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_STACK_SIZE 10

struct stack_s {
  int stack_size;
  int pos;
  char * array;
};

stack_t create_stack() {
  stack_t stack;
  if ((stack = malloc(sizeof(struct stack_s))) == NULL) {
    perror("malloc stack");
    return NULL;
  }
  if ((stack->array = malloc(sizeof(char) * DEFAULT_STACK_SIZE)) == NULL) {
    perror("malloc stack->array");
    return NULL;
  }

  stack->stack_size = DEFAULT_STACK_SIZE;
  stack->pos = -1;
  return stack;
}
unsigned stack_size(stack_t stack) {
  return stack->pos + 1;
}
void destroy_stack(stack_t * pstack) {
  free((*pstack)->array);
  free(*pstack);
  *pstack = NULL;
}
char top(stack_t stack) {
  if (stack->pos == -1) return 0;
  return stack->array[stack->pos];
}
char pop(stack_t stack) {
  if (stack->pos == -1) return 0;
  return stack->array[stack->pos--];
}
int push(stack_t stack, char c) {
  if (stack->pos == stack->stack_size - 1) {
    if ((stack->array = realloc(stack->array, stack->stack_size + DEFAULT_STACK_SIZE)) == NULL) {
      perror("realloc stack");
      return 0;
    }
    stack->stack_size += DEFAULT_STACK_SIZE;
  }
  stack->array[stack->pos++] = c;
  return 1;
}

