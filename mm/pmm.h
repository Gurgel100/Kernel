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

#define PMM_STACK_LENGTH_PER_BLOCK MM_BLOCK_SIZE	//Maximale Anzahl Bytes für die Stacklänge am Anfang

//Eine Speicherstelle = 4kb

typedef uintptr_t paddr_t;

bool pmm_Init(void);					//Initialisiert die physikalische Speicherverwaltung
paddr_t pmm_Alloc(void);				//Allokiert eine Speicherstelle
void pmm_Free(paddr_t Address);		//Gibt eine Speicherstelle frei
paddr_t pmm_AllocDMA(paddr_t maxAddress, size_t Size);
void getRamSize(void);					//Findet heraus, wieviel Speicher vorhanden ist

#endif /* PMM_H_ */
