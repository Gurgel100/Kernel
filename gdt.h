/*
 * gdt.h
 *
 *  Created on: 02.07.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#ifndef GDT_H_
#define GDT_H_

#define GDT_ENTRIES	7

#include "stdint.h"

typedef struct{
		uint16_t	limit;
		void 		*pointer;
}__attribute__((packed)) gdtr_t;

typedef struct{
		uint16_t Limit1;	//Segment Limit 0-15
		uint32_t Base1 :24;	//Base Address 0-23
		uint8_t A :1;		//Zugriff-Bit (Wird von der CPU bei Zugriff gesetzt)
		uint8_t R :1;		//Lesbar-Bit (Kann das Segment gelesen werden?)
		uint8_t C :1;		//
		uint8_t MB1 :2;		//Muss 0b11 sein
		uint8_t DPL :2;		//
		uint8_t P :1;
		uint8_t Limit2 :4;	//Segment Limit 16-19
		uint8_t AVL :1;		//Frei verfügbar
		uint8_t L :1;		//Long Attribute Bit (64-Bit Modus = 1 | Legacy Mode = 0)
		uint8_t D :1;
		uint8_t G :1;		//Granularity-Bit
		uint8_t Base2;		//Base Address 24-31
}__attribute__((packed)) gdt_code_segment_t;


//Funktionen
void GDT_Init(void);
void GDT_SetEntry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags);
void GDT_SetSystemDescriptor(int i, uint64_t base, uint32_t limit, uint8_t access, uint8_t flags);

#endif /* GDT_H_ */

#endif
