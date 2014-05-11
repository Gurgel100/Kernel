/*
 * mm.h
 *
 *  Created on: 02.07.2012
 *      Author: pascal
 */

#ifndef MM_H_
#define MM_H_

#include "stdint.h"
#include "paging.h"
#include "stdbool.h"

//Speicherverwaltung
bool mm_Init(void);
uintptr_t mm_Alloc(uint64_t Size);
void mm_Free(uintptr_t Address, uint64_t Size);

uintptr_t mm_SysAlloc(uint64_t Size);
bool mm_SysAllocAddr(uintptr_t Address, uint64_t Size);
bool mm_SysFree(uintptr_t Address, uint64_t Size);

#endif /* MM_H_ */
