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
#include "string.h"
#ifdef DEBUGMODE
#include "stdio.h"
#endif

#define PMM_BITS_PER_ELEMENT	(sizeof(*Map) * 8)

//Anfang und Ende des Kernels
extern uint8_t kernel_start;
extern uint8_t kernel_end;

uint64_t pmm_Speicher;					//Maximal verfügbarer RAM (physisch)
uint64_t pmm_Speicher_Verfuegbar = 0;	//Verfügbarer (freier) physischer Speicher (4kb)
static uint64_t pmm_Kernelsize;			//Grösse des Kernels in Bytes

//32768 byte grosse Bitmap (für die ersten 1GB Speicher)
//Ein Bit ist gesetzt, wenn die Page frei ist
static uint64_t tmpMap[4096] __attribute__((aligned(MM_BLOCK_SIZE)));
static uint64_t *Map = tmpMap;
static size_t mapSize = 4096;			//Grösse der Bitmap

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

	//"Nachschauen", wieviel Speicher vorhanden ist
	uint32_t maxLength = mapLength / sizeof(mmap);
	pmm_Speicher = 0;
	for(i = 0; i < maxLength; i++)
		pmm_Speicher += map[i].length;

	//Stack auf den ersten Stackframe legen

	//Jetzt muss zuerst die virt. Speicherverwaltung initialisiert werden
	if(!vmm_Init(pmm_Speicher)) return false;

	//Die ersten 1GB eintragen
	//Map analysieren und entsprechende Einträge in die Speicherverwaltung machen
	size_t pages = 1 * 1024 * 1024 * 1024 / 4096;	//Anzahl der Pages für den Anfang
	do
	{
		if(map->type == 1)
			for(i = map->base_addr;pages > 0 && i < map->base_addr + map->length; i += MM_BLOCK_SIZE)
				if(i < vmm_getPhysAddress(&kernel_start) || i > vmm_getPhysAddress(&kernel_end))
				{
					pmm_Free(i);
					pages--;
				}
		map = (mmap*)((uintptr_t)map + map->size + 4);
	}
	while(pages > 0 && map < (mmap*)(uintptr_t)(MBS->mbs_mmap_addr + mapLength));

	if(pages == 0)
	{
		//neuen Speicher für die Bitmap anfordern und zwar so viel wie nötig
		Map = memcpy(malloc(pmm_Speicher / MM_BLOCK_SIZE / 8 * sizeof(*Map)), Map, mapSize * sizeof(*Map));
		mapSize = pmm_Speicher / MM_BLOCK_SIZE / sizeof(*Map);
		//Weiter Speicher freigeben
		while(map < (mmap*)(uintptr_t)(MBS->mbs_mmap_addr + mapLength))
		{
			if(map->type == 1)
				for(i = map->base_addr; i < map->base_addr + map->length; i += MM_BLOCK_SIZE)
					if(i < vmm_getPhysAddress(&kernel_start) || i > vmm_getPhysAddress(&kernel_end))
					{
						pmm_Free(i);
					}
			map = (mmap*)((uintptr_t)map + map->size + 4);
		}
	}

	#ifdef DEBUGMODE
	printf("    %u MB Speicher gefunden\n    %u GB Speicher gefunden\n", pmm_Speicher >> 20, pmm_Speicher >> 30);
	#endif

	//Liste mit reservierten Pages
	extern context_t kernel_context;
	list_t reservedPages = vmm_getTables(&kernel_context);
	//Liste durchgehen und Bits in der BitMap löschen
	uintptr_t Address;
	while((Address = list_pop(reservedPages)))
	{
		Map[Address / (PMM_BITS_PER_ELEMENT * MM_BLOCK_SIZE)] &= ~(1ULL << ((((uintptr_t)Address) / MM_BLOCK_SIZE) % PMM_BITS_PER_ELEMENT));
	}
	list_destroy(reservedPages);

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
	void *Address = 1;
	size_t i;
	if(pmm_Speicher_Verfuegbar == 0)
		return 1;
	for(i = 4; i < mapSize; i++)
	{
		if(Map[i] & (-1ULL))
		{
			uint8_t j;
			//Jedes Bit prüfen, ob Speicherseite frei
			for(j = 0; j < PMM_BITS_PER_ELEMENT; j++)
			{
				if(Map[i] & (1ULL << j))
				{
					Address = (i * PMM_BITS_PER_ELEMENT + j) * MM_BLOCK_SIZE;

					//In der Bitmap eintragen, dass Page reserviert
					Map[i] &= ~(1ULL << j);

					break;
				}
			}
			pmm_Speicher_Verfuegbar--;
			break;
		}
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
	//entsprechendes Bit in der Bitmap zurücksetzen
	size_t i = (uintptr_t)Address / MM_BLOCK_SIZE / PMM_BITS_PER_ELEMENT;
	uint8_t bit = ((uintptr_t)Address / MM_BLOCK_SIZE) % PMM_BITS_PER_ELEMENT;
	Map[i] |= (1ULL << bit);
	pmm_Speicher_Verfuegbar++;
}

//Für DMA erforderlich
void *pmm_AllocDMA(void *maxAddress, size_t Size)
{
	if(pmm_Speicher_Verfuegbar == 0)
		return 1;
	//Durchsuche die Bitmap um Speicherseiten zu finden, die hintereinander liegen
	size_t Bits = Size / PMM_BITS_PER_ELEMENT + Size % PMM_BITS_PER_ELEMENT;
	size_t startBit;
	uintptr_t i;
	bool found = false;
	for(i = 0; i < maxAddress; i += MM_BLOCK_SIZE)
	{
		uint64_t Index = i / (PMM_BITS_PER_ELEMENT * MM_BLOCK_SIZE);
		startBit = (i / MM_BLOCK_SIZE) % PMM_BITS_PER_ELEMENT;
		if(((Map[Index] >> ((i / MM_BLOCK_SIZE) % PMM_BITS_PER_ELEMENT)) & 0x1) == 1)	//Speicherseite frei, dann nachschauen, ob  nachfolgende auch frei sind
		{
			uint64_t j;
			for(j = 0; j < Bits; j++)
			{
				if((Map[Index + Bits / PMM_BITS_PER_ELEMENT] >> (startBit + j)) & 0x1 == 0)	//Speicherseite nicht frei => weitersuchen
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
				Map[Index + Bits / PMM_BITS_PER_ELEMENT] &= ~(1 << (startBit + j));
				return (void*)i;
			}
		}
	}
	return 1;
}
