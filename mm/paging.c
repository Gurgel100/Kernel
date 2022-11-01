/*
 * paging.c
 *
 *  Created on: 07.07.2012
 *      Author: pascal
 */

#include "paging.h"
#include "cpu.h"

inline void FlushTLB(void);

/*Flags:
 * Present: 1: Page ist im Speicher; 0: Page ist nicht im Speicher, der ganze Eintrag wird ignoriert
 * RW: 0: Zugriff auf Page nur lesen; 1: Auf Page kann lesend und schreibend zugegriffen werden
 * US: 0: Nur CPL 0,1,2 kann auf Page zugreifen; 1: auch CPL 3 kann auf Page zugreifen
 * PWT: 0: Page hat Writeback caching; 1: Page hat Writethrough caching
 * PCD: 0: Page ist cacheable; 1: Page ist nicht cacheable
 * A: 1: Prozessor hat auf die Page zugegriffen muss durch Software zurückgesetzt werden (std: 0)
 * D: 1: Prozessor hat in die Page geschrieben muss durch Software zurückgesetzt werden
 * PS: 1: Dieser Eintrag ist der Letzte in der Pagingtranslation, für 4kb Pages immer 0
 * G: 1: Die Page ist global (CR4.PGE muss 1 sein)
 * AVL: Verfügbar für Software, werden nicht vom Prozessor interpretiert
 * PAT: Kann mit dem PAT Register verwendet werden. Ich nehme 0
 * NX: 0: Es kann Code von der Page ausgeführt werden; 1: Es kann kein Code von der Page ausgeführt werden
 */

void setPML4Entry(uint16_t i, PML4_t *PML4, bool Present, bool RW, bool US, bool PWT,
		bool PCD, bool A, uint16_t AVL, bool NX, paddr_t Address)
{
	PML4->PML4E[i] = (Present & 1);
	PML4->PML4E[i] |= (RW & 1) << 1;
	PML4->PML4E[i] |= (US & 1) << 2;
	PML4->PML4E[i] |= (PWT & 1) << 3;
	PML4->PML4E[i] |= (PCD & 1) << 4;
	PML4->PML4E[i] |= (A & 1) << 5;
	PML4->PML4E[i] |= (AVL & 0x7LL) << 9;
	PML4->PML4E[i] |= Address & 0xFFFFFFFFFF000LL;
	PML4->PML4E[i] |= ((AVL >> 3) & 0x7FFLL) <<52;
	PML4->PML4E[i] |= ((NX & cpuInfo.nx) & 1LL) << 63;
}

void setPDPEntry(uint16_t i, PDP_t *PDP, bool Present, bool RW, bool US, bool PWT,
		bool PCD, bool A, uint16_t AVL, bool NX, paddr_t Address)
{
	PDP->PDPE[i] = (Present & 1);
	PDP->PDPE[i] |= (RW & 1) << 1;
	PDP->PDPE[i] |= (US & 1) << 2;
	PDP->PDPE[i] |= (PWT & 1) << 3;
	PDP->PDPE[i] |= (PCD & 1) << 4;
	PDP->PDPE[i] |= (A & 1) << 5;
	PDP->PDPE[i] |= (AVL & 0x7LL) << 9;
	PDP->PDPE[i] |= Address & 0xFFFFFFFFFF000LL;
	PDP->PDPE[i] |= ((AVL >> 3) & 0x7FFLL) <<52;
	PDP->PDPE[i] |= ((NX & cpuInfo.nx) & 1LL) << 63;
}

void setPDEntry(uint16_t i, PD_t *PD, bool Present, bool RW, bool US, bool PWT,
		bool PCD, bool A, uint16_t AVL, bool NX, paddr_t Address)
{
	PD->PDE[i] = (Present & 1);
	PD->PDE[i] |= (RW & 1) << 1;
	PD->PDE[i] |= (US & 1) << 2;
	PD->PDE[i] |= (PWT & 1) << 3;
	PD->PDE[i] |= (PCD & 1) << 4;
	PD->PDE[i] |= (A & 1) << 5;
	PD->PDE[i] |= (AVL & 0x7LL) << 9;
	PD->PDE[i] |= Address & 0xFFFFFFFFFF000LL;
	PD->PDE[i] |= ((AVL >> 3) & 0x7FFLL) <<52;
	PD->PDE[i] |= ((NX & cpuInfo.nx) & 1LL) << 63;
}

void setPTEntry(uint16_t i, PT_t *PT, bool Present, bool RW, bool US, bool PWT,
		bool PCD, bool A, bool D, bool G, uint16_t AVL,
		bool PAT, bool NX, paddr_t Address)
{
	PT->PTE[i] = (Present & 1);
	PT->PTE[i] |= (RW & 1) << 1;
	PT->PTE[i] |= (US & 1) << 2;
	PT->PTE[i] |= (PWT & 1) << 3;
	PT->PTE[i] |= (PCD & 1) << 4;
	PT->PTE[i] |= (A & 1) << 5;
	PT->PTE[i] |= (D & 1) << 6;
	PT->PTE[i] |= (PAT & 1) << 7;
	PT->PTE[i] |= ((G & cpuInfo.GlobalPage) & 1LL) << 8;
	PT->PTE[i] |= (AVL & 0x7LL) << 9;
	PT->PTE[i] |= Address & 0xFFFFFFFFFF000LL;
	PT->PTE[i] |= ((AVL >> 3) & 0x7FFLL) << 52;
	PT->PTE[i] |= ((NX & cpuInfo.nx) & 1LL) << 63;
}

//XXX
void clearPML4Entry(uint16_t i, PML4_t *PML4)
{
	PML4->PML4E[i] = 0;
}

void clearPDPEntry(uint16_t i, PDP_t *PDP)
{
	PDP->PDPE[i] = 0;
}

void clearPDEntry(uint16_t i, PD_t *PD)
{
	PD->PDE[i] = 0;
}

void clearPTEntry(uint16_t i, PT_t *PT)
{
	PT->PTE[i] = 0;
}

/*
 * Invalidiert eine Page im TLB der CPU
 * Parameter:	Address = virt. Addresse der Page
 */
void InvalidateTLBEntry(void *Address)
{
	asm volatile("invlpg (%0)" : :"r" (Address));
}

inline void FlushTLB() {
	cpu_writeControlRegister(CPU_CR3, cpu_readControlRegister(CPU_CR3));
}
