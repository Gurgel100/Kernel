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
#include "multiboot.h"

//Speicherverwaltung
bool mm_Init(const mmap *map, uint32_t map_length);
void *mm_Alloc(uint64_t Size);
void mm_Free(void *Address, uint64_t Size);

void *mm_SysAlloc(uint64_t Size);
bool mm_SysFree(void *Address, uint64_t Size);

#endif /* MM_H_ */
