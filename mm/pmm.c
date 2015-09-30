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
#include "stdlib.h"
#include "lock.h"
#ifdef DEBUGMODE
#include "stdio.h"
#endif

#define PMM_BITS_PER_ELEMENT	(sizeof(*Map) * 8)
#define PMM_MAP_ALIGN_SIZE(x)	((x + (sizeof(*Map) - 1)) & ~(sizeof(*Map) - 1))

#define MAX(a, b)				((a > b) ? a : b)
#define MIN(a, b)				((a < b) ? a : b)

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
static size_t lastIndex = 4;

static lock_t pmm_lock = LOCK_UNLOCKED;

/*
 * Initialisiert die physikalische Speicherverwaltung
 */
bool pmm_Init()
{
	mmap *map;
	uint32_t mapLength;
	paddr_t i, maxAddress;

	pmm_Kernelsize = &kernel_end - &kernel_start;
	map = (mmap*)(uintptr_t)MBS->mbs_mmap_addr;
	mapLength = MBS->mbs_mmap_length;

	//"Nachschauen", wieviel Speicher vorhanden ist
	uint32_t maxLength = mapLength / sizeof(mmap);
	pmm_Speicher = 0;
	for(i = 0; i < maxLength; i++)
	{
		pmm_Speicher += map[i].length;
		maxAddress = map[i].base_addr + map[i].length;
	}

	//Stack auf den ersten Stackframe legen

	//Jetzt muss zuerst die virt. Speicherverwaltung initialisiert werden
	if(!vmm_Init(pmm_Speicher)) return false;

	//Die ersten 1GB eintragen
	//Map analysieren und entsprechende Einträge in die Speicherverwaltung machen
	do
	{
		if(map->type == 1)
			for(i = map->base_addr; i < MM_BLOCK_SIZE * mapLength * PMM_BITS_PER_ELEMENT && i < map->base_addr + map->length; i += MM_BLOCK_SIZE)
				if(i < vmm_getPhysAddress(&kernel_start) || i > vmm_getPhysAddress(&kernel_end))
				{
					pmm_Free(i);
				}
		if(i < MM_BLOCK_SIZE * mapLength * PMM_BITS_PER_ELEMENT)
			map = (mmap*)((uintptr_t)map + map->size + 4);
	}
	while(i < MM_BLOCK_SIZE * mapLength * PMM_BITS_PER_ELEMENT && map < (mmap*)(uintptr_t)(MBS->mbs_mmap_addr + mapLength));

	if(i >= MM_BLOCK_SIZE * mapLength * PMM_BITS_PER_ELEMENT)
	{
		//neuen Speicher für die Bitmap anfordern und zwar so viel wie nötig
		Map = memcpy(calloc(PMM_MAP_ALIGN_SIZE(maxAddress / MM_BLOCK_SIZE / 8), 1), Map, mapSize * sizeof(*Map));
		mapSize = PMM_MAP_ALIGN_SIZE(maxAddress / MM_BLOCK_SIZE / 8) / sizeof(*Map);
		//Weiter Speicher freigeben
		while(map < (mmap*)(uintptr_t)(MBS->mbs_mmap_addr + mapLength))
		{
			if(map->type == 1)
			{
				if(i < map->base_addr || i > map->base_addr + map->length)
					i = map->base_addr;
				for(; i < map->base_addr + map->length; i += MM_BLOCK_SIZE)
					if(i < vmm_getPhysAddress(&kernel_start) || i > vmm_getPhysAddress(&kernel_end))
					{
						pmm_Free(i);
					}
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
	while((Address = (uintptr_t)list_pop(reservedPages)))
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
paddr_t pmm_Alloc()
{
	size_t i;
	if(pmm_Speicher_Verfuegbar == 0)
	{
		return 1;
	}
	lock(&pmm_lock);
	for(i = lastIndex; i < mapSize; i++)
	{
		uint8_t j = __builtin_ffsl(Map[i]);
		if(j > 0)
		{
			//j - 1 rechnen, da ffsl eins drauf addiert
			j--;
			paddr_t Address = ((i * PMM_BITS_PER_ELEMENT + j) * MM_BLOCK_SIZE);

			//In der Bitmap eintragen, dass Page reserviert
			asm volatile("btr %0,%1": :"r"(j & 0xFF) ,"m"(Map[i]));
			lastIndex = i;
			unlock(&pmm_lock);
			locked_dec(&pmm_Speicher_Verfuegbar);
			return Address;
		}
	}

	unlock(&pmm_lock);
	return 1;
}

/*
 * Gibt eine Speicherstelle frei, dabei wird wenn möglich in der Bitmap kontrolliert, ob diese schon mal freigegeben wurde
 * Params: phys. Addresse der Speicherstelle
 * Rückgabewert:	true = Aktion erfolgreich
 * 					false = Aktion fehlgeschlagen (zu wenig Speicher)
 */
void pmm_Free(paddr_t Address)
{
	uint8_t bit_status;
	//entsprechendes Bit in der Bitmap zurücksetzen
	size_t i = Address / MM_BLOCK_SIZE / PMM_BITS_PER_ELEMENT;
	uint8_t bit = (Address / MM_BLOCK_SIZE) % PMM_BITS_PER_ELEMENT;
	lock(&pmm_lock);
	asm volatile("bts %1,%2;"
				"setc %0": "=r"(bit_status): "r"(bit & 0xFF), "m"(Map[i]));
	lastIndex = MAX(MIN(i, lastIndex), 4);
	unlock(&pmm_lock);
	if(bit_status == 0)
		locked_inc(&pmm_Speicher_Verfuegbar);
	else
		printf("\e[33;mWarning:\e[0m Freed page which was already freed (0x%X)\n", Address);
}

//Für DMA erforderlich
paddr_t pmm_AllocDMA(paddr_t maxAddress, size_t Size)
{
	lock(&pmm_lock);
	if(pmm_Speicher_Verfuegbar == 0)
	{
		unlock(&pmm_lock);
		return 0;
	}
	//Durchsuche die Bitmap um Speicherseiten zu finden, die hintereinander liegen
	size_t Bits = Size / PMM_BITS_PER_ELEMENT + Size % PMM_BITS_PER_ELEMENT;
	size_t startBit;
	paddr_t i;
	bool found = false;
	//NULL ist eine ungültige Adresse deshalb fangen wir bei der 2. Page an zu suchen
	for(i = MM_BLOCK_SIZE; i < maxAddress; i += MM_BLOCK_SIZE)
	{
		uint64_t Index = i / (PMM_BITS_PER_ELEMENT * MM_BLOCK_SIZE);
		startBit = (i / MM_BLOCK_SIZE) % PMM_BITS_PER_ELEMENT;
		if(((Map[Index] >> ((i / MM_BLOCK_SIZE) % PMM_BITS_PER_ELEMENT)) & 0x1) == 1)	//Speicherseite frei, dann nachschauen, ob  nachfolgende auch frei sind
		{
			uint64_t j;
			for(j = 0; j < Bits; j++)
			{
				if(((Map[Index + Bits / PMM_BITS_PER_ELEMENT] >> (startBit + j)) & 0x1) == 0)	//Speicherseite nicht frei => weitersuchen
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
				unlock(&pmm_lock);
				return i;
			}
		}
	}
	unlock(&pmm_lock);
	return 0;
}
