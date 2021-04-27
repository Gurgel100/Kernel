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
#include "assert.h"
#include "stdio.h"

#define PMM_BITS_PER_ELEMENT	(sizeof(*Map) * 8)
#define PMM_MAP_ALIGN_SIZE(x)	((x + (sizeof(*Map) - 1)) & ~(sizeof(*Map) - 1))
#define ROUND_UP_PAGESIZE(x)	(((x) + MM_BLOCK_SIZE - 1) & ~(MM_BLOCK_SIZE - 1))
#define ROUND_DOWN_PAGESIZE(x)	((x) & ~(MM_BLOCK_SIZE - 1))

#define MAX(a, b)				((a > b) ? a : b)
#define MIN(a, b)				((a < b) ? a : b)

//Anfang und Ende des Kernels
extern uint8_t kernel_start;
extern uint8_t kernel_end;

static uint64_t pmm_totalMemory;		//Maximal verfügbarer RAM (physisch)
static uint64_t pmm_totalPages;			//Gesamtanzahl an phys. Pages
static uint64_t pmm_freePages;			//Verfügbarer (freier) physischer Speicher (4kb)
static uint64_t pmm_Kernelsize;			//Grösse des Kernels in Bytes

//Ein Bit ist gesetzt, wenn die Page frei ist
static uint64_t *Map;
static size_t mapSize;			//Grösse der Bitmap

/*
 * Initialisiert die physikalische Speicherverwaltung
 */
bool pmm_Init()
{
	pmm_Kernelsize = &kernel_end - &kernel_start;
	paddr_t maxAddress = 0;
	mmap *map = (mmap*)(uintptr_t)MBS->mbs_mmap_addr;
	uint32_t mapLength = MBS->mbs_mmap_length;

	//"Nachschauen", wieviel Speicher vorhanden ist
	printf("Provided memory map:\n");
	for (mmap *m = map; m < (mmap*)(uintptr_t)(MBS->mbs_mmap_addr + mapLength); m = (mmap*)((uintptr_t)m + m->size + 4))
	{
		pmm_totalMemory += m->length;
		maxAddress = MAX(m->base_addr + m->length, maxAddress);
		printf("%p - %p -> %s (%i)\n", m->base_addr, m->base_addr + m->length - 1, m->type == 1 ? "usable" : "reserved", m->type);
		
		// Adjust memory map because we don't want to use the page at address 0 (for reasons...)
		if (m->base_addr == 0) {
			m->base_addr = 0x1000;
			m->length -= 0x1000;
		}
	}

	pmm_totalPages = pmm_totalMemory / MM_BLOCK_SIZE;
	assert(pmm_totalMemory % MM_BLOCK_SIZE == 0);

	// Get memory for bitmap
	// TODO: what if we use a range which is not mapped?
	size_t required_size = PMM_MAP_ALIGN_SIZE(maxAddress / MM_BLOCK_SIZE / 8);
	for (mmap *m = map; m < (mmap*)(uintptr_t)(MBS->mbs_mmap_addr + mapLength); m = (mmap*)((uintptr_t)m + m->size + 4)) {
		if (m->type == 1) {
			uintptr_t base_addr = m->base_addr;
			size_t size = m->length;
			if ((base_addr < (uintptr_t)&kernel_start && MIN((uintptr_t)&kernel_start - base_addr, size) >= required_size) || base_addr > (uintptr_t)&kernel_end) {
				// Take memory from the beginning of the map
				Map = (uint64_t*)base_addr;
				mapSize = required_size;

				m->base_addr += required_size;
				m->length -= required_size;
				break;
			} else if (base_addr < (uintptr_t)&kernel_end && size - ((uintptr_t)&kernel_end - base_addr) >= required_size) {
				// Take memory from the end of the map
				Map = (uint64_t*)(base_addr + size - required_size);
				mapSize = required_size;

				m->length -= required_size;
				break;
			}
		}
	}

	if (Map == NULL) {
		Panic("PMM", "No memory found to place bitmap");
	}
	memset(Map, 0, mapSize);

	//Map analysieren und entsprechende Einträge in die Speicherverwaltung machen
	while(map < (mmap*)(uintptr_t)(MBS->mbs_mmap_addr + mapLength))
	{
		if(map->type == 1 && map->base_addr >= 0x100000) {
			paddr_t map_start = ROUND_UP_PAGESIZE(map->base_addr);
			size_t map_end = ROUND_DOWN_PAGESIZE(map->base_addr + map->length);
			printf("marking as free: %p - %p\n", map_start, map_end);
			for(paddr_t i = map_start; i < map_end; i += MM_BLOCK_SIZE) {
				if(i < (paddr_t)&kernel_start || i > (paddr_t)&kernel_end)
				{
					pmm_Free(i);
				}
			}
		}
		map = (mmap*)((uintptr_t)map + map->size + 4);
	}

	#ifdef DEBUGMODE
	printf("    %u MB Speicher gefunden\n    %u GB Speicher gefunden\n", pmm_totalMemory >> 20, pmm_totalMemory >> 30);
	#endif

	SysLog("PMM", "Initialisierung abgeschlossen");
	return true;
}

void pmm_markPageReserved(paddr_t address) {
	size_t i = address / MM_BLOCK_SIZE / PMM_BITS_PER_ELEMENT;
	uint8_t bit = (address / MM_BLOCK_SIZE) % PMM_BITS_PER_ELEMENT;
	__sync_fetch_and_and(&Map[i], ~(1ul << bit));
	__sync_fetch_and_sub(&pmm_freePages, 1);
}

/*
 * Reserviert eine Speicherstelle
 * Rückgabewert:	phys. Addresse der Speicherstelle
 * 					1 = Kein phys. Speicherplatz mehr vorhanden
 */
paddr_t pmm_Alloc()
{
	while(pmm_freePages > 0)
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
							"setc %1": "=m"(Map[i]), "=r"(status): "r"(j): "cc", "memory");
				if(status)
				{
					__sync_fetch_and_sub(&pmm_freePages, 1);
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
				"setc %0": "=r"(bit_status): "r"(bit & 0xFF), "m"(Map[i]): "cc", "memory");
	if(bit_status == 0)
		__sync_fetch_and_add(&pmm_freePages, 1);
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
	while(pmm_freePages >= size)
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
	__sync_fetch_and_add(&pmm_freePages, -size);
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
	__sync_fetch_and_add(&pmm_freePages, bitFreed);
	goto search;
}

uint64_t pmm_getTotalPages()
{
	return pmm_totalPages;
}

uint64_t pmm_getFreePages()
{
	return pmm_freePages;
}

paddr_t pmm_getHighestAddress() {
	return mapSize * PMM_BITS_PER_ELEMENT * MM_BLOCK_SIZE;
}
