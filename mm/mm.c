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
	return vmm_Init() && pmm_Init();
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
	if(Address < USERSPACE_START || Address > USERSPACE_END)	//Kontrolle ob richtiger Adress-
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
	uintptr_t Address;
	Address = vmm_SysAlloc(Size);
	if(Address == 1) Panic("MM", "Nicht genuegend physikalischer Speicher vorhanden");
	if(Address == 2) Panic("MM", "Virtuelle Adresse ist schon belegt");
	if(Address == 3) Panic("MM", "Nich genuegend virtueller Speicher vorhanden");
	return Address;
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
