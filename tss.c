/*
 * tss.c
 *
 *  Created on: 02.11.2012
 *      Author: pascal
 */

#include "tss.h"
#include "gdt.h"

#define SELECTOR	4

static tss_entry_t tss;
static uint8_t stack0[1000] __attribute__((aligned(16)));	//0.5 MB für den Stack (16-Byte aligned)

void TSS_Init()
{
	//GDT-Eintrag erstellen
	GDT_SetSystemDescriptor(SELECTOR, &tss, sizeof(tss), 0x89, 0x0);

	//Task-Segment Selector
	tr_t tr;
	tr.Selector = SELECTOR << 3;

	//TSS initialisieren
	tss.rsp0 = &stack0;
	tss.MapBaseAddress = 0x96;
	int i;
	for(i = 0; i < 8192; i++)
		tss.IOPD[i] = 0;

	//Taskergister laden
	asm volatile("ltr %0" : : "m"(tr));
}
