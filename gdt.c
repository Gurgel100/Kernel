/*
 * gdt.c
 *
 *  Created on: 02.07.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "gdt.h"
#include "display.h"

static uint64_t gdt[GDT_ENTRIES];

void GDT_Init()
{
	gdtr_t gdtr;
	GDT_SetEntry(0, 0, 0, 0, 0);				//NULL-Deskriptor
	//Ring 0
	GDT_SetEntry(1, 0, 0xFFFFF, 0x9A, 0xA);	//Codesegment, ausführ- und lesbar, 64-bit, Ring 0
	GDT_SetEntry(2, 0, 0xFFFFF, 0x92, 0xC);	//Datensegment, les- und schreibbar
	//Ring 3
	GDT_SetEntry(3, 0, 0xFFFFF, 0xFA, 0xA);	//Codesegment, ausführ- und lesbar, 64-bit, Ring 3
	GDT_SetEntry(4, 0, 0xFFFFF, 0xF2, 0xC);	//Datensegment, les- und schreibbar, Ring 3

	gdtr.limit = GDT_ENTRIES *8 - 1;
	gdtr.pointer = gdt;
	asm volatile("lgdt %0": :"m"(gdtr));
	asm volatile(
			"mov $0x10,%ax;"//Index für das Datensegment des Kernels
			"mov %ax,%ds;"
			"mov %ax,%es;"
			"mov %ax,%ss;"
			"mov %ax,%fs;"
			"mov %ax,%gs;"
			"push $0x8;"	//Compiler akzeptiert keinen farjump also machen wir es auf diese
			"push $(.1);"	//Art. So holt sich die CPU den Codesegmentindex vom Stack
			"lretq;"
			".1:"
	);
	SysLog("GDT", "Initialisierung abgeschlossen");
}

void GDT_SetEntry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
{
	gdt[i] = limit & 0xFFFF;
	gdt[i] |= (base & 0xFFFFFFLL) << 16;
	gdt[i] |= (access & 0xFFLL) << 40;
	gdt[i] |= ((limit >> 16) & 0xFLL) << 48;
	gdt[i] |= (flags & 0xFLL) << 52;
	gdt[i] |= ((base >> 24) & 0xFFLL) << 56;
}

void GDT_SetSystemDescriptor(int i, uint64_t base, uint32_t limit, uint8_t access, uint8_t flags)
{
	gdt[i] = limit & 0xFFFF;
	gdt[i] |= (base & 0xFFFFFFLL) << 16;
	gdt[i] |= (access & 0xFFLL) << 40;
	gdt[i] |= ((limit >> 16) & 0xFLL) << 48;
	gdt[i] |= (flags & 0xFLL) << 52;
	gdt[i + 1] = base >> 24;
	gdt[i + 1] |= 0 << 32;
}

#endif
