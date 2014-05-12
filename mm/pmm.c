/*
 * pmm.c
 *
 *  Created on: 09.07.2012
 *      Author: pascal
 */

#include "pmm.h"
#include "config.h"
#include "vmm.h"
#include "memory.h"
#include "multiboot.h"
#include "display.h"
#include "mm.h"
#ifdef DEBUGMODE
#include "stdio.h"
#endif

//Anfang und Ende des Kernels
extern uint8_t kernel_start;
extern uint8_t kernel_end;

static uintptr_t pmm_FirstStack[512] __attribute__((aligned(MM_BLOCK_SIZE)));

uint64_t pmm_Speicher;					//Maximal verfügbarer RAM (physisch)
uint64_t pmm_Speicher_Verfuegbar = 0;	//Verfügbarer (freier) physischer Speicher (4kb)
static uint64_t pmm_Kernelsize;			//Grösse des Kernels in Bytes
static uintptr_t *pmm_Stack;			//"Stack" mit freien phys. Speicherstellenadressen
static uint16_t pmm_Stack_Index;		//Index zum obigen Stack

static char *Map;						//Zeiger auf die Bitmap (Bit = 1 => Speicherseite frei)

/*
 * Initialisiert die physikalische Speicherverwaltung
 */
bool pmm_Init()
{
	mmap *map;
	uint32_t mapLength;
	uintptr_t i;

	pmm_Kernelsize = &kernel_end - &kernel_start;
	map = (mmap*)(uintptr_t)MBS->mbs_mmap_addr;
	mapLength = MBS->mbs_mmap_length;
	Map = NULL;

	//"Nachschauen", wieviel Speicher vorhanden ist
	uint32_t maxLength = mapLength / sizeof(mmap);
	pmm_Speicher = 0;
	for(i = 0; i < maxLength; i++)
		pmm_Speicher += map[i].length;

	//Stack auf den ersten Stackframe legen
	pmm_Stack = pmm_FirstStack;
	pmm_Stack_Index = 1;
	pmm_Stack[0] = 0;		//Erster Eintrag des Stacks zeigt auf den vorherigen Speicherblock
	pmm_Stack[511] = 0;		//Zeiger null gibt das Ende des Stacks an

	//Jetzt muss zuerst die virt. Speicherverwaltung initialisiert werden
	if(!vmm_Init(pmm_Speicher, (uintptr_t)pmm_Stack)) return false;

	//Map analysieren und entsprechende Einträge in die Speicherverwaltung machen
	do
	{
		if(map->type == 1)
			for(i = map->base_addr; i < map->base_addr + map->length; i += MM_BLOCK_SIZE)
				if(i != (uintptr_t)(pmm_FirstStack) ||
						i < vmm_getPhysAddress(&kernel_start) || i > vmm_getPhysAddress(&kernel_end))
					pmm_Free(i);
		map = (mmap*)((uintptr_t)map + map->size + 4);
	}
	while(map < (mmap*)(uintptr_t)(MBS->mbs_mmap_addr + mapLength));

	//TODO: ohne "Hack" auskommen, um Speicher zu bestimmen
	pmm_Speicher = pmm_Stack[pmm_Stack_Index - 1];

	#ifdef DEBUGMODE
	printf("    %u MB Speicher gefunden\n    %u GB Speicher gefunden\n", pmm_Speicher >> 20, pmm_Speicher >> 30);
	#endif

	//Bit-Map anlegen und die Bits für reservierte Speicherbereiche setzen
	size_t size = pmm_Speicher / (8 * MM_BLOCK_SIZE);
	if(size % (8 * MM_BLOCK_SIZE) != 0) size++;
	Map = calloc(size, 1);

	uintptr_t *pmm_Stack_tmp = pmm_Stack;
	uint16_t pmm_Stack_Index_tmp = pmm_Stack_Index;
	while(true)	//Stack von hinten nach vorne durchgehen und für jede Adresse das entsprechende Bit setzen
	{
		if(pmm_Stack_Index_tmp - 1 == 0)
		{
			if(pmm_Stack_tmp[0] == 0) break;
			pmm_Stack_tmp = (uintptr_t*)pmm_Stack_tmp[0];
			pmm_Stack_Index_tmp = 510;
		}
		uintptr_t Address = pmm_Stack_tmp[--pmm_Stack_Index_tmp];
		Map[Address / (8 * MM_BLOCK_SIZE)] |= (1 << ((Address / MM_BLOCK_SIZE) % 8));
	}

	SysLog("PMM", "Initialisierung abgeschlossen");
	return true;
}

/*
 * Reserviert eine Speicherstelle
 * Rückgabewert:	phys. Addresse der Speicherstelle
 * 					1 = Kein phys. Speicherplatz mehr vorhanden
 */
void *pmm_Alloc()
{
	bool found = false;
	void *Address;
	do
	{
		if(pmm_Stack_Index - 1 == 0)
		{
			if(pmm_Stack[0] == 0) return 1;				//Eintrag null zeigt das Ende an
			pmm_Stack = (uintptr_t*)pmm_Stack[0];
			pmm_Stack_Index = 510;						//Stack-Index auf letzten Eintrag stellen
		}
		else if(pmm_Stack_Index + 2 == 511)
		{
			mm_SysFree(pmm_Stack[511], PMM_STACK_LENGTH_PER_BLOCK);	//Alten Speicherblock freigeben
			pmm_Stack[511] = 0;										//Addresse löschen
		}
		Address = pmm_Stack[--pmm_Stack_Index];
		pmm_Speicher_Verfuegbar--;

		//In der Bitmap nachschauen, ob diese Adresse schon reserviert wurde
		if(Map != NULL)
		{
			uint64_t Index = ((uintptr_t)Address) / (8 * MM_BLOCK_SIZE);
			if(((Map[Index] >> ((((uintptr_t)Address) / MM_BLOCK_SIZE) % 8)) & 0x1) == 1)	//Speicherseite frei?
				found = true;																//Dann ist die Suche beendet.
		}
		else
			found = true;
	}
	while(!found);

	//In der Bitmap eintragen, dass dieser Speicherbereich nicht mehr frei ist (Bit löschen)
	if(Map != NULL)
	{
		uint64_t Index = ((uintptr_t)Address) / (8 * MM_BLOCK_SIZE);
		Map[Index] &= ~(1 << ((((uintptr_t)Address) / MM_BLOCK_SIZE) % 8));
	}

	return Address;
}

/*
 * Gibt eine Speicherstelle frei, dabei wird wenn möglich in der Bitmap kontrolliert, ob diese schon mal freigegeben wurde
 * Params: phys. Addresse der Speicherstelle
 * Rückgabewert:	true = Aktion erfolgreich
 * 					false = Aktion fehlgeschlagen (zu wenig Speicher)
 */
void pmm_Free(void *Address)
{
	//Zuerst in der Bitmap nachschauen, ob diese Adresse schon freigegeben wurde (Bit gesetzt)
	if(Map != NULL)
	{
		uint64_t Index = ((uintptr_t)Address) / (8 * MM_BLOCK_SIZE);
		if(((Map[Index] >> ((((uintptr_t)Address) / MM_BLOCK_SIZE) % 8)) & 0x1) == 1) return;
		//Wenn der Speicherbereich noch nicht freigegeben wurde, ihn als freigegeben markieren
		Map[Index] |= (1 << ((((uintptr_t)Address) / MM_BLOCK_SIZE) % 8));
	}

	pmm_Stack[pmm_Stack_Index++] = (uintptr_t)Address;
	pmm_Speicher_Verfuegbar++;
	if(pmm_Stack_Index >= 511)
	{
		if(pmm_Stack[511] == 0)	//Wenn keine Adresse eingetragen ist muss ein neuer
		{						//Speicherblock angefordert werden
			uintptr_t newAddress = mm_SysAlloc(PMM_STACK_LENGTH_PER_BLOCK);
			pmm_Stack[511] = newAddress;//Letzter Eintrag zeigt auf den nächsten Speicherblock
		}
		else
		{
			uintptr_t oldAddress = (uintptr_t)pmm_Stack;
			pmm_Stack = (uintptr_t*)pmm_Stack[511];
			pmm_Stack[511] = 0;
			pmm_Stack[0] = oldAddress;
			pmm_Stack_Index = 1;
		}
	}
}

//Für DMA erforderlich
void *pmm_AllocDMA(void *maxAddress, size_t Size)
{
	if(Map == NULL)
		return NULL;
	//Durchsuche die Bitmap um Speicherseiten zu finden, die hintereinander liegen
	size_t Bits = Size / 8 + Size % 8;
	size_t startBit;
	uintptr_t i;
	bool found = false;
	for(i = 0; i < maxAddress; i += MM_BLOCK_SIZE)
	{
		uint64_t Index = i / (8 * MM_BLOCK_SIZE);
		startBit = (i / MM_BLOCK_SIZE) % 8;
		if(((Map[Index] >> ((i / MM_BLOCK_SIZE) % 8)) & 0x1) == 1)	//Speicherseite frei, dann nachschauen, ob  nachfolgende auch frei sind
		{
			uint64_t j;
			for(j = 0; j < Bits; j++)
			{
				if((Map[Index + Bits / 8] >> (startBit + j)) & 0x1 == 0)	//Speicherseite nicht frei => weitersuchen
				{
					found = false;
					break;
				}
				else
					found = true;
			}
			if(found)	//Speicher reservieren
			{
				for(j = 0; j < Bits; j++)
				Map[Index + Bits / 8] &= ~(1 << (startBit + j));
				return (void*)i;
			}
		}
	}
	return NULL;
}
