/*
 * vmm.h
 *
 *  Created on: 09.07.2012
 *      Author: pascal
 */

#ifndef VMM_H_
#define VMM_H_

#include "stdint.h"
#include "mm.h"
#include "pmm.h"
#include "stdbool.h"
#include "stddef.h"

#define VMM_FLAGS_WRITE		(1 << 0)	//Wenn gesetzt, dann kann auf die Page auch geschrieben werden ansonsten nur lesen
#define VMM_FLAGS_GLOBAL	(1 << 1)	//Bestimmt, ob die Page global ist
#define VMM_FLAGS_USER		(1 << 2)	//Bestimmt, ob die Page im Usermode ist oder nicht
#define VMM_FLAGS_NX		(1 << 3)	//Bestimmt, ob in der Page ausführbare Daten sind
#define VMM_FLAGS_PWT		(1 << 4)	//Bestimmt, ob die Page mit write-through policy gecacht werden soll
#define VMM_FLAGS_NO_CACHE	(1 << 5)	//Bestimmt, ob die Page nicht gecacht werden soll
#define VMM_FLAGS_ALLOCATE	(1 << 6)	//Bestimmt, ob der physikalische Speicher schon alloziiert sein soll

#define VMM_UNUSED_PAGE		0x4		//Marks page as unused by process

typedef struct vmm_context context_t;

extern context_t kernel_context;

bool vmm_Init();									//Initialisiert virtuelle Speicherverw.
void *vmm_Alloc(context_t *context, size_t Size);						//Reserviert eine virtuelle Speicherst.
void vmm_Free(context_t *context, void *Address, size_t Size);		//Gibt eine Speicherstelle frei

void *vmm_SysAlloc(size_t Length);
void vmm_SysFree(void *vAddress, size_t Length);

void *vmm_AllocDMA(paddr_t maxAddress, size_t Size, paddr_t *Phys);
void vmm_getPageTables(void(*callback)(paddr_t));

/**
 * \brief Maps a memory area.
 *
 * If vAddress is NULL then it allocates a virtual memory area big enough to hold pages pages.
 * If pAddress is 0 it will map the memory area with the VMM_UNUSED_PAGE flag so that a physical page will be allocated on the first access.
 *
 * This function locks the vmm_lock lock.
 *
 * @param context The context of the virtual memory
 * @param vAddress Virtual address to map the memory area to
 * @param pAddress Physical address of the memory area
 * @param pages Number pages the memory area spans
 * @param flags Flags with access rights
 * @return NULL on failure otherwise virtual address
 */
void *vmm_Map(context_t *context, void *vAddress, paddr_t pAddress, size_t pages, uint8_t flags);

/**
 * \brief Maps a memory area with surrounding guard pages.
 *
 * If vAddress is NULL then it allocates a virtual memory area big enough to hold pages pages.
 * If pAddress is 0 it will map the memory area with the VMM_UNUSED_PAGE flag so that a physical page will be allocated on the first access.
 *
 * This function locks the vmm_lock lock.
 *
 * @param context The context of the virtual memory
 * @param vAddress Virtual address to map the memory area to
 * @param pAddress Physical address of the memory area
 * @param pages Number pages the memory area spans
 * @param flags Flags with access rights
 * @return NULL on failure otherwise virtual address
 */
void *vmm_MapGuarded(context_t *context, void *vAddress, paddr_t pAddress, size_t pages, uint8_t flags);

/**
 * \brief Unmaps a memory area.
 *
 * This function locks the vmm_lock lock.
 *
 * @param context The context of the virtual memory
 * @param vAddress Virtual address of the start of the memory area
 * @param pages Number of pages the memory area spans
 * @param freePages Indicates if the physical memory should be freed
 */
void vmm_UnMap(context_t *context, void *vAddress, size_t pages, bool freePages);

/**
 * \brief Unmaps a memory area with surrounding guard pages.
 *
 * This function locks the vmm_lock lock.
 *
 * @param context The context of the virtual memory
 * @param vAddress Virtual address of the start of the memory area
 * @param pages Number of pages the memory area spans
 * @param freePages Indicates if the physical memory should be freed
 */
void vmm_UnMapGuarded(context_t *context, void *vAddress, size_t pages, bool freePages);

paddr_t vmm_getPhysAddress(context_t *context, void *virtualAddress);
uint8_t vmm_ReMap(context_t *src_context, void *src, context_t *dst_context, void *dst, size_t length, uint8_t flags);

bool vmm_getPageStatus(context_t *context, void *Address);

void vmm_unusePages(context_t *context, void *virt, size_t pages);
void vmm_usePages(context_t *context, void *virt, size_t pages);

bool vmm_userspacePointerValid(const void *ptr, const size_t size);

context_t *createContext(void);
void deleteContext(context_t *context);
void activateContext(context_t *context);

//Interrupt handler
int vmm_handlePageFault(context_t *context, void *page, uint64_t errorcode);

#endif /* VMM_H_ */
