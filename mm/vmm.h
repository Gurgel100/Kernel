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
#include "stdbool.h"
#include "multiboot.h"
#include "stddef.h"
#include "list.h"

typedef struct{
	uintptr_t physAddress;
	void *virtualAddress;
}context_t;

bool vmm_Init(uint64_t Speicher, uintptr_t Stack);									//Initialisiert virtuelle Speicherverw.
uintptr_t vmm_Alloc(uint64_t Size);						//Reserviert eine virtuelle Speicherst.
void vmm_Free(uintptr_t Address, uint64_t Size);		//Gibt eine Speicherstelle frei

uintptr_t vmm_SysAlloc(uintptr_t vAddress, uint64_t Length, bool Ignore);
void vmm_SysFree(uintptr_t vAddress, uint64_t Length);

void *vmm_AllocDMA(void *maxAddress, size_t Size, void **Phys);
list_t vmm_getTables(context_t *context);

void vmm_MapModule(mods *mod);
void vmm_UnMapModule(mods *mod);

context_t *createContext(void);
void deleteContext(context_t *context);

#endif /* VMM_H_ */
