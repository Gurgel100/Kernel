/*
 * shm.h
 *
 *  Created on: 19.02.2015
 *      Author: pascal
 */

#ifndef SHM_H_
#define SHM_H_

#include "stdint.h"
#include "pm.h"

void shm_Init();
uint64_t shm_create(size_t size);
void *shm_open(process_t *process, uint64_t id);
void shm_close(process_t *process, uint64_t id);

#endif /* SHM_H_ */
