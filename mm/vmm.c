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

#define NULL (void*)0

#define VMM_SIZE_PER_PAGE	MM_BLOCK_SIZE
#define VMM_SIZE_PER_TABLE	4096
#define VMM_MAX_ADDRESS		MAX_ADDRESS

//Flags für AVL Bits
#define VMM_PAGE_FULL			(1 << 0)

#define VMM_PAGES_PER_PML4		VMM_PAGES_PER_PDP * PAGE_ENTRIES
#define VMM_PAGES_PER_PDP		VMM_PAGES_PER_PD * PAGE_ENTRIES
#define VMM_PAGES_PER_PD		VMM_PAGES_PER_PT * PAGE_ENTRIES
#define VMM_PAGES_PER_PT		PAGE_ENTRIES

#define VMM_EXTEND(address)	((int64_t)((address) << 16) >> 16)
#define VMM_GET_ADDRESS(PML4i, PDPi, PDi, PTi)	(void*)VMM_EXTEND(((uint64_t)PML4i << 39) | ((uint64_t)PDPi << 30) | (PDi << 21) | (PTi << 12))
#define VMM_ALLOCATED(entry) ((entry & PG_P) || (PG_AVL(entry) & VMM_UNUSED_PAGE))	//Prüft, ob diese Page schon belegt ist

#define PAGE_TABLE_INDEX(address, level)	(((uintptr_t)(address) >> (12 + (3 - (level)) * 9)) & 0x1FF)
#define PML4_INDEX(address)					PAGE_TABLE_INDEX(address, 0)
#define PDP_INDEX(address)					PAGE_TABLE_INDEX(address, 1)
#define PD_INDEX(address)					PAGE_TABLE_INDEX(address, 2)
#define PT_INDEX(address)					PAGE_TABLE_INDEX(address, 3)

#define PML4_ADDRESS()					((PML4_t*)0xFFFFFFFFFFFFF000)
#define PDP_ADDRESS(PML4i)				((PDP_t*)(0xFFFFFFFFFFE00000 + (PML4i << 12)))
#define PD_ADDRESS(PML4i, PDPi)			((PD_t*)(0xFFFFFFFFC0000000 + (((uint64_t)PML4i << 21) | (PDPi << 12))))
#define PT_ADDRESS(PML4i, PDPi, PDi)	((PT_t*)(0xFFFFFF8000000000 + (((uint64_t)PML4i << 30) | ((uint64_t)PDPi << 21) | (PDi << 12))))

const uint16_t PML4e = PML4_INDEX(KERNELSPACE_END) + 1;
const uint16_t PDPe = PDP_INDEX(KERNELSPACE_END) + 1;
const uint16_t PDe = PD_INDEX(KERNELSPACE_END) + 1;
const uint16_t PTe = PT_INDEX(KERNELSPACE_END) + 1;

static lock_t vmm_lock = LOCK_INIT;

context_t kernel_context;

//Funktionen, die nur in dieser Datei aufgerufen werden sollen
uint8_t vmm_ChangeMap(void *vAddress, paddr_t pAddress, uint8_t flags, uint16_t avl);

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
	PageTable_t next_table = (PageTable_t)(((uintptr_t)table << 9) | (index << 12));

	if(!table[index].P) {
		table[index].entry = 0;

		if(level == 3) {
			table[index].P = !(avl & VMM_UNUSED_PAGE);
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

static uint8_t map(void *vAddress, paddr_t pAddress, uint8_t flags, uint16_t avl) {
	return map_entry(vAddress, pAddress, flags, avl, (PageTable_t)PML4_ADDRESS(), 0);
}

static void *getFreePages(void *start, void *end, size_t pages)
{
	PML4_t *const PML4 = PML4_ADDRESS();

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
				PDP_t *const PDP = PDP_ADDRESS(PML4i);
				uint16_t last_PDPi = (PML4i == end_PML4i) ? end_PDPi : PAGE_ENTRIES - 1;
				for(uint16_t PDPi = (PML4i == start_PML4i) ? start_PDPi : 0; PDPi <= last_PDPi; PDPi++)
				{
					if((PDP->PDPE[PDPi] & PG_P))
					{
						//Es sind noch Pages frei
						if(!(PG_AVL(PDP->PDPE[PDPi]) & VMM_PAGE_FULL))
						{
							PD_t *const PD = PD_ADDRESS(PML4i, PDPi);
							uint16_t last_PDi = (PML4i == end_PML4i && PDPi == end_PDPi) ? end_PDi : PAGE_ENTRIES - 1;
							for(uint16_t PDi = (PML4i == start_PML4i && PDPi == start_PDPi) ? start_PDi : 0; PDi <= last_PDi; PDi++)
							{
								if((PD->PDE[PDi] & PG_P))
								{
									//Es sind noch Pages frei
									if(!(PG_AVL(PD->PDE[PDi]) & VMM_PAGE_FULL))
									{
										PT_t *const PT = PT_ADDRESS(PML4i, PDPi, PDi);
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
	PageTable_t next_table = (PageTable_t)(((uintptr_t)table << 9) | (index << 12));

	if (!table[index].P)
		return 0;

	if (level < 3) {
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
	InvalidateTLBEntry(level == 3 ? vAddress : next_table);

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

static paddr_t unmap(void *vAddress)
{
	paddr_t pAddress;
	unmap_entry(vAddress, (PageTable_t)PML4_ADDRESS(), 0, &pAddress);
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
	uint64_t cr3;
	void *i;

	PML4_t *PML4;

	asm volatile("mov %%cr3,%0" : "=r" (cr3));
	PML4 = (PML4_t*)(cr3 & 0xFFFFFFFFFF000);	//nur die Adresse wollen wir haben

	//Lasse den letzten Eintrag der PML4 auf die PML4 selber zeigen
	setPML4Entry(511, PML4, 1, 1, 0, 1, 0, 0, 0, 1, (uintptr_t)PML4);

	//Ersten Eintrag überarbeiten
	setPML4Entry(0, PML4, 1, 1, 0, 1, 0, 0, 0, 0, PML4->PML4E[0] & PG_ADDRESS);

	//PML4 in Kernelkontext eintragen
	kernel_context.physAddress = (uintptr_t)PML4;
	kernel_context.virtualAddress = PML4_ADDRESS();

	PML4 = PML4_ADDRESS();

	//Speicher bis 1MB bearbeiten
	//Addresse 0 ist nicht gemappt
	unmap(NULL);
	for(i = (void*)0x1000; i < (void*)0x100000; i += 0x1000)
	{
		vmm_ChangeMap(i, (paddr_t)i, VMM_FLAGS_GLOBAL | VMM_FLAGS_NX | VMM_FLAGS_WRITE, 0);
	}
	for(i = (void*)&kernel_start; i <= (void*)&kernel_end; i += 0x1000)
	{
		if(i >= (void*)&kernel_code_start && i <= (void*)&kernel_code_end)
		{
			vmm_ChangeMap(i, vmm_getPhysAddress(i), VMM_FLAGS_GLOBAL, 0);
		}
		else
		{
			vmm_ChangeMap(i, vmm_getPhysAddress(i), VMM_FLAGS_GLOBAL | VMM_FLAGS_NX | VMM_FLAGS_WRITE, 0);
		}
	}
	//Den restlichen virtuellen Speicher freigeben
	while(!vmm_getPageStatus(i))
	{
		unmap(i);
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
void *vmm_Alloc(size_t Length)
{
	return vmm_Map(NULL, 0, Length, VMM_FLAGS_WRITE | VMM_FLAGS_USER | VMM_FLAGS_NX);
}

/*
 * Gibt Speicherplatz im Userspace frei
 * Parameter:	vAddress = Virtuelle Adresse, an die der Block anfängt
 * 				Pages = Anzahl Pages, die dieser Block umfasst
 */
void vmm_Free(void *vAddress, size_t Pages)
{
	vmm_UnMap(vAddress, Pages, true);
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
	return vmm_Map(NULL, 0, Length, VMM_FLAGS_WRITE | VMM_FLAGS_GLOBAL | VMM_FLAGS_NX);
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
	vmm_UnMap(vAddress, Length, true);
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

	return vmm_Map(NULL, *Phys, Pages, VMM_FLAGS_NO_CACHE | VMM_FLAGS_NX | VMM_FLAGS_WRITE);
}

list_t vmm_getTables(context_t *context)
{
	list_t list = list_create();
	PML4_t *PML4 = context->virtualAddress;
	PDP_t *PDP;
	PD_t *PD;

	//PML4 eintragen
	list_push(list, (void*)(context->physAddress));

	uint16_t PML4i, PDPi, PDi;
	//PML4 durchsuchen dabei aber den letzten Eintrag auslassen: er zeigt auf PML4 selber
	for(PML4i = 0; PML4i < 511; PML4i++)
	{
		//Wenn PDP nicht vorhanden dann auch nicht auflisten
		if(!(PML4->PML4E[PML4i] & PG_P))
			continue;
		//PDP auf die Liste setzen
		list_push(list, (void*)(PML4->PML4E[PML4i] & PG_ADDRESS));

		PDP = PDP_ADDRESS(PML4i);

		//Danach die PDP nach Einträgen durchsuchen
		for(PDPi = 0; PDPi < 512; PDPi++)
		{
			//Wenn PD nicht vorhanden dann auch nicht auflisten
			if(!(PDP->PDPE[PDPi] & PG_P))
				continue;

			//PD auf die Liste setzen
			list_push(list, (void*)(PDP->PDPE[PDPi] & PG_ADDRESS));

			PD = PD_ADDRESS(PML4i, PDPi);

			//Danach PD durchsuchen
			for(PDi = 0; PDi < 512; PDi++)
			{
				//Wenn PT nicht vorhanden dann auch nicht auflisten
				if(!(PD->PDE[PDi] & PG_P))
					continue;

				//PT auf die Liste setzen
				list_push(list, (void*)(PD->PDE[PDi] & PG_ADDRESS));
			}
		}
	}

	return list;
}

//----------------------Allgemeine Funktionen------------------------
void *vmm_Map(void *vAddress, paddr_t pAddress, size_t pages, uint8_t flags)
{
	uint8_t res;
	bool unused = pAddress == 0;
	bool allocate = flags & VMM_FLAGS_ALLOCATE;
	uint16_t avl = (unused && !allocate) ? VMM_UNUSED_PAGE : 0;

	if(pages == 0)
		return NULL;

	return LOCKED_RESULT(vmm_lock, {
		if(vAddress == NULL)
		{
			void *start = (void*)((flags & VMM_FLAGS_USER) ? USERSPACE_START : KERNELSPACE_START);
			void *end = (void*)((flags & VMM_FLAGS_USER) ? USERSPACE_END : KERNELSPACE_END);
			vAddress = getFreePages(start, end, pages);
		}

		bool error = false;
		size_t i;
		for(i = 0; i < pages; i++)
		{
			paddr_t paddr;
			uintptr_t currentOffset = i * VMM_SIZE_PER_PAGE;
			if(allocate)
			{
				paddr = pmm_Alloc();
				if(paddr == 1)
				{
					error = true;
					break;
				}
			}
			else
			{
				paddr = pAddress + currentOffset * !unused;
			}

			if((res = map(vAddress + currentOffset, paddr, flags, avl)) != 0)
			{
				error = true;
				break;
			}

			if(allocate)
				clearPage(vAddress + currentOffset);
		}

		if(error)
		{
			for(size_t j = 0; j < i; j++)
			{
				paddr_t paddr = unmap(vAddress + j * VMM_SIZE_PER_PAGE);
				if(allocate)
					pmm_Free(paddr);
			}
		}
		res == 0 ? vAddress : NULL;
	});
}

void vmm_UnMap(void *vAddress, size_t pages, bool freePages)
{
	LOCKED_TASK(vmm_lock, {
		for(size_t i = 0; i < pages; i++)
		{
			paddr_t pAddress = unmap(vAddress + i * VMM_SIZE_PER_PAGE);
			if(freePages)
				pmm_Free(pAddress);
		}
	});
}

/*
 * Ändert das Mapping einer Page. Falls die Page nicht vorhanden ist, wird sie gemappt
 * Params:			vAddress = Neue virt. Addresse der Page
 * 					pAddress = Neue phys. Addresse der Page
 * 					flags = neue Flags der Page
 * Rückgabewert:	0 = Alles in Ordnung
 * 					1 = zu wenig phys. Speicher vorhanden
 */
uint8_t vmm_ChangeMap(void *vAddress, paddr_t pAddress, uint8_t flags, uint16_t avl)
{
	//Einträge in die Page Tabellen
	const uint16_t PML4i = PML4_INDEX(vAddress);
	const uint16_t PDPi = PDP_INDEX(vAddress);
	const uint16_t PDi = PD_INDEX(vAddress);
	const uint16_t PTi = PT_INDEX(vAddress);

	PML4_t *const PML4 = PML4_ADDRESS();
	PDP_t *const PDP = PDP_ADDRESS(PML4i);
	PD_t *const PD = PD_ADDRESS(PML4i, PDPi);
	PT_t *const PT = PT_ADDRESS(PML4i, PDPi, PDi);

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
			if(map(vAddress, pAddress, flags, avl) == 1)
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
//FIXME: getPhysAddress funktioniert nur für kernel_context
//FIXME: correctly handle pages which are allocated but have not been mapped yet (VMM_UNUSED_PAGE)
uint8_t vmm_ReMap(context_t *src_context, void *src, context_t *dst_context, void *dst, size_t length, uint8_t flags, uint16_t avl)
{
	size_t i;
	for(i = 0; i < length; i++)
	{
		uint8_t r;
		if((r = vmm_ContextMap(dst_context, dst + i * VMM_SIZE_PER_PAGE, vmm_getPhysAddress(src + i * VMM_SIZE_PER_PAGE) ? : pmm_Alloc(), flags, avl)) != 0)
			return r;
		if(vmm_ContextUnMap(src_context, src + i * VMM_SIZE_PER_PAGE, false) == 2)
			return 1;
	}
	return 0;
}

/*
 * Mappt einen Speicherbereich an die vorgegebene Address im entsprechendem Kontext
 */
uint8_t vmm_ContextMap(context_t *context, void *vAddress, paddr_t pAddress, uint8_t flags, uint16_t avl)
{
	PML4_t *PML4 = context->virtualAddress;
	PDP_t *PDP;
	PD_t *PD;
	PT_t *PT;
	paddr_t Address;

	//Einträge in die Page Tabellen
	const uint16_t PML4i = PML4_INDEX(vAddress);
	const uint16_t PDPi = PDP_INDEX(vAddress);
	const uint16_t PDi = PD_INDEX(vAddress);
	const uint16_t PTi = PT_INDEX(vAddress);

	//Flags auslesen
	bool US = (flags & VMM_FLAGS_USER);
	bool G = (flags & VMM_FLAGS_GLOBAL);
	bool RW = (flags & VMM_FLAGS_WRITE);
	bool NX = (flags & VMM_FLAGS_NX);
	bool P = !(avl & VMM_UNUSED_PAGE);
	bool PCD = (flags & VMM_FLAGS_NO_CACHE);
	bool PWT = (flags & VMM_FLAGS_PWT);

	//PML4 Tabelle bearbeiten
	if((PML4->PML4E[PML4i] & PG_P) == 0)		//Eintrag für die PML4 schon vorhanden?
	{											//Erstelle neuen Eintrag
		if((Address = pmm_Alloc()) == 1)		//Speicherplatz für die PDP reservieren
		{
			return 1;							//Kein Speicherplatz vorhanden
		}

		//Eintrag in die PML4S
		setPML4Entry(PML4i, PML4, 1, 1, (PML4->PML4E[PML4i] & PG_US) || US, 1, 0, 0, 0, 0, Address);
		//PDP mappen
		PDP = vmm_Map(NULL, Address, 1, VMM_FLAGS_NX | VMM_FLAGS_WRITE);
		clearPage(PDP);
	}
	else
	{
		//PDP mappen
		PDP = vmm_Map(NULL, PML4->PML4E[PML4i] & PG_ADDRESS, 1, VMM_FLAGS_NX | VMM_FLAGS_WRITE);
	}

	//PDP Tabelle bearbeiten
	if((PDP->PDPE[PDPi] & PG_P) == 0)			//Eintrag in die PDP schon vorhanden?
	{											//Neuen Eintrag erstellen
		if((Address = pmm_Alloc()) == 1)		//Speicherplatz für die PD reservieren
		{
			unmap(PDP);
			return 1;							//Kein Speicherplatz vorhanden
		}

		//Eintrag in die PDP
		setPDPEntry(PDPi, PDP, 1, 1, (PDP->PDPE[PDPi] & PG_US) || US, 1, 0, 0, 0, 0, Address);
		//PD mappen
		PD = vmm_Map(NULL, Address, 1, VMM_FLAGS_NX | VMM_FLAGS_WRITE);
		clearPage(PD);
	}
	else
	{
		//PD mappen
		PD = vmm_Map(NULL, PDP->PDPE[PDPi] & PG_ADDRESS, 1, VMM_FLAGS_NX | VMM_FLAGS_WRITE);
	}

	//PD Tabelle bearbeiten
	if((PD->PDE[PDi] & PG_P) == 0)			//Eintrag in die PD schon vorhanden?
	{										//Neuen Eintrag erstellen
		if((Address = pmm_Alloc()) == 1)	//Speicherplatz für die PT reservieren
		{
			unmap(PDP);
			unmap(PD);
			return 1;							//Kein Speicherplatz vorhanden
		}

		//Eintrag in die PDP
		setPDEntry(PDi, PD, 1, 1, (PD->PDE[PDi] & PG_US) || US, 1, 0, 0, 0, 0, Address);
		//PT mappen
		PT = vmm_Map(NULL, Address, 1, VMM_FLAGS_NX | VMM_FLAGS_WRITE);
		clearPage(PT);
	}
	else
	{
		//PT mappen
		PT = vmm_Map(NULL, PD->PDE[PDi] & PG_ADDRESS, 1, VMM_FLAGS_NX | VMM_FLAGS_WRITE);
	}

	//PT Tabelle bearbeiten
	if((PT->PTE[PTi] & PG_P) == 0)			//Eintrag in die PT schon vorhanden?
	{										//Neuen Eintrag erstellen
		//Eintrag in die PT
		setPTEntry(PTi, PT, P, RW, US, PWT, PCD, 0, 0, G, avl, 0, NX, pAddress);
	}
	else
	{
		unmap(PDP);
		unmap(PD);
		unmap(PT);
		return 2;							//virtuelle Addresse schon besetzt
	}

	//Reserved-Bits zurücksetzen
	PD->PDE[PDi] &= ~0x1C0;
	PDP->PDPE[PDPi] &= ~0x1C0;
	PML4->PML4E[PML4i] &= ~0x1C0;

	//Tabellen wieder unmappen
	unmap(PDP);
	unmap(PD);
	unmap(PT);

	return 0;
}

/*
 * Gibt eine physikalischer Addresse zu einer virtuellen Addresse frei
 * Params:	context = Kontext in dem die Page demapped werden soll
 * 			vAddress = virt. Addresse der freizugebenden Speicherstelle
 *
 * Rückgabewert:	0 = Page wurde freigegeben
 * 					1 = virt. Addresse nicht belegt
 * 					2 = zu wenig phys. Speicherplatz vorhanden
 */
uint8_t vmm_ContextUnMap(context_t *context, void *vAddress, bool free_page)
{
	PML4_t *PML4 = context->virtualAddress;
	PDP_t *PDP;
	PD_t *PD;
	PT_t *PT;
	uint16_t i;

	//Einträge in die Page Tabellen
	const uint16_t PML4i = PML4_INDEX(vAddress);
	const uint16_t PDPi = PDP_INDEX(vAddress);
	const uint16_t PDi = PD_INDEX(vAddress);
	const uint16_t PTi = PT_INDEX(vAddress);

	InvalidateTLBEntry(vAddress);

	//PML4 Tabelle bearbeiten
	if((PML4->PML4E[PML4i] & PG_P) == 0)	//PML4 Eintrag vorhanden?
	{
		PML4->PML4E[PML4i] &= ~0x1C0;
		return 1;
	}

	//PDP mappen
	PDP = vmm_Map(NULL, PML4->PML4E[PML4i] & PG_ADDRESS, 1, VMM_FLAGS_NX | VMM_FLAGS_WRITE);

	//PDP Tabelle bearbeiten
	if((PDP->PDPE[PDPi] & PG_P) == 0)		//PDP Eintrag vorhanden?
	{
		PDP->PDPE[PDPi] &= ~0x1C0;
		PML4->PML4E[PML4i] &= ~0x1C0;
		unmap(PDP);
		return 1;
	}

	//PD mappen
	PD = vmm_Map(NULL, PDP->PDPE[PDPi] & PG_ADDRESS, 1, VMM_FLAGS_NX | VMM_FLAGS_WRITE);

	//PD Tabelle bearbeiten
	if((PD->PDE[PDi] & PG_P) == 0)			//PD Eintrag vorhanden?
	{
		PD->PDE[PDi] &= ~0x1C0;
		PDP->PDPE[PDPi] &= ~0x1C0;
		PML4->PML4E[PML4i] &= ~0x1C0;
		unmap(PD);
		unmap(PDP);
		return 1;
	}

	//PT mappen
	PT = vmm_Map(NULL, PD->PDE[PDi] & PG_ADDRESS, 1, VMM_FLAGS_NX | VMM_FLAGS_WRITE);

	//PT Tabelle bearbeiten
	if((PT->PTE[PTi] & PG_P) == 1)			//Wenn PT Eintrag vorhanden
	{										//dann lösche ihn
		if(free_page)
		{
			paddr_t page_addr = PT->PTE[PTi] & PG_ADDRESS;
			pmm_Free(page_addr);
		}
		clearPTEntry(PTi, PT);

		//Wird die PT noch benötigt?
		for(i = 0; i < PAGE_ENTRIES; i++)
		{
			if((PT->PTE[i] & PG_P) || (PG_AVL(PT->PTE[i]) & VMM_UNUSED_PAGE))
			{
				PT->PTE[PTi] &= ~0x1C0;
				PD->PDE[PDi] &= ~0x1C0;
				PDP->PDPE[PDPi] &= ~0x1C0;
				PML4->PML4E[PML4i] &= ~0x1C0;
				unmap(PT);
				unmap(PD);
				unmap(PDP);
				return 0; //Wird die PT noch benötigt, sind wir fertig
			}
		}
		//Ansonsten geben wir den Speicherplatz für die PT frei
		vmm_SysFree(PT, 1);
		//und löschen den Eintrag für diese PT in der PD

		clearPDEntry(PDi, PD);

		//Wird die PD noch benötigt?
		for(i = 0; i < PAGE_ENTRIES; i++)
		{
			if(PD->PDE[i] & PG_P)
			{
				PD->PDE[PDi] &= ~0x1C0;
				PDP->PDPE[PDPi] &= ~0x1C0;
				PML4->PML4E[PML4i] &= ~0x1C0;
				unmap(PD);
				unmap(PDP);
				return 0; //Wid die PD noch benötigt, sind wir fertig
			}
		}
		//Ansonsten geben wir den Speicherplatz für die PD frei
		vmm_SysFree(PD, 1);
		//und löschen den Eintrag für diese PD in der PDP

		clearPDPEntry(PDPi, PDP);

		//Wird die PDP noch benötigt?
		for(i = 0; i < PAGE_ENTRIES; i++)
		{
			//Wird die PDP noch benötigt, sind wir fertig
			if(PDP->PDPE[i] & PG_P)
			{
				PDP->PDPE[PDPi] &= ~0x1C0;
				PML4->PML4E[PML4i] &= ~0x1C0;
				unmap(PDP);
				return 0;
			}
		}
		//Ansonsten geben wir den Speicherplatz für die PDP frei
		vmm_SysFree(PDP, 1);
		//und löschen den Eintrag für diese PDP in der PML4

		clearPML4Entry(PML4i, PML4);	//PML4 ist immer vorhanden, daher keine Überprüfung
										//ob sie leer ist (was sehr schlecht sein würde --> PF)
		//Reserved-Bits zurücksetzen
		PML4->PML4E[PML4i] &= ~0x1C0;
		return 0;
	}
	else
	{
		PT->PTE[PTi] &= ~0x1C0;
		PD->PDE[PDi] &= ~0x1C0;
		PDP->PDPE[PDPi] &= ~0x1C0;
		PML4->PML4E[PML4i] &= ~0x1C0;
		unmap(PT);
		unmap(PD);
		unmap(PDP);
		return 1;
	}
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
bool vmm_getPageStatus(void *Address)
{
	//Einträge in die Page Tabellen
	const uint16_t PML4i = PML4_INDEX(Address);
	const uint16_t PDPi = PDP_INDEX(Address);
	const uint16_t PDi = PD_INDEX(Address);
	const uint16_t PTi = PT_INDEX(Address);

	const PML4_t *const PML4 = PML4_ADDRESS();
	const PDP_t *const PDP = PDP_ADDRESS(PML4i);
	const PD_t *const PD = PD_ADDRESS(PML4i, PDPi);
	const PT_t *const PT = PT_ADDRESS(PML4i, PDPi, PDi);

	return isPageFree(&PML4->PML4E[PML4i], &PDP->PDPE[PDPi], &PD->PDE[PDi], &PT->PTE[PTi]);
}

paddr_t vmm_getPhysAddress(void *virtualAddress)
{
	//Einträge in die Page Tabellen
	const uint16_t PML4i = PML4_INDEX(virtualAddress);
	const uint16_t PDPi = PDP_INDEX(virtualAddress);
	const uint16_t PDi = PD_INDEX(virtualAddress);
	const uint16_t PTi = PT_INDEX(virtualAddress);

	const PML4_t *const PML4 = PML4_ADDRESS();
	const PDP_t *const PDP = PDP_ADDRESS(PML4i);
	const PD_t *const PD = PD_ADDRESS(PML4i, PDPi);
	const PT_t *const PT = PT_ADDRESS(PML4i, PDPi, PDi);

	return isPageFree(&PML4->PML4E[PML4i], &PDP->PDPE[PDPi], &PD->PDE[PDi], &PT->PTE[PTi]) ? 0 : (paddr_t)(PT->PTE[PTi] & PG_ADDRESS);
}

void vmm_unusePages(void *virt, size_t pages)
{
	for(void *address = virt; address < virt + pages * VMM_SIZE_PER_PAGE; address += VMM_SIZE_PER_PAGE)
	{
		//Einträge in die Page Tabellen
		const uint16_t PML4i = PML4_INDEX(address);
		const uint16_t PDPi = PDP_INDEX(address);
		const uint16_t PDi = PD_INDEX(address);
		const uint16_t PTi = PT_INDEX(address);

		const PML4_t *const PML4 = PML4_ADDRESS();
		const PDP_t *const PDP = PDP_ADDRESS(PML4i);
		const PD_t *const PD = PD_ADDRESS(PML4i, PDPi);
		PT_t *const PT = PT_ADDRESS(PML4i, PDPi, PDi);

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

void vmm_usePages(void *virt, size_t pages)
{
	void *address = (void*)((uintptr_t)virt & ~0xFFF);

	for(; address < (void*)((uintptr_t)virt & ~0xFFF) + pages * VMM_SIZE_PER_PAGE; address += VMM_SIZE_PER_PAGE)
	{
		//Einträge in die Page Tabellen
		const uint16_t PML4i = PML4_INDEX(address);
		const uint16_t PDPi = PDP_INDEX(address);
		const uint16_t PDi = PD_INDEX(address);
		const uint16_t PTi = PT_INDEX(address);

		PT_t *const PT = PT_ADDRESS(PML4i, PDPi, PDi);

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
	PML4_t *newPML4 = memset(vmm_SysAlloc(1), 0, MM_BLOCK_SIZE);

	//Kernel in den Adressraum einbinden
	PML4_t *const PML4 = PML4_ADDRESS();

	for(uint16_t PML4i = 0; PML4i < PML4e; PML4i++)
		newPML4->PML4E[PML4i] = PML4->PML4E[PML4i];

	context->physAddress = vmm_getPhysAddress(newPML4);
	//Den letzten Eintrag verwenden wir als Zeiger auf den Anfang der Tabelle. Das ermöglicht das Editieren derselben.
	setPML4Entry(511, newPML4, 1, 1, 0, 1, 0, 0, 0, 1, (uintptr_t)context->physAddress);

	context->virtualAddress = newPML4;

	return context;
}

/*
 * Löscht einen virtuellen Adressraum
 */
void deleteContext(context_t *context)
{
	//Erst alle Pages des Kontextes freigeben
	PML4_t *PML4 = context->virtualAddress;
	for(uint16_t PML4i = PML4e; PML4i < PAGE_ENTRIES - 1; PML4i++)
	{
		//Ist der Eintrag gültig
		if(PML4->PML4E[PML4i] & PG_P)
		{
			//PDP mappen
			PDP_t *PDP = vmm_Map(NULL, PML4->PML4E[PML4i] & PG_ADDRESS, 1, VMM_FLAGS_NX);
			for(uint16_t PDPi = 0; PDPi < PAGE_ENTRIES; PDPi++)
			{
				//Ist der Eintrag gültig
				if(PDP->PDPE[PDPi] & PG_P)
				{
					//PD mappen
					PD_t *PD = vmm_Map(NULL, PDP->PDPE[PDPi] & PG_ADDRESS, 1, VMM_FLAGS_NX);
					for(uint16_t PDi = 0; PDi < PAGE_ENTRIES; PDi++)
					{
						//Ist der Eintrag gültig
						if(PD->PDE[PDi] & PG_P)
						{
							//PT mappen
							PT_t *PT = vmm_Map(NULL, PD->PDE[PDi] & PG_ADDRESS, 1, VMM_FLAGS_NX);
							for(uint16_t PTi = 0; PTi < PAGE_ENTRIES; PTi++)
							{
								//Ist die Page alloziiert
								if(PT->PTE[PTi] & PG_P)
									pmm_Free(PT->PTE[PTi] & PG_ADDRESS);
							}
							//PT löschen
							vmm_SysFree(PT, 1);
						}
					}
					//PD löschen
					vmm_SysFree(PD, 1);
				}
			}
			//PDP löschen
			vmm_SysFree(PDP, 1);
		}
	}

	//Restliche Datenstrukturen freigeben
	vmm_SysFree(context->virtualAddress, 1);
	free(context);
}

/*
 * Aktiviert einen virtuellen Adressraum
 */
inline void activateContext(context_t *context)
{
	asm volatile("mov %0,%%cr3" : : "r"(context->physAddress));
}

bool vmm_handlePageFault(void *page, uint64_t errorcode)
{
	uint16_t PML4i = PML4_INDEX(page);
	uint16_t PDPi = PDP_INDEX(page);
	uint16_t PDi = PD_INDEX(page);
	uint16_t PTi = PT_INDEX(page);

	PML4_t *const PML4 = PML4_ADDRESS();
	PDP_t *const PDP = PDP_ADDRESS(PML4i);
	PD_t *const PD = PD_ADDRESS(PML4i, PDPi);
	PT_t *const PT = PT_ADDRESS(PML4i, PDPi, PDi);

	//Activate unused pages
	if(!isPageFree(&PML4->PML4E[PML4i], &PDP->PDPE[PDPi], &PD->PDE[PDi], &PT->PTE[PTi]) && (PG_AVL(PT->PTE[PTi]) & VMM_UNUSED_PAGE))
	{
		vmm_usePages(page, 1);
		return true;
	}
	return false;
}
