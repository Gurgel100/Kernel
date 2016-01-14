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
	while(pmm_Speicher_Verfuegbar > 0)
	{
		size_t i;
		for(i = 4; i < mapSize; i++)
		{
			uint16_t tmp;
			while((tmp = __builtin_ffsl(Map[i])) > 0)
			{
				uint16_t j;
				bool status;
				j = tmp - 1;
				asm volatile("lock btr %2,%0;"
							"setc %1": "=m"(Map[i]), "=r"(status): "r"(j): "cc");
				if(status)
				{
					locked_dec(&pmm_Speicher_Verfuegbar);
					return (i * PMM_BITS_PER_ELEMENT + j) * MM_BLOCK_SIZE;
				}
			}
		}
	}
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
	asm volatile("lock bts %1,%2;"
				"setc %0": "=r"(bit_status): "r"(bit & 0xFF), "m"(Map[i]): "cc");
	if(bit_status == 0)
		locked_inc(&pmm_Speicher_Verfuegbar);
	else
		printf("\e[33;mWarning:\e[0m Freed page which was already freed (0x%X)\n", Address);
}

//Für DMA erforderlich
paddr_t pmm_AllocDMA(paddr_t maxAddress, size_t size)
{
	size_t maxIndex = maxAddress / MM_BLOCK_SIZE / PMM_BITS_PER_ELEMENT;
	uint8_t maxBit = (maxAddress / MM_BLOCK_SIZE) % PMM_BITS_PER_ELEMENT;
	size_t i, bitCount, bitAlloc, startIndex, lastIndex;
	uint8_t startBit;
	uint8_t lastBit;
	size_t bitFreed;
	search:
	while(pmm_Speicher_Verfuegbar >= size)
	{
		size_t maxI = MIN(mapSize - 1, maxIndex);
		bitCount = 0;
		startIndex = lastIndex = -1;
		startBit = lastBit = -1;
		for(i = 4; i <= maxI; i++)
		{
			uint8_t bit;
			uint8_t maxBitIndex = (i == maxI) ? maxBit : PMM_BITS_PER_ELEMENT - 1;
			for(bit = 0; bit <= maxBitIndex; bit++)
			{
				bool status;
				asm volatile(
						"bt %2,%1;"
						"setc %0"
						: "=r"(status) : "m"(Map[i]), "r"((uint16_t)bit): "cc");
				if(status)
				{
					if(((lastIndex == i && lastBit == bit - 1) || (lastIndex == i - 1 && lastBit == PMM_BITS_PER_ELEMENT - 1 && bit == 0))
							&& bitCount > 0)
					{
						lastIndex = i;
						lastBit = bit;
						bitCount++;
					}
					else
					{
						bitCount = 1;
						startIndex = lastIndex = i;
						startBit = lastBit = bit;
					}
					if(bitCount == size)
						goto found;
				}
			}
		}
	}
	return 1;

	found:
	bitAlloc = 0;
	for(i = startIndex; i <= lastIndex; i++)
	{
		uint8_t bit;
		uint8_t lastBitIndex = (i == lastIndex) ? lastBit : PMM_BITS_PER_ELEMENT - 1;
		uint8_t startBitIndex = (i == startIndex) ? startBit : 0;
		for(bit = startBitIndex; bit <= lastBitIndex; bit++)
		{
			bool status;
			asm volatile(
					"lock btr %2,%1;"
					"setc %0"
					: "=r"(status), "=m"(Map[i]): "r"((uint16_t)bit): "cc");
			if(!status)
				goto failure;
			else
				bitAlloc++;
		}
	}
	__sync_fetch_and_add(&pmm_Speicher_Verfuegbar, -size);
	return (startIndex * PMM_BITS_PER_ELEMENT + startBit) * MM_BLOCK_SIZE;

	failure:
	bitFreed = bitAlloc;
	//Alles rückgängig machen
	for(i = startIndex; bitAlloc > 0; i++)
	{
		uint8_t bit;
		for(bit = (i == startIndex) ? startBit : 0; bit < PMM_BITS_PER_ELEMENT && bitAlloc > 0; bit++, bitAlloc--)
			asm volatile("lock bts %1,%0": "=m"(Map[i]): "r"((uint16_t)bit): "cc");
	}
	__sync_fetch_and_add(&pmm_Speicher_Verfuegbar, bitFreed);
	goto search;
}
