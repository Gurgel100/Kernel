/*
 * tss.c
 *
 *  Created on: 02.11.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "tss.h"
#include "gdt.h"

#define SELECTOR	5
#define STACKSIZE	4096		//4kb für den Stack

static tss_entry_t tss;
static uint8_t stack0[STACKSIZE] __attribute__((aligned(16)));	//0.5 MB für den Stack (16-Byte aligned)

void TSS_Init()
{
	//GDT-Eintrag erstellen
	GDT_SetSystemDescriptor(SELECTOR, (uintptr_t)&tss, sizeof(tss), 0x89, 0x0);

	//Task-Segment Selector
	tr_t tr;
	tr.Selector = SELECTOR << 3;

	//TSS initialisieren
	TSS_setStack((void*)(stack0 + STACKSIZE));
	tss.MapBaseAddress = 0x96;
	memset(tss.IOPD, 0, 8192);

	//Taskergister laden
	asm volatile("ltr %0" : : "m"(tr));
}

void TSS_setStack(void *stack)
{
	tss.rsp0 = stack;
}

#endif
