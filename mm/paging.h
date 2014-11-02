/*
 * paging.h
 *
 *  Created on: 07.07.2012
 *      Author: pascal
 */

#ifndef PAGING_H_
#define PAGING_H_

#include "stdint.h"

/*Pagingstrukturen
 *PML4 = Page-Map-Level-4 Table
 *PML4E = Page-Map-Level-4 Entry
 *PDP = Page-Directory-Pointer Table
 *PDPE = Page-Directory-Pointer Entry
 *PD = Page-Directory Table
 *PDE = Page-Directory Entry
 *PT = Page Table
 *PTE = Page Table Entry
 */
//Allgemeine Bits in den Page Tabellen
#define PG_P		0x1
#define PG_RW		0x2
#define PG_US		0x4
#define	PG_PWT		0x8
#define PG_PCD		0x10
#define PG_A		0x20
//Nur in PTE vorhanden
#define PG_D		0x40
#define PG_PAT		0x80
#define PG_G		0x100LL
//Allgemein
#define PG_AVL1		0xE00LL
#define PG_ADDRESS	0xFFFFFFFFFF000LL
#define PG_AVL2		0x7FF0000000000000LL
#define PG_NX		0x8000000000000000LL
#define PG_AVL(Entry)	(((Entry & PG_AVL1) | ((Entry & PG_AVL2) >> 40)) >> 9)

//Indices der Tabellen in der virtuellen Addresse
#define PG_PML4_INDEX	0xFF8000000000
#define PG_PDP_INDEX	0x7FC0000000
#define PG_PD_INDEX		0x3FE00000
#define PG_PT_INDEX		0x1FF000

#define MAP				4096	//Anzahl der Bytes pro Map (4kb)
#define PAGE_ENTRIES	512		//Anzahl der Einträge pro Tabelle

/*typedef struct{
		unsigned P : 1;
		unsigned RW : 1;
		unsigned US : 1;
		unsigned PWT : 1;
		unsigned PCD : 1;
		unsigned A : 1;
		unsigned IGN : 1;
		unsigned MBZ : 2;
		unsigned AVL1 : 3;
		unsigned long PDP : 40;
		unsigned AVL2 : 11;
		unsigned NX : 1;
}__attribute__((packed)) PML4_t;*/

typedef struct{
		uint64_t PML4E[PAGE_ENTRIES];
}PML4_t;

typedef struct{
		uint64_t PDPE[PAGE_ENTRIES];
}PDP_t;

typedef struct{
		uint64_t PDE[PAGE_ENTRIES];
}PD_t;

typedef struct{
		uint64_t PTE[PAGE_ENTRIES];
}PT_t;

void setPML4Entry(uint16_t i, PML4_t *PML4, uint8_t Present, uint8_t RW, uint8_t US, uint8_t PWT,
		uint8_t PCD, uint8_t A, uint16_t AVL, uint8_t NX, uintptr_t Address);
void setPDPEntry(uint16_t i, PDP_t *PDP, uint8_t Present, uint8_t RW, uint8_t US, uint8_t PWT,
		uint8_t PCD, uint8_t A, uint16_t AVL, uint8_t NX, uintptr_t Address);
void setPDEntry(uint16_t i, PD_t *PD, uint8_t Present, uint8_t RW, uint8_t US, uint8_t PWT,
		uint8_t PCD, uint8_t A, uint16_t AVL, uint8_t NX, uintptr_t Address);
void setPTEntry(uint16_t i, PT_t *PT, uint8_t Present, uint8_t RW, uint8_t US, uint8_t PWT,
		uint8_t PCD, uint8_t A, uint8_t D, uint8_t G, uint16_t AVL,
		uint8_t PAT, uint8_t NX, uintptr_t Address);
inline void clearPML4Entry(uint16_t i, PML4_t *PML4);
inline void clearPDPEntry(uint16_t i, PDP_t *PDP);
inline void clearPDEntry(uint16_t i, PD_t *PD);
inline void clearPTEntry(uint16_t i, PT_t *PT);

inline void InvalidateTLBEntry(void *Address);
#endif /* PAGING_H_ */
