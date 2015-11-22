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
#include "multiboot.h"
#include "stddef.h"
#include "list.h"

#define VMM_FLAGS_WRITE		(1 << 0)	//Wenn gesetzt, dann kann auf die Page auch geschrieben werden ansonsten nur lesen
#define VMM_FLAGS_GLOBAL	(1 << 1)	//Bestimmt, ob die Page global ist
#define VMM_FLAGS_USER		(1 << 2)	//Bestimmt, ob die Page im Usermode ist oder nicht
#define VMM_FLAGS_NX		(1 << 3)	//Bestimmt, ob in der Page ausführbare Daten sind

#define VMM_UNUSED_PAGE		0x4		//Marks page as unused by process

typedef struct{
	paddr_t physAddress;
	void *virtualAddress;
}context_t;

bool vmm_Init(uint64_t Speicher);									//Initialisiert virtuelle Speicherverw.
void *vmm_Alloc(size_t Size);						//Reserviert eine virtuelle Speicherst.
void vmm_Free(void *Address, size_t Size);		//Gibt eine Speicherstelle frei

void *vmm_SysAlloc(size_t Length);
void vmm_SysFree(void *vAddress, size_t Length);

void *vmm_AllocDMA(paddr_t maxAddress, size_t Size, paddr_t *Phys);
list_t vmm_getTables(context_t *context);

void vmm_MapModule(mods *mod);
void vmm_UnMapModule(mods *mod);

uint8_t vmm_Map(void *vAddress, paddr_t pAddress, uint8_t flags, uint16_t avl);

void *getFreePages(void *start, void *end, size_t pages);

paddr_t vmm_getPhysAddress(void *virtualAddress);
uint8_t vmm_ReMap(context_t *src_context, void *src, context_t *dst_context, void *dst, size_t length, uint8_t flags, uint16_t avl);
uint8_t vmm_ContextMap(context_t *context, void *vAddress, paddr_t pAddress, uint8_t flags, uint16_t avl);
uint8_t vmm_ContextUnMap(context_t *context, void *vAddress);

bool vmm_getPageStatus(void *Address);

void vmm_unusePages(void *virt, size_t pages);
void vmm_usePages(void *virt, size_t pages);

context_t *createContext(void);
void deleteContext(context_t *context);
inline void activateContext(context_t *context);

#endif /* VMM_H_ */
