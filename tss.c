/*
 * tss.c
 *
 *  Created on: 02.11.2012
 *      Author: pascal
 */

#include "tss.h"
#include "gdt.h"
#include "string.h"
#include <assert.h>

#define SELECTOR	5

typedef struct{
		uint16_t Selector;
}__attribute__((packed)) tr_t;

typedef struct{
		uint32_t IGN;
		uint64_t rsp0;
		uint64_t rsp1;
		uint64_t rsp2;
		uint64_t IGN2;
		uint64_t ist[7];
		uint64_t IGN3;
		uint16_t IGN4;
		uint16_t MapBaseAddress;
		uint8_t IOPD[8192];
}__attribute__((packed)) tss_entry_t;

tss_entry_t tss;

void TSS_Init()
{
	//GDT-Eintrag erstellen
	GDT_SetSystemDescriptor(SELECTOR, (uintptr_t)&tss, sizeof(tss), 0x89, 0x0);

	//Task-Segment Selector
	tr_t tr;
	tr.Selector = SELECTOR << 3;

	//TSS initialisieren
	tss.MapBaseAddress = 0x96;
	memset(tss.IOPD, 0, 8192);

	//Taskergister laden
	asm volatile("ltr %0" : : "m"(tr));
}

void TSS_setStack(void *stack)
{
	tss.rsp0 = (uint64_t)stack;
}

void TSS_setIST(uint32_t ist, void *ist_stack) {
	ist--;
	assert(ist < 7);
	tss.ist[ist] = (uint64_t)ist_stack;
}
