/*
 * stack.c
 *
 *  Created on: 30.03.2017
 *      Author: pascal
 */

#include "stack.h"
#include "stdlib.h"
#include "assert.h"
#include "stdint.h"

typedef struct stack_elem{
	struct stack_elem *next;
	void *data;
}stack_elem_t;

struct stack{
	union{
		uint128_t full_stack;
		struct{
			stack_elem_t *head;
			uintptr_t tag;
		};
	};
};

stack_t *stack_create()
{
	return calloc(1, sizeof(stack_t));
}

void stack_destroy(stack_t *stack)
{
	assert(stack != NULL);
	while (stack_pop(stack, NULL));
	free(stack);
}

void stack_push(stack_t *stack, void *data)
{
	assert(stack != NULL);
	stack_t prev, new;
	stack_elem_t *elem = malloc(sizeof(stack_elem_t));
	elem->data = data;
	do
	{
		prev = *stack;
		elem->next = prev.head;
		new.head = elem;
		new.tag = prev.tag + 1;
	}
	while(!__sync_bool_compare_and_swap(&stack->full_stack, prev.full_stack, new.full_stack));
}

bool stack_pop(stack_t *stack, void **value)
{
	assert(stack != NULL);
	stack_t prev, new;

	do
	{
		prev = *stack;
		if(prev.head == NULL)
			return false;
		new.tag = prev.tag;
		new.head = prev.head->next;
	}
	while(!__sync_bool_compare_and_swap(&stack->full_stack, prev.full_stack, new.full_stack));

	if(value != NULL)
		*value = prev.head->data;
	free(prev.head);
	return true;
}
