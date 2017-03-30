/*
 * stack.h
 *
 *  Created on: 30.03.2017
 *      Author: pascal
 */

#ifndef STACK_H_
#define STACK_H_

#include "stdbool.h"

typedef struct stack stack_t;

stack_t *stack_create();
void stack_destroy(stack_t *stack);
void stack_push(stack_t *stack, void *data);
bool stack_pop(stack_t *stack, void **value);

#endif /* STACK_H_ */
