/*
 * idt.c
 *
 *  Created on: 02.07.2012
 *      Author: pascal
 */

#include "idt.h"
#include "display.h"

static uint128_t idt[IDT_ENTRIES];	//Jeder Eintrag ist 16-Byte (128-Bit) gross

//Exception Handler
extern int0;
extern int1;
extern int2;
extern int3;
extern int4;
extern int5;
extern int6;
extern int7;
extern int8;
extern int9;
extern int10;
extern int11;
extern int12;
extern int13;
extern int14;
extern int16;
extern int17;
extern int18;
extern int19;
//IRQ-Handler
extern int32;
extern int33;
extern int34;
extern int35;
extern int36;
extern int37;
extern int38;
extern int39;
extern int40;
extern int41;
extern int42;
extern int43;
extern int44;
extern int45;
extern int46;
extern int47;
//Syscalls
extern int255;
void IDT_Init(void)
{
	idtr_t idtr;

	//Exceptions
	IDT_SetEntry(0, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int0);
	IDT_SetEntry(1, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int1);
	IDT_SetEntry(2, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int2);
	IDT_SetEntry(3, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int3);
	IDT_SetEntry(4, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int4);
	IDT_SetEntry(5, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int5);
	IDT_SetEntry(6, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int6);
	IDT_SetEntry(7, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int7);
	IDT_SetEntry(8, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int8);
	IDT_SetEntry(9, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int9);
	IDT_SetEntry(10, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int10);
	IDT_SetEntry(11, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int11);
	IDT_SetEntry(12, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int12);
	IDT_SetEntry(13, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int13);
	IDT_SetEntry(14, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int14);
	IDT_SetEntry(16, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int16);
	IDT_SetEntry(17, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int17);
	IDT_SetEntry(18, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int18);
	IDT_SetEntry(19, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int19);

	//IRQs
	IDT_SetEntry(32, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int32);
	IDT_SetEntry(33, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int33);
	IDT_SetEntry(34, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int34);
	IDT_SetEntry(35, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int35);
	IDT_SetEntry(36, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int36);
	IDT_SetEntry(37, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int37);
	IDT_SetEntry(38, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int38);
	IDT_SetEntry(39, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int39);
	IDT_SetEntry(40, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int40);
	IDT_SetEntry(41, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int41);
	IDT_SetEntry(42, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int42);
	IDT_SetEntry(43, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int43);
	IDT_SetEntry(44, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int44);
	IDT_SetEntry(45, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int45);
	IDT_SetEntry(46, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int46);
	IDT_SetEntry(47, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_KERNEL | IDT_PRESENT, (uintptr_t)&int47);

	//Syscall
	IDT_SetEntry(255, 0x8, IDT_TYPE_INTERRUPT | IDT_DPL_USER | IDT_PRESENT, (uintptr_t)&int255);

	idtr.limit = sizeof(idt) - 1;
	idtr.pointer = idt;
	asm volatile("lidt %0" : :"m"(idtr));
	SysLog("IDT", "Initialisierung abgeschlossen");
}

/*
 * Setzt einen Eintrag in die IDT
 */
void IDT_SetEntry(uint8_t i, uint16_t Selector, uint16_t Flags, uintptr_t Offset)
{
	idt[i] = (uint128_t)(Offset & 0xFFFF);
	idt[i] |= (uint128_t)(Selector << 16);
	idt[i] |= (uint128_t)Flags << 32;
	idt[i] |= (uint128_t)(Offset >> 16) << 48;
}
