/*
 * mm.c
 *
 *  Created on: 02.07.2012
 *      Author: pascal
 */

#include "mm.h"
#include "multiboot.h"
#include "display.h"
#include "vmm.h"
#include "pmm.h"
#include "memory.h"

//Speicherverwaltung
bool mm_Init()
{
	if(!pmm_Init()) return false;
	return true;
}

/*
 * Reserviert Speicher für Pages Pages.
 * Parameter:	Pages = Anzahl Pages, die reserviert werden sollen
 */
uintptr_t mm_Alloc(uint64_t Pages)
{
	return vmm_Alloc(Pages);
}

void mm_Free(uintptr_t Address, uint64_t Pages)
{
	if(Address > USERSPACE_START || Address < MAX_ADDRESS)	//Kontrolle ob richtiger Adress-
		Panic("MM", "Ungueltiger Adressbereich");			//bereich
	vmm_Free(Address, Pages);
}

//System
/*
 * Reserviert Speicher für das System
 * Parameter:		Size = Grösse des Speicherbereichs, der reserviert werden soll in Bytes
 * Rückgabewert:	Adresse, an die der Speichblock reserviert wurde
 */
uintptr_t mm_SysAlloc(uint64_t Size)
{
	uint64_t Pages;
	uintptr_t Address;
	Pages = Size / MM_BLOCK_SIZE;
	if(Size % MM_BLOCK_SIZE > 0) Pages++;	//Aufrunden
	Address = vmm_SysAlloc(0, Pages, true);
	if(Address == 1) Panic("MM", "Nicht genuegend physikalischer Speicher vorhanden");
	if(Address == 2) Panic("MM", "Virtuelle Adresse ist schon belegt");
	if(Address == 3) Panic("MM", "Nich genuegend virtueller Speicher vorhanden");
	return Address;
}

/*
 * Reserviert Speicher für das System an der angegeben Addresse
 * Parameter:	Address = virt. Addresse des Speicherbereichs
 * 				Size = Grösse des Speicherbereichs
 *
 * Rückgabewert:	true = Speicherbereich erfolgreich reserviert
 * 					false = Fehler beim reservieren des Speicherbereichs
 */
bool mm_SysAllocAddr(uintptr_t Address, uint64_t Size)
{
	uint64_t Pages;
	uint8_t Fehler;
	Pages = Size / MM_BLOCK_SIZE;
	if(Size % MM_BLOCK_SIZE > 0) Pages++;	//Aufrunden
	Fehler = vmm_SysAlloc(Address, Pages, false);
	if(Fehler == 1) Panic("MM", "Nich genügend physikalischer Speicher vorhanden");
	if(Fehler == 2 || Fehler == 3) return false;
	return true;
}

/*
 * Gibt die angegebene Speicherstelle frei
 * Parameter:	Address = virt. Adresse des Speicherbereichs
 * 				Size = Grösse des Speicherbereichs
 *
 * Rückgabewert:	true = Speicherbereich erfolgreich freigegeben
 * 					false = Fehler beim Freigeben des Speicherbereichs
 */
bool mm_SysFree(uintptr_t Address, uint64_t Size)
{
	if(Address > KERNELSPACE_END) return false;
	vmm_SysFree(Address, Size);
	return true;
}
