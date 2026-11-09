#ifndef __STACK_H__
#define __STACK_H__

struct stack_s;
typedef struct stack_s * stack_t;

extern stack_t create_stack();
/**
 * Get the top of the stack
 *
 * Returns 0 if empty
 */
extern char top(stack_t);
/**
 * Push an element at the top of the stack
 *
 * Returns if the element has been successfully pushed. 1 : is done 0 is failed.
 * More details on fails can be written on standard error
 */
extern int push(stack_t, char);
/**
 * Get the top of the stack, and remove the top element
 *
 * Returns 0 if empty
 */
extern char pop(stack_t);
extern void destroy_stack(stack_t *);
extern int stack_empty(stack_t);
extern unsigned stack_size(stack_t);

#endif
