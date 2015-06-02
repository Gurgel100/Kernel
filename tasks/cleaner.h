/*
 * cleaner.h
 *
 *  Created on: 29.05.2015
 *      Author: pascal
 */

#ifndef CLEANER_H_
#define CLEANER_H_

#include "pm.h"
#include "thread.h"

typedef enum{
	CL_PROCESS, CL_THREAD
}clean_type_t;

typedef struct{
	void *data;
	clean_type_t type;
}clean_entry_t;

void cleaner();

void cleaner_Init();
void cleaner_cleanProcess(process_t *process);
void cleaner_cleanThread(thread_t *thread);

#endif /* CLEANER_H_ */
