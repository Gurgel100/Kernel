/*
 * paging.h
 *
 *  Created on: 07.07.2012
 *      Author: pascal
 */

#ifndef PAGING_H_
#define PAGING_H_

#include "stdint.h"
#include "stdbool.h"
#include "pmm.h"

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

#define PG_PAGE_SIZE				4096
#define PG_PAGE_ALIGN_ROUND_DOWN(n)	((n) & PG_ADDRESS)
#define PG_PAGE_ALIGN_ROUND_UP(n)	(((n) + ~PG_ADDRESS) & PG_ADDRESS)
#define PG_NUM_PAGES(size)			(PG_PAGE_ALIGN_ROUND_UP(size) / PG_PAGE_SIZE)

#define MAP				4096	//Anzahl der Bytes pro Map (4kb)
#define PAGE_ENTRIES	512		//Anzahl der Einträge pro Tabelle

typedef union {
	struct {
		bool P: 1;
		bool RW: 1;
		bool US: 1;
		bool PWT: 1;
		bool PCD: 1;
		bool A: 1;
		bool D: 1;			// Only valid on last level page table
		bool PS_PAT: 1;	// PS: valid for PML4 (must be 0), PDP, PD. PAT: valid for PT
		bool G: 1;
		uint32_t AVL1: 3;
		paddr_t Address: 40;
		uint32_t AVL2: 11;
		bool NX: 1;
	}__attribute__((packed));
	uint64_t entry;
} PageTableEntry_t;

typedef PageTableEntry_t* PageTable_t;

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

void setPML4Entry(uint16_t i, PML4_t *PML4, bool Present, bool RW, bool US, bool PWT,
		bool PCD, bool A, uint16_t AVL, bool NX, paddr_t Address);
void setPDPEntry(uint16_t i, PDP_t *PDP, bool Present, bool RW, bool US, bool PWT,
		bool PCD, bool A, uint16_t AVL, bool NX, paddr_t Address);
void setPDEntry(uint16_t i, PD_t *PD, bool Present, bool RW, bool US, bool PWT,
		bool PCD, bool A, uint16_t AVL, bool NX, paddr_t Address);
void setPTEntry(uint16_t i, PT_t *PT, bool Present, bool RW, bool US, bool PWT,
		bool PCD, bool A, bool D, bool G, uint16_t AVL,
		bool PAT, bool NX, paddr_t Address);
void clearPML4Entry(uint16_t i, PML4_t *PML4);
void clearPDPEntry(uint16_t i, PDP_t *PDP);
void clearPDEntry(uint16_t i, PD_t *PD);
void clearPTEntry(uint16_t i, PT_t *PT);

void InvalidateTLBEntry(void *Address);
#endif /* PAGING_H_ */
