/*
 * vmm.c
 *
 *  Created on: 09.07.2012
 *      Author: pascal
 */

/*
 * vmm_SysPMMAlloc(): Rueckgabewerte: 1=Kein freier Speicherplatz mehr vorhanden
 */
#include "vmm.h"
#include "pmm.h"
#include "memory.h"
#include "display.h"
#include "paging.h"
#include "stdlib.h"
#include "string.h"
#include "lock.h"
#include "cpu.h"
#include <assert.h>

#define NULL (void*)0

#define VMM_SIZE_PER_PAGE	MM_BLOCK_SIZE
#define VMM_SIZE_PER_TABLE	4096
#define VMM_MAX_ADDRESS		MAX_ADDRESS

//Flags für AVL Bits
#define VMM_PAGE_FULL			(1 << 0)
#define VMM_GUARD_PAGE			(1 << 1)

#define VMM_PAGES_PER_PML4		VMM_PAGES_PER_PDP * PAGE_ENTRIES
#define VMM_PAGES_PER_PDP		VMM_PAGES_PER_PD * PAGE_ENTRIES
#define VMM_PAGES_PER_PD		VMM_PAGES_PER_PT * PAGE_ENTRIES
#define VMM_PAGES_PER_PT		PAGE_ENTRIES

#define VMM_EXTEND(address)	((int64_t)((address) << 16) >> 16)
#define VMM_GET_ADDRESS(PML4i, PDPi, PDi, PTi)	(void*)VMM_EXTEND(((uint64_t)PML4i << 39) | ((uint64_t)PDPi << 30) | (PDi << 21) | (PTi << 12))
#define VMM_ALLOCATED(entry) ((entry & PG_P) || (PG_AVL(entry) & (VMM_UNUSED_PAGE | VMM_GUARD_PAGE)))	//Prüft, ob diese Page schon belegt ist

#define PAGE_TABLE_INDEX(address, level)	(((uintptr_t)(address) >> (12 + (3 - (level)) * 9)) & 0x1FF)
#define PML4_INDEX(address)					PAGE_TABLE_INDEX(address, 0)
#define PDP_INDEX(address)					PAGE_TABLE_INDEX(address, 1)
#define PD_INDEX(address)					PAGE_TABLE_INDEX(address, 2)
#define PT_INDEX(address)					PAGE_TABLE_INDEX(address, 3)

#define MAPPED_PHYS_MEM_BASE		0xFFFFFF8000000000
#define MAPPED_PHYS_MEM_GET(addr)	((void*)(MAPPED_PHYS_MEM_BASE + (paddr_t)(addr)))

struct vmm_context {
	paddr_t physAddress;
};

const uint16_t PML4e = PML4_INDEX(KERNELSPACE_END) + 1;
const uint16_t PDPe = PDP_INDEX(KERNELSPACE_END) + 1;
const uint16_t PDe = PD_INDEX(KERNELSPACE_END) + 1;
const uint16_t PTe = PT_INDEX(KERNELSPACE_END) + 1;

static lock_t vmm_lock = LOCK_INIT;

context_t kernel_context;

//Funktionen, die nur in dieser Datei aufgerufen werden sollen
uint8_t vmm_ChangeMap(context_t *context, void *vAddress, paddr_t pAddress, uint8_t flags, uint16_t avl);

/*
 * Löscht eine (virtuelle) Page.
 * Parameter:	address = virtuelle Addresse der Page
 */
static void clearPage(void *address)
{
	asm volatile("rep stosq" : :"c"(VMM_SIZE_PER_PAGE / sizeof(uint64_t)), "D"((uintptr_t)address & ~0xFFF), "a"(0) :"memory");
}

/*
 * Gibt zurück, ob die entsprechende Page belegt ist oder nicht
 * Parameter:		Address = virtuelle Addresse
 * Rückgabewert:	true = Page ist frei
 * 					false = Page ist belegt
 */
static bool isPageFree(const uint64_t *const PML4E, const uint64_t *const PDPE, const uint64_t *const PDE, const uint64_t *const PTE)
{
	//PML4 Eintrag überprüfen
	if((*PML4E & PG_P) == 0)		//Wenn PML4-Eintrag vorhanden ist
		return true;
	//Ansonsten überprüfe PDP-Eintrag
	else if((*PDPE & PG_P) == 0)	//Wenn PDP-Eintrag vorhanden ist
		return true;
	//Ansonsten überprüfe PD-Eintrag
	else if((*PDE & PG_P) == 0)		//Wenn PD-Eintrag vorhanden ist
		return true;
	//Ansonsten überprüfe PT-Eintrag
	return !VMM_ALLOCATED(*PTE);
}

static uint8_t map_entry(void *vAddress, paddr_t pAddress, uint8_t flags, uint16_t avl, PageTable_t table, uint32_t level)
{
	uint32_t index = PAGE_TABLE_INDEX(vAddress, level);
	PageTable_t next_table = MAPPED_PHYS_MEM_GET(table[index].Address << 12);

	if(!table[index].P) {
		table[index].entry = 0;

		if(level == 3) {
			table[index].P = !(avl & (VMM_UNUSED_PAGE | VMM_GUARD_PAGE));
			table[index].RW = flags & VMM_FLAGS_WRITE;
			table[index].US = flags & VMM_FLAGS_USER;
			table[index].PWT = flags & VMM_FLAGS_PWT;
			table[index].PCD = flags & VMM_FLAGS_NO_CACHE;
			table[index].G = flags & VMM_FLAGS_GLOBAL;
			table[index].AVL1 = avl;
			table[index].AVL2 = avl >> 3;
			table[index].Address = pAddress >> 12;
			table[index].NX = flags & VMM_FLAGS_NX;
			return 0;
		} else {
			paddr_t address = pmm_Alloc();
			if(address == 1) return 1;

			table[index].P = 1;
			table[index].PWT = 1;
			table[index].RW = 1;			// Always mark as writable so that this recursive mapping method works
			table[index].Address = address >> 12;

			next_table = MAPPED_PHYS_MEM_GET(address);
			clearPage(next_table);
		}
	} else if (level == 3) {
		return 2;
	}

	// Potentially relax restriction
	table[index].US |= flags & VMM_FLAGS_USER;
	// There is no need for TLB invalidation here as the CPU will rewalk the page tables if it encounters a restriction validation

	uint8_t res = map_entry(vAddress, pAddress, flags, avl, next_table, level + 1);

	//Full flags setzen
	uint16_t i;
	for(i = 0; i < PAGE_ENTRIES; i++) {
		if(!VMM_ALLOCATED(table[i].entry))
			break;
	}
	if(i == PAGE_ENTRIES) {
		table[index].AVL1 |= VMM_PAGE_FULL;
		table[index].AVL2 |= VMM_PAGE_FULL >> 3;
	}

	return res;
}

static uint8_t map(context_t *context, void *vAddress, paddr_t pAddress, uint8_t flags, uint16_t avl) {
	return map_entry(vAddress, pAddress, flags, avl, MAPPED_PHYS_MEM_GET(context->physAddress), 0);
}

static void *getFreePages(context_t *context, void *start, void *end, size_t pages)
{
	PML4_t *const PML4 = MAPPED_PHYS_MEM_GET(context->physAddress);

	size_t found_pages = 0;

	//Einträge in die Page Tabellen
	uint16_t start_PML4i = PML4_INDEX(start);
	uint16_t start_PDPi = PDP_INDEX(start);
	uint16_t start_PDi = PD_INDEX(start);
	uint16_t start_PTi = PT_INDEX(start);
	uint16_t end_PML4i = PML4_INDEX(end);
	uint16_t end_PDPi = PDP_INDEX(end);
	uint16_t end_PDi = PD_INDEX(end);
	uint16_t end_PTi = PT_INDEX(end);

	//Anfang des zusammenhängenden Bereichs
	uint16_t begin_PML4i = start_PML4i;
	uint16_t begin_PDPi = start_PDPi;
	uint16_t begin_PDi = start_PDi;
	uint16_t begin_PTi = start_PTi;

	for (uint16_t PML4i = start_PML4i; PML4i <= end_PML4i; PML4i++)
	{
		if((PML4->PML4E[PML4i] & PG_P))
		{
			//Es sind noch Pages frei
			if(!(PG_AVL(PML4->PML4E[PML4i]) & VMM_PAGE_FULL))
			{
				PDP_t *const PDP = MAPPED_PHYS_MEM_GET(PML4->PML4E[PML4i] & PG_ADDRESS);
				uint16_t last_PDPi = (PML4i == end_PML4i) ? end_PDPi : PAGE_ENTRIES - 1;
				for(uint16_t PDPi = (PML4i == start_PML4i) ? start_PDPi : 0; PDPi <= last_PDPi; PDPi++)
				{
					if((PDP->PDPE[PDPi] & PG_P))
					{
						//Es sind noch Pages frei
						if(!(PG_AVL(PDP->PDPE[PDPi]) & VMM_PAGE_FULL))
						{
							PD_t *const PD = MAPPED_PHYS_MEM_GET(PDP->PDPE[PDPi] & PG_ADDRESS);
							uint16_t last_PDi = (PML4i == end_PML4i && PDPi == end_PDPi) ? end_PDi : PAGE_ENTRIES - 1;
							for(uint16_t PDi = (PML4i == start_PML4i && PDPi == start_PDPi) ? start_PDi : 0; PDi <= last_PDi; PDi++)
							{
								if((PD->PDE[PDi] & PG_P))
								{
									//Es sind noch Pages frei
									if(!(PG_AVL(PD->PDE[PDi]) & VMM_PAGE_FULL))
									{
										PT_t *const PT = MAPPED_PHYS_MEM_GET(PD->PDE[PDi] & PG_ADDRESS);
										uint16_t last_PTi = (PML4i == end_PML4i && PDPi == end_PDPi && PDi == end_PDi) ? end_PTi : PAGE_ENTRIES - 1;
										for(uint16_t PTi = (PML4i == start_PML4i && PDPi == start_PDPi && PDi == start_PDi) ? start_PTi : 0;
												PTi <= last_PTi; PTi++)
										{
											if(VMM_ALLOCATED(PT->PTE[PTi]))
											{
												found_pages = 0;
												continue;
											}
											else
											{
												if(found_pages == 0)
												{
													begin_PTi = PTi;
													begin_PDi = PDi;
													begin_PDPi = PDPi;
													begin_PML4i = PML4i;
												}
												found_pages++;
											}

											if(found_pages >= pages)
												return VMM_GET_ADDRESS(begin_PML4i, begin_PDPi, begin_PDi, begin_PTi);
										}
									}
									else
									{
										found_pages = 0;
										continue;
									}
								}
								else
								{
									if(found_pages == 0)
									{
										begin_PTi = 0;
										begin_PDi = PDi;
										begin_PDPi = PDPi;
										begin_PML4i = PML4i;
									}
									found_pages += VMM_PAGES_PER_PT;
								}

								if(found_pages >= pages)
									return VMM_GET_ADDRESS(begin_PML4i, begin_PDPi, begin_PDi, begin_PTi);
							}
						}
						else
						{
							found_pages = 0;
							continue;
						}
					}
					else
					{
						if(found_pages == 0)
						{
							begin_PTi = 0;
							begin_PDi = 0;
							begin_PDPi = PDPi;
							begin_PML4i = PML4i;
						}
						found_pages += VMM_PAGES_PER_PD;
					}

					if(found_pages >= pages)
						return VMM_GET_ADDRESS(begin_PML4i, begin_PDPi, begin_PDi, begin_PTi);
				}
			}
			else
			{
				found_pages = 0;
				continue;
			}
		}
		else
		{
			if(found_pages == 0)
			{
				begin_PTi = 0;
				begin_PDi = 0;
				begin_PDPi = 0;
				begin_PML4i = PML4i;
			}
			found_pages += VMM_PAGES_PER_PDP;
		}

		if(found_pages >= pages)
			return VMM_GET_ADDRESS(begin_PML4i, begin_PDPi, begin_PDi, begin_PTi);
	}

	return NULL;
}

static uint8_t unmap_entry(void *vAddress, PageTable_t table, uint32_t level, paddr_t *pAddress) {
	const uint8_t FLAG_CLEAR_FULL_FLAG = 1 << 0;
	const uint8_t FLAG_DELETE_TABLE = 1 << 1;

	uint32_t index = PAGE_TABLE_INDEX(vAddress, level);

	if (level < 3) {
		if (!table[index].P) return FLAG_CLEAR_FULL_FLAG;

		PageTable_t next_table = MAPPED_PHYS_MEM_GET(table[index].Address << 12);
		uint8_t res = unmap_entry(vAddress, next_table, level + 1, pAddress);
		if (res & FLAG_DELETE_TABLE) {
			pmm_Free(table[index].Address << 12);
		} else {
			if (res & FLAG_CLEAR_FULL_FLAG) {
				// Clear full flag
				table[index].AVL1 &= ~VMM_PAGE_FULL;
				table[index].AVL2 &= ~(VMM_PAGE_FULL >> 3);
			}
			return res;
		}
	} else {
		*pAddress = table[index].Address << 12;
	}

	table[index].entry = 0;
	if (level == 3) {
		InvalidateTLBEntry(vAddress);
	}

	// Check if we still need this page table
	if (level > 0) {
		for (uint16_t i = 0; i < PAGE_ENTRIES; i++) {
			if (table[i].P || (PG_AVL(table[i].entry) & VMM_UNUSED_PAGE)) {
				return FLAG_CLEAR_FULL_FLAG;
			}
		}
	}

	return FLAG_DELETE_TABLE | FLAG_CLEAR_FULL_FLAG;
}

static paddr_t unmap(context_t *context, void *vAddress)
{
	paddr_t pAddress = 0;
	unmap_entry(vAddress, MAPPED_PHYS_MEM_GET(context->physAddress), 0, &pAddress);
	return pAddress;
}

/*
 * Initialisiert die virtuelle Speicherverwaltung.
 * Parameter:	Speicher = Grösse des phys. Speichers
 * 				Stack = Zeiger auf den Stack
 */
bool vmm_Init()
{
	extern uint8_t kernel_start, kernel_end, kernel_code_start, kernel_code_end;

	PML4_t *PML4 = (PML4_t*)(cpu_readControlRegister(CPU_CR3) & 0xFFFFFFFFFF000);	//nur die Adresse wollen wir haben

	//Ersten Eintrag überarbeiten
	setPML4Entry(0, PML4, 1, 1, 0, 0, 0, 0, 0, 0, PML4->PML4E[0] & PG_ADDRESS);

	// Map first 512GiB to the end of the virtual memory. For this we are using 2MiB pages initially
	// Currently the first 16MiB are identity mapped
	// For this we temporarily map the pml4 as pdp in the second entry of the pml4
	setPML4Entry(1, PML4, 1, 1, 0, 0, 0, 0, 0, 1, (uintptr_t)PML4);
	setPML4Entry(PML4_INDEX(MAPPED_PHYS_MEM_BASE), PML4, 1, 1, 0, 0, 0, 0, 0, 1, pmm_Alloc());
	PageTable_t pdp = (PageTable_t)(((1ul << 27) | (1 << 18) | (1 << 9) | PML4_INDEX(MAPPED_PHYS_MEM_BASE)) << 12);
	if (cpuInfo.page_size_1gb) {
		// We use 1GiB pages
		paddr_t paddr = 0;
		for (uint32_t i = 0; i < 512; i++) {
			PageTableEntry_t pdp_entry = {
				.P = true,
				.RW = true,
				.PS_PAT = true,
				.G = true,
				.NX = true,
				.Address = paddr >> 12
			};
			*(pdp++) = pdp_entry;
			paddr += MM_BLOCK_SIZE * 512 * 512;
		}
	} else {
		// We use 2MiB pages
		paddr_t highest_address = pmm_getHighestAddress();
		paddr_t paddr = 0;
		for (uint32_t i = 0; i < 512 && paddr < highest_address; i++) {
			PageTableEntry_t pdp_entry = {
				.P = true,
				.RW = true,
				.NX = true,
				.Address = pmm_Alloc() >> 12
			};
			*(pdp++) = pdp_entry;
			PageTable_t pd = (PageTable_t)(((1ul << 27) | (1 << 18) | (PML4_INDEX(MAPPED_PHYS_MEM_BASE) << 9) | i) << 12);
			for (uint32_t j = 0; j < 512 && paddr < highest_address; j++) {
				PageTableEntry_t pd_entry = {
					.P = true,
					.RW = true,
					.PS_PAT = true,
					.G = true,
					.NX = true,
					.Address = paddr >> 12
				};
				*(pd++) = pd_entry;
				paddr += MM_BLOCK_SIZE * 512;
			}
		}
	}

	// Unmap PML4 again
	clearPML4Entry(1, PML4);
	// Clear TLB
	cpu_writeControlRegister(CPU_CR3, PML4);

	//PML4 in Kernelkontext eintragen
	kernel_context.physAddress = (uintptr_t)PML4;

	PML4 = MAPPED_PHYS_MEM_GET(PML4);

	// Mark all pages tables as reserved
	vmm_getPageTables(pmm_markPageReserved);

	//Speicher bis 1MB bearbeiten
	//Addresse 0 ist nicht gemappt
	unmap(&kernel_context, NULL);
	for(uint8_t *i = (uint8_t*)0x1000; i < (uint8_t*)0x100000; i += 0x1000)
	{
		vmm_ChangeMap(&kernel_context, i, (paddr_t)i, VMM_FLAGS_GLOBAL | VMM_FLAGS_NX | VMM_FLAGS_WRITE, 0);
	}
	uint8_t *i;
	for(i = &kernel_start; i <= &kernel_end; i += 0x1000)
	{
		if(i >= &kernel_code_start && i <= &kernel_code_end)
		{
			vmm_ChangeMap(&kernel_context, i, vmm_getPhysAddress(&kernel_context, i), VMM_FLAGS_GLOBAL, 0);
		}
		else
		{
			vmm_ChangeMap(&kernel_context, i, vmm_getPhysAddress(&kernel_context, i), VMM_FLAGS_GLOBAL | VMM_FLAGS_NX | VMM_FLAGS_WRITE, 0);
		}
	}
	//Den restlichen virtuellen Speicher freigeben
	while(!vmm_getPageStatus(&kernel_context, i))
	{
		unmap(&kernel_context, i);
		i += 0x1000;
	}

	SysLog("VMM", "Initialisierung abgeschlossen");
	return true;
}

//Userspace Funktionen
/*
 * Reserviert ein Speicherblock mit der Blockgrösse Length (in Pages)
 * Parameter:		Length = Die Anzahl Pages, die reserviert werden sollen
 * Rückgabewerte:	Adresse zum Speicherblock
 * 					NULL = Ein Fehler ist aufgetreten
 */
void *vmm_Alloc(context_t *context, size_t Length)
{
	return vmm_Map(context, NULL, 0, Length, VMM_FLAGS_WRITE | VMM_FLAGS_USER | VMM_FLAGS_NX);
}

/*
 * Gibt Speicherplatz im Userspace frei
 * Parameter:	vAddress = Virtuelle Adresse, an die der Block anfängt
 * 				Pages = Anzahl Pages, die dieser Block umfasst
 */
void vmm_Free(context_t *context, void *vAddress, size_t Pages)
{
	vmm_UnMap(context, vAddress, Pages, true);
}

//------------------------Systemfunktionen---------------------------
/*
 * Reserviert Speicherplatz für den Kernel an
 * der Addresse vAddress und der Länge Length.
 * Params:
 * vAddress =	Virtuelle Addresse, an die der Speicherplatz gemappt werden soll (fakultativ)
 * 				Length = Anzahl Pages die reserviert werden sollen (mind. 1)
 * 				Ignore = Gibt an ob der Parameter vAddress ignoriert (1) werden soll oder nicht (0)
 *
 * Rückgabewert:	virt. Addresse des angeforderten Speicherbereichs
 * 					NULL bei Fehler
 */
void *vmm_SysAlloc(size_t Length)
{
	return vmm_Map(&kernel_context, NULL, 0, Length, VMM_FLAGS_WRITE | VMM_FLAGS_GLOBAL | VMM_FLAGS_NX);
}

/*
 * Gibt den für den Stack der Physikalischen Speicherverwaltung reservierten Speicherplatz
 * an der Addresse vAddress und der Länge Length frei.
 * Params:
 * vAddress = Virtuelle Addresse des Speicherplatzes, der freigegeben werden soll
 * Length = Anzahl Pages die freigegeben werden sollen
 */
void vmm_SysFree(void *vAddress, size_t Length)
{
	vmm_UnMap(&kernel_context, vAddress, Length, true);
}

/*
 * Reserviert Speicher für DMA und mappt diesen
 * Params:	maxAddress = maximale physische Adresse für den Speicherbereich
 * 			size = Anzahl Pages des Speicherbereichs
 * 			phys = Zeiger auf Variable, in der die phys. Adresse geschrieben wird
 */
void *vmm_AllocDMA(paddr_t maxAddress, size_t Size, paddr_t *Phys)
{
	//Freie virt. Adresse finden
	uint64_t Pages = Size;

	//Physischen Speicher allozieren
	*Phys = pmm_AllocDMA(maxAddress, Size);
		if(*Phys == 1) return NULL;

	return vmm_Map(&kernel_context, NULL, *Phys, Pages, VMM_FLAGS_NO_CACHE | VMM_FLAGS_NX | VMM_FLAGS_WRITE);
}

static void walkPageTable(PageTable_t table, uint32_t level, void(*callback)(paddr_t)) {
	const uint32_t end = PAGE_ENTRIES - (level == 0);
	for (uint32_t i = 0; i < end; i++) {
		if (table[i].P) {
			paddr_t page = table[i].Address << 12;
			callback(page);

			if (level < 2) {	// We don't walk though the PTs as we only want the page tables
				PageTable_t next_table = (PageTable_t)MAPPED_PHYS_MEM_GET(page);
				walkPageTable(next_table, level + 1, callback);
			}
		}
	}
}

void vmm_getPageTables(void(*callback)(paddr_t)) {
	paddr_t pml4 = cpu_readControlRegister(CPU_CR3) & PG_ADDRESS;
	callback(pml4);
	walkPageTable((PageTable_t)MAPPED_PHYS_MEM_GET(pml4), 0, callback);
}

//----------------------Allgemeine Funktionen------------------------
static void *mapPages(context_t *context, void *vAddress, paddr_t pAddress, size_t pages, uint8_t flags, bool guard) {
	bool unused = pAddress == 0;
	bool allocate = flags & VMM_FLAGS_ALLOCATE;
	uint16_t avl = (unused && !allocate) ? VMM_UNUSED_PAGE : 0;

	if (pages == 0) return NULL;

	if (guard) pages += MM_GUARD_PAGES * 2;

	return LOCKED_RESULT(vmm_lock, {
		if (vAddress == NULL) {
			void *start = (void*)((flags & VMM_FLAGS_USER) ? USERSPACE_START : KERNELSPACE_START);
			void *end = (void*)((flags & VMM_FLAGS_USER) ? USERSPACE_END : KERNELSPACE_END);
			vAddress = getFreePages(context, start, end, pages);
		}

		bool error = false;
		size_t i;
		for (i = 0; i < pages; i++) {
			paddr_t paddr = 0;
			uintptr_t currentOffset = i * VMM_SIZE_PER_PAGE;
			uint16_t actual_avl = avl;
			uint8_t actual_flags = flags;
			bool is_guard_page = guard && (i < MM_GUARD_PAGES || i >= pages - MM_GUARD_PAGES);
			if (is_guard_page) {
				actual_avl = VMM_GUARD_PAGE;
				actual_flags = 0;
			} else if (allocate) {
				paddr = pmm_Alloc();
				if (paddr == 1) {
					error = true;
					break;
				}
			} else {
				paddr = pAddress + currentOffset * !unused;
			}

			if (map(context, vAddress + currentOffset, paddr, actual_flags, actual_avl) != 0) {
				error = true;
				break;
			}
			if (allocate && !is_guard_page) clearPage(vAddress + currentOffset);
		}

		if (error) {
			for (size_t j = 0; j < i; j++) {
				paddr_t paddr = unmap(context, vAddress + j * VMM_SIZE_PER_PAGE);
				if (allocate) pmm_Free(paddr);
			}
		}
		error ? NULL : (vAddress + guard * MM_GUARD_PAGES * VMM_SIZE_PER_PAGE);
	});
}

void *vmm_Map(context_t *context, void *vAddress, paddr_t pAddress, size_t pages, uint8_t flags) {
	return mapPages(context, vAddress, pAddress, pages, flags, false);
}

void *vmm_MapGuarded(context_t *context, void *vAddress, paddr_t pAddress, size_t pages, uint8_t flags) {
	return mapPages(context, vAddress, pAddress, pages, flags, true);
}

static void unmapPages(context_t *context, void *vAddress, size_t pages, bool freePages, bool guard) {
	pages += guard ? MM_GUARD_PAGES * 2 : 0;
	LOCKED_TASK(vmm_lock, {
		for(size_t i = 0; i < pages; i++)
		{
			paddr_t pAddress = unmap(context, vAddress + i * VMM_SIZE_PER_PAGE);
			if(freePages && !(guard && (i < MM_GUARD_PAGES || i >= pages - MM_GUARD_PAGES)))
				pmm_Free(pAddress);
		}
	});
}

void vmm_UnMap(context_t *context, void *vAddress, size_t pages, bool freePages) {
	return unmapPages(context, vAddress, pages, freePages, false);
}

void vmm_UnMapGuarded(context_t *context, void *vAddress, size_t pages, bool freePages) {
	return unmapPages(context, vAddress, pages, freePages, true);
}

/*
 * Ändert das Mapping einer Page. Falls die Page nicht vorhanden ist, wird sie gemappt
 * Params:			vAddress = Neue virt. Addresse der Page
 * 					pAddress = Neue phys. Addresse der Page
 * 					flags = neue Flags der Page
 * Rückgabewert:	0 = Alles in Ordnung
 * 					1 = zu wenig phys. Speicher vorhanden
 */
uint8_t vmm_ChangeMap(context_t *context, void *vAddress, paddr_t pAddress, uint8_t flags, uint16_t avl)
{
	//Einträge in die Page Tabellen
	const uint16_t PML4i = PML4_INDEX(vAddress);
	const uint16_t PDPi = PDP_INDEX(vAddress);
	const uint16_t PDi = PD_INDEX(vAddress);
	const uint16_t PTi = PT_INDEX(vAddress);

	PML4_t *const PML4 = MAPPED_PHYS_MEM_GET(context->physAddress);
	PDP_t *const PDP = MAPPED_PHYS_MEM_GET(PML4->PML4E[PML4i] & PG_ADDRESS);
	PD_t *const PD = MAPPED_PHYS_MEM_GET(PDP->PDPE[PDPi] & PG_ADDRESS);
	PT_t *const PT = MAPPED_PHYS_MEM_GET(PD->PDE[PDi] & PG_ADDRESS);

	//Flags auslesen
	bool US = (flags & VMM_FLAGS_USER);
	bool G = (flags & VMM_FLAGS_GLOBAL);
	bool RW = (flags & VMM_FLAGS_WRITE);
	bool NX = (flags & VMM_FLAGS_NX);
	bool P = !(avl & VMM_UNUSED_PAGE);
	bool PCD = (flags & VMM_FLAGS_NO_CACHE);
	bool PWT = (flags & VMM_FLAGS_PWT);

	return LOCKED_RESULT(vmm_lock, {
		uint8_t res = 0;
		if(isPageFree(&PML4->PML4E[PML4i], &PDP->PDPE[PDPi], &PD->PDE[PDi], &PT->PTE[PTi]))
		{
			if(map(context, vAddress, pAddress, flags, avl) == 1)
			{
				res = 1;
			}
		}
		else
		{
			setPTEntry(PTi, PT, P, RW, US, PWT, PCD, 0, 0, G, avl, 0, NX, pAddress);

			//Reserved bits zurücksetzen
			PD->PDE[PDi] &= ~0x1C0;
			PDP->PDPE[PDPi] &= ~0x1C0;
			PML4->PML4E[PML4i] &= ~0x1C0;

			InvalidateTLBEntry((void*)vAddress);
		}
		res;
	});
}

/*
 * Mappt eine virtuelle Adresse an eine andere Adresse
 * Die src Addresse wird nicht auf Gültigkeit überprüft
 * Params:			src = virt. Addresse der Speicherstelle
 * 					dst = virt. Addresse an die remappt werden soll
 * 					length = Anzahl Pages, die die Speicherstelle lang ist
 * 					us = ist das Ziel ein Userspace?
 *
 * Rückgabewert:	0 = Speicherstelle wurde erfolgreich virt. verschoben
 * 					1 = zu wenig phys. Speicherplatz vorhanden
 * 					2 = Destinationaddresse ist schon belegt
 *///TODO: Bei Fehler alles Rückgängig machen
//FIXME: correctly handle pages which are allocated but have not been mapped yet (VMM_UNUSED_PAGE)
uint8_t vmm_ReMap(context_t *src_context, void *src, context_t *dst_context, void *dst, size_t length, uint8_t flags, uint16_t avl)
{
	size_t i;
	for(i = 0; i < length; i++)
	{
		uint8_t r;
		if((r = map(dst_context, dst + i * VMM_SIZE_PER_PAGE, vmm_getPhysAddress(src_context, src + i * VMM_SIZE_PER_PAGE) ? : pmm_Alloc(), flags, avl)) != 0)
			return r;
		unmap(src_context, src + i * VMM_SIZE_PER_PAGE);
	}
	return 0;
}

/*
 * Sucht die zugehörigen virtuelle Adresse der übergebenen phys. Adresse
 * Parameter:		pAddress = die phys. Addresse der zu suchenden virt. Adresse
 * Rückgabewert:	virtuelle Adresse der phys. Adresse
 * 					0 = phys. Adresse wurde nicht gemappt
 */
/*uintptr_t vmm_getVirtAddress(uintptr_t pAddress)
{
	register uintptr_t i;
	for(i = 0; i < VMM_MAX_ADDRESS; i += VMM_SIZE_PER_PAGE)
	{
		if(vmm_getPhysAddress(i) == pAddress)
			return i;
	}
	return 0;
}*/

//Hilfsfunktionen
/*
 * Gibt zurück, ob die entsprechende Page belegt ist oder nicht
 * Parameter:		Address = virtuelle Addresse
 * Rückgabewert:	true = Page ist frei
 * 					false = Page ist belegt
 */
bool vmm_getPageStatus(context_t *context, void *Address)
{
	//Einträge in die Page Tabellen
	const uint16_t PML4i = PML4_INDEX(Address);
	const uint16_t PDPi = PDP_INDEX(Address);
	const uint16_t PDi = PD_INDEX(Address);
	const uint16_t PTi = PT_INDEX(Address);

	const PML4_t *const PML4 = MAPPED_PHYS_MEM_GET(context->physAddress);
	const PDP_t *const PDP = MAPPED_PHYS_MEM_GET(PML4->PML4E[PML4i] & PG_ADDRESS);
	const PD_t *const PD = MAPPED_PHYS_MEM_GET(PDP->PDPE[PDPi] & PG_ADDRESS);
	const PT_t *const PT = MAPPED_PHYS_MEM_GET(PD->PDE[PDi] & PG_ADDRESS);

	return isPageFree(&PML4->PML4E[PML4i], &PDP->PDPE[PDPi], &PD->PDE[PDi], &PT->PTE[PTi]);
}

paddr_t vmm_getPhysAddress(context_t *context, void *virtualAddress)
{
	//Einträge in die Page Tabellen
	const uint16_t PML4i = PML4_INDEX(virtualAddress);
	const uint16_t PDPi = PDP_INDEX(virtualAddress);
	const uint16_t PDi = PD_INDEX(virtualAddress);
	const uint16_t PTi = PT_INDEX(virtualAddress);

	const PML4_t *const PML4 = MAPPED_PHYS_MEM_GET(context->physAddress);
	const PDP_t *const PDP = MAPPED_PHYS_MEM_GET(PML4->PML4E[PML4i] & PG_ADDRESS);
	const PD_t *const PD = MAPPED_PHYS_MEM_GET(PDP->PDPE[PDPi] & PG_ADDRESS);
	const PT_t *const PT = MAPPED_PHYS_MEM_GET(PD->PDE[PDi] & PG_ADDRESS);

	return isPageFree(&PML4->PML4E[PML4i], &PDP->PDPE[PDPi], &PD->PDE[PDi], &PT->PTE[PTi]) ? 0 : (paddr_t)(PT->PTE[PTi] & PG_ADDRESS);
}

void vmm_unusePages(context_t *context, void *virt, size_t pages)
{
	for(void *address = virt; address < virt + pages * VMM_SIZE_PER_PAGE; address += VMM_SIZE_PER_PAGE)
	{
		//Einträge in die Page Tabellen
		const uint16_t PML4i = PML4_INDEX(address);
		const uint16_t PDPi = PDP_INDEX(address);
		const uint16_t PDi = PD_INDEX(address);
		const uint16_t PTi = PT_INDEX(address);

		const PML4_t *const PML4 = MAPPED_PHYS_MEM_GET(context->physAddress);
		const PDP_t *const PDP = MAPPED_PHYS_MEM_GET(PML4->PML4E[PML4i] & PG_ADDRESS);
		const PD_t *const PD = MAPPED_PHYS_MEM_GET(PDP->PDPE[PDPi] & PG_ADDRESS);
		PT_t *const PT = MAPPED_PHYS_MEM_GET(PD->PDE[PDi] & PG_ADDRESS);

		if(!isPageFree(&PML4->PML4E[PML4i], &PDP->PDPE[PDPi], &PD->PDE[PDi], &PT->PTE[PTi]) && (PG_AVL(PT->PTE[PTi]) & VMM_UNUSED_PAGE) == 0)
		{
			paddr_t entry = PT->PTE[PTi];
			pmm_Free(entry & PG_ADDRESS);
			setPTEntry(PTi, PT, 0, !!(entry & PG_RW), !!(entry & PG_US), !!(entry & PG_PWT), !!(entry & PG_PCD), !!(entry & PG_A),
					!!(entry & PG_D), !!(entry & PG_G), PG_AVL(entry) | VMM_UNUSED_PAGE, !!(entry & PG_PAT), !!(entry & PG_NX), 0);
			InvalidateTLBEntry(address);
		}
	}
}

void vmm_usePages(context_t *context, void *virt, size_t pages)
{
	void *address = (void*)((uintptr_t)virt & ~0xFFF);

	for(; address < (void*)((uintptr_t)virt & ~0xFFF) + pages * VMM_SIZE_PER_PAGE; address += VMM_SIZE_PER_PAGE)
	{
		//Einträge in die Page Tabellen
		const uint16_t PML4i = PML4_INDEX(address);
		const uint16_t PDPi = PDP_INDEX(address);
		const uint16_t PDi = PD_INDEX(address);
		const uint16_t PTi = PT_INDEX(address);

		const PML4_t *const PML4 = MAPPED_PHYS_MEM_GET(context->physAddress);
		const PDP_t *const PDP = MAPPED_PHYS_MEM_GET(PML4->PML4E[PML4i] & PG_ADDRESS);
		const PD_t *const PD = MAPPED_PHYS_MEM_GET(PDP->PDPE[PDPi] & PG_ADDRESS);
		PT_t *const PT = MAPPED_PHYS_MEM_GET(PD->PDE[PDi] & PG_ADDRESS);

		uint64_t entry = PT->PTE[PTi];
		paddr_t pAddr = pmm_Alloc();
		if(pAddr == 1)
			Panic("VMM", "Out of memory!");

		setPTEntry(PTi, PT, 1, !!(entry & PG_RW), !!(entry & PG_US), !!(entry & PG_PWT), !!(entry & PG_PCD), !!(entry & PG_A),
				!!(entry & PG_D), !!(entry & PG_G), PG_AVL(entry) & ~VMM_UNUSED_PAGE, !!(entry & PG_PAT), !!(entry & PG_NX), pAddr);
		InvalidateTLBEntry(address);
		clearPage(address);
	}
}


//Prozesse

/*
 * Überprüft, ob ein Pointer in den Userspace bereich zeigt,
 * Parameter:	Den zu überprüfenden Pointer
 * Rückgabe:	True, wenn der Pointer in den Userspace zeigt, ansonsten false
 */
bool vmm_userspacePointerValid(const void *ptr, const size_t size)
{
	return (USERSPACE_START <= (uintptr_t)ptr && (uintptr_t)ptr + size <= USERSPACE_END);
}

/*
 * Erstellt einen neuen virtuellen Adressraum und intialisiert diesen
 */
context_t *createContext()
{
	context_t *context = malloc(sizeof(context_t));
	context->physAddress = pmm_Alloc();
	PML4_t *newPML4 = memset(MAPPED_PHYS_MEM_GET(context->physAddress), 0, MM_BLOCK_SIZE);

	//Kernel in den Adressraum einbinden
	PML4_t *const PML4 = MAPPED_PHYS_MEM_GET(kernel_context.physAddress);

	for(uint16_t PML4i = 0; PML4i < PML4e; PML4i++)
		newPML4->PML4E[PML4i] = PML4->PML4E[PML4i];

	//Den letzten Eintrag verwenden wir als Zeiger auf den Anfang der Tabelle. Das ermöglicht das Editieren derselben.
	newPML4->PML4E[511] = PML4->PML4E[511];

	return context;
}

/*
 * Löscht einen virtuellen Adressraum
 */
void deleteContext(context_t *context)
{
	//Erst alle Pages des Kontextes freigeben
	PML4_t *PML4 = MAPPED_PHYS_MEM_GET(context->physAddress);
	for(uint16_t PML4i = PML4e; PML4i < PAGE_ENTRIES - 1; PML4i++)
	{
		//Ist der Eintrag gültig
		if(PML4->PML4E[PML4i] & PG_P)
		{
			//PDP mappen
			paddr_t PDP_phys = PML4->PML4E[PML4i] & PG_ADDRESS;
			PDP_t *PDP = MAPPED_PHYS_MEM_GET(PDP_phys);
			for(uint16_t PDPi = 0; PDPi < PAGE_ENTRIES; PDPi++)
			{
				//Ist der Eintrag gültig
				if(PDP->PDPE[PDPi] & PG_P)
				{
					//PD mappen
					paddr_t PD_phys = PDP->PDPE[PDPi] & PG_ADDRESS;
					PD_t *PD = MAPPED_PHYS_MEM_GET(PD_phys);
					for(uint16_t PDi = 0; PDi < PAGE_ENTRIES; PDi++)
					{
						//Ist der Eintrag gültig
						if(PD->PDE[PDi] & PG_P)
						{
							//PT mappen
							paddr_t PT_phys = PD->PDE[PDi] & PG_ADDRESS;
							PT_t *PT = MAPPED_PHYS_MEM_GET(PT_phys);
							for(uint16_t PTi = 0; PTi < PAGE_ENTRIES; PTi++)
							{
								//Ist die Page alloziiert
								if(PT->PTE[PTi] & PG_P)
									pmm_Free(PT->PTE[PTi] & PG_ADDRESS);
							}
							//PT löschen
							PD->PDE[PDi] = 0;
							pmm_Free(PT_phys);
						}
					}
					//PD löschen
					PDP->PDPE[PDPi] = 0;
					pmm_Free(PD_phys);
				}
			}
			//PDP löschen
			PML4->PML4E[PML4i] = 0;
			pmm_Free(PDP_phys);
		}
	}

	//Restliche Datenstrukturen freigeben
	pmm_Free(context->physAddress);
	free(context);
}

/*
 * Aktiviert einen virtuellen Adressraum
 */
inline void activateContext(context_t *context)
{
	asm volatile("mov %0,%%cr3" : : "r"(context->physAddress));
}

int vmm_handlePageFault(context_t *context, void *page, uint64_t errorcode)
{
	uint16_t PML4i = PML4_INDEX(page);
	uint16_t PDPi = PDP_INDEX(page);
	uint16_t PDi = PD_INDEX(page);
	uint16_t PTi = PT_INDEX(page);

	const PML4_t *const PML4 = MAPPED_PHYS_MEM_GET(context->physAddress);
	const PDP_t *const PDP = MAPPED_PHYS_MEM_GET(PML4->PML4E[PML4i] & PG_ADDRESS);
	const PD_t *const PD = MAPPED_PHYS_MEM_GET(PDP->PDPE[PDPi] & PG_ADDRESS);
	const PT_t *const PT = MAPPED_PHYS_MEM_GET(PD->PDE[PDi] & PG_ADDRESS);

	//Activate unused pages
	if(!isPageFree(&PML4->PML4E[PML4i], &PDP->PDPE[PDPi], &PD->PDE[PDi], &PT->PTE[PTi]) && (PG_AVL(PT->PTE[PTi]) & VMM_UNUSED_PAGE))
	{
		vmm_usePages(context, page, 1);
		return 0;
	}
	if (PG_AVL(PT->PTE[PTi]) & VMM_GUARD_PAGE) {
		return 2;
	}
	return 1;
}
