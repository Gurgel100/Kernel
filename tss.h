/*
 * tss.h
 *
 *  Created on: 02.11.2012
 *      Author: pascal
 */

#ifndef TSS_H_
#define TSS_H_

#include <stdint.h>

void TSS_Init(void);
void TSS_setStack(void *stack);
void TSS_setIST(uint32_t ist, void *ist_stack);

#endif /* TSS_H_ */
