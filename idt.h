/*
 * idt.h
 *
 *  Created on: 02.07.2012
 *      Author: pascal
 */

#ifndef IDT_H_
#define IDT_H_

#define IDT_ENTRIES 49

#include "stdint.h"

//Flags
#define IDT_TYPE_INTERRUPT	(0b1110 << 8)	//Interrupts werden deaktiviert
#define IDT_TYPE_TRAP_GATE	(0b1111 << 8)	//Interrupts bleiben aktiviert

#define IDT_DPL_KERNEL	0

#define IDT_PRESENT			0x8000

typedef struct{
		uint16_t	limit;
		void 		*pointer;
}__attribute__((packed)) idtr_t;

//Funktionen
void IDT_Init(void);
void IDT_SetEntry(uint8_t i, uint16_t Selector, uint16_t Flags, uintptr_t Offset);

#endif /* IDT_H_ */
