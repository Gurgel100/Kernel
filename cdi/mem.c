/*
 * mem.c
 *
 *  Created on: 30.07.2013
 *      Author: pascal
 */

#include "mem.h"
#include "stdlib.h"
#include "vmm.h"
#include "memory.h"

/**
 * \german
 * Reserviert einen Speicherbereich.
 *
 * @param size Größe des Speicherbereichs in Bytes
 * @param flags Flags, die zusätzliche Anforderungen beschreiben
 *
 * @return Eine cdi_mem_area bei Erfolg, NULL im Fehlerfall
 * \endgerman
 * \english
 * Allocates a memory area.
 *
 * @param size Size of the memory area in bytes
 * @param flags Flags that describe additional requirements
 *
 * @return A cdi_mem_area on success, NULL on failure
 * \endenglish
 *///TODO: restliche Flags implementieren
struct cdi_mem_area* cdi_mem_alloc(size_t size, cdi_mem_flags_t flags)
{
	struct cdi_mem_area *Area;
	struct cdi_mem_sg_item *sg_item;
	size_t alignment = flags & CDI_MEM_ALIGN_MASK;
	size_t asize;
	void *vaddr, *paddr;

	//Wir vergeben nur ganze Pages
	size = (size + 0xFFF) & (~0xFFF);

	//Wenn die physische Adresse nicht interessiert, können wir das Alignment ignorieren
	if(flags & CDI_MEM_VIRT_ONLY)
		alignment = 0;

	//TODO: Momentan wird Alignment nur bei zusammenhängenden Seiten unterstützt
	if(alignment > 12)
	{
		if(!(flags & CDI_MEM_PHYS_CONTIGUOUS))
			return NULL;
		asize = size + (1ULL << alignment);
	}

	//Speicher reservieren
	void *maxAddress;
	if(flags & CDI_MEM_DMA_16M)
		maxAddress = 16777216;
	else if(flags & CDI_MEM_DMA_4G)
		maxAddress = 4294967296;
	vaddr = vmm_AllocDMA(maxAddress, size / MM_BLOCK_SIZE, &paddr);
	if(vaddr == NULL)
		return NULL;

	//Wie oben erwähnt Alignment umsetzen
	if(alignment > 12)
	{
		uintptr_t amask = (1ULL << alignment) - 1;
		uintptr_t astart = ((uintptr_t)paddr + amask) & (~amask);
		uintptr_t off = astart - (uintptr_t)paddr;

		//TODO: Die Stücke, die abgeschnitten wurden, bleiben immer reserviert

		paddr = (void*)astart;
		vaddr = (void*)(((uintptr_t)vaddr) + off);
	}

	//cdi_mem_area anlegen und befüllen
	sg_item = malloc(sizeof(*sg_item));
	*sg_item = (struct cdi_mem_sg_item){
		.start = (uintptr_t)paddr,
		.size = size
	};

	Area = malloc(sizeof(*Area));
	*Area = (struct cdi_mem_area){
			.size = size,
			.vaddr = vaddr,
			.paddr = {
					.num = 1,
					.items = sg_item
			}
	};

	return Area;
}

/**
 * \german
 * Reserviert physisch zusammenhägenden Speicher an einer definierten Adresse
 *
 * @param paddr Physische Adresse des angeforderten Speicherbereichs
 * @param size Größe des benötigten Speichers in Bytes
 *
 * @return Eine cdi_mem_area bei Erfolg, NULL im Fehlerfall
 * \endgerman
 * \english
 * Reserves physically contiguous memory at a defined address
 *
 * @param paddr Physical address of the requested memory aread
 * @param size Size of the requested memory area in bytes
 *
 * @return A cdi_mem_area on success, NULL on failure
 * \endenglish
 */
struct cdi_mem_area* cdi_mem_map(uintptr_t paddr, size_t size)
{
	struct cdi_mem_area *area;
	struct cdi_mem_sg_item *sgitem;

	size = (size + 0xFFF) & ~0xFFF;

	void *vaddr = getFreePages((void*)KERNELSPACE_START, (void*)KERNELSPACE_END, size / MM_BLOCK_SIZE);

	size_t i;
	for(i = 0; i < size; i += MM_BLOCK_SIZE)
	{
		vmm_Map((uintptr_t)vaddr + i, paddr + i, VMM_FLAGS_NX | VMM_FLAGS_WRITE | VMM_FLAGS_GLOBAL, 0);
	}

	area = malloc(sizeof(*area));
	sgitem = malloc(sizeof(*sgitem));

	*sgitem = (struct cdi_mem_sg_item){
		.size = size,
		.start = paddr
	};

	*area = (struct cdi_mem_area){
		.size = size,
		.vaddr = vaddr,
		.paddr = {
			.num = 1,
			.items = sgitem
		}
	};

	return area;
}

/**
 * \german
 * Gibt einen durch cdi_mem_alloc oder cdi_mem_map reservierten Speicherbereich
 * frei
 * \endgerman
 * \english
 * Frees a memory area that was previously allocated by cdi_mem_alloc or
 * cdi_mem_map
 * \endenglish
 */
void cdi_mem_free(struct cdi_mem_area* p)
{
	vmm_SysFree(p->vaddr, p->size / 4096);
}
