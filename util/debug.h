/*
 * debug.h
 *
 *  Created on: 15.03.2013
 *      Author: pascal
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#include "isr.h"
#include "stdbool.h"

#define DEBUG_CONTINUE		0x0
#define DEBUG_SINGLESTEP	0x1

volatile bool Debugged;

void Debug_Init(void);
void Debug_Main(ihs_t *ihs);

uint64_t traceRegistersToString(ihs_t *ihs, char *out);
uint64_t traceStackToString(uint64_t rsp, uint64_t *rbp, uint8_t length, char *out);

#endif /* DEBUG_H_ */
