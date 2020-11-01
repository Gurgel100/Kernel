/*
 * system.c
 *
 *  Created on: 11.08.2012
 *      Author: pascal
 */

#include "system.h"
#include "pmm.h"
#include "console.h"
#include "stdio.h"
#include "debug.h"
#include "lock.h"
#include "scheduler.h"

extern uint64_t Uptime;

char system_panic_buffer[25 * 80];
static lock_t panic_buffer_lock = LOCK_INIT;

/*
 * Speichert Systeminformationen in die übergebene Struktur
 * Parameter:	Adresse auf die Systeminformationen-Struktur
 */
void getSystemInformation(SIS *Struktur)
{
	Struktur->physSpeicher = pmm_getTotalPages() * 4096;
	Struktur->physFree = pmm_getFreePages() * 4096;
	Struktur->Uptime = Uptime;
}

void system_panic_enter()
{
	// Der lock wird nie mehr freigegeben, deshalb können wir das hier so machen
	static lock_node_t lock_node;
	if(!try_lock(&panic_buffer_lock, &lock_node))
	{
		asm volatile("cli;hlt");
		__builtin_unreachable();
	}
}

void system_panic()
{
	console_switch(0);
	printf("\e[37;41mSystem panic\t\t");
	if(currentThread != NULL)
		printf("process: %lu(%.20s), tid: %lu", currentProcess->PID, currentProcess->cmd, currentThread->tid);
	printf("\n\nThe following error occurred:\n%s", system_panic_buffer);
	asm volatile("cli;hlt");
	__builtin_unreachable();
}
