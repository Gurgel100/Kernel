/*
 * pmm.h
 *
 *  Created on: 09.07.2012
 *      Author: pascal
 */

#ifndef PMM_H_
#define PMM_H_

#include "multiboot.h"
#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

//Eine Speicherstelle = 4kb

typedef uintptr_t paddr_t;

bool pmm_Init(void);					//Initialisiert die physikalische Speicherverwaltung
void pmm_markPageReserved(paddr_t address);
paddr_t pmm_Alloc(void);				//Allokiert eine Speicherstelle
void pmm_Free(paddr_t Address);		//Gibt eine Speicherstelle frei
paddr_t pmm_AllocDMA(paddr_t maxAddress, size_t Size);
uint64_t pmm_getTotalPages();
uint64_t pmm_getFreePages();
paddr_t pmm_getHighestAddress();

#endif /* PMM_H_ */
