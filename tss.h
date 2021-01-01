/*
 * tss.h
 *
 *  Created on: 02.11.2012
 *      Author: pascal
 */

#ifndef TSS_H_
#define TSS_H_
#include "stdint.h"

typedef struct{
		uint16_t Selector;
}__attribute__((packed)) tr_t;

typedef struct{
		uint32_t IGN;
		uint64_t rsp0;
		uint64_t rsp1;
		uint64_t rsp2;
		uint64_t IGN2;
		uint64_t ist1;
		uint64_t ist2;
		uint64_t ist3;
		uint64_t ist4;
		uint64_t ist5;
		uint64_t ist6;
		uint64_t ist7;
		uint64_t IGN3;
		uint16_t IGN4;
		uint16_t MapBaseAddress;
		uint8_t IOPD[8192];
}__attribute__((packed))tss_entry_t;

void TSS_Init(void);
void TSS_setStack(void *stack);

#endif /* TSS_H_ */
