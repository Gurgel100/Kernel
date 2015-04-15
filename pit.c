/*
 * pit.c
 *
 *  Created on: 16.03.2014
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "pit.h"
#include "util.h"
#include "list.h"
#include "stdlib.h"
#include "scheduler.h"

#define CH0		0x40
#define CH1		0x41
#define CH2		0x42
#define CH_BASE	CH0
#define REGINIT	0x43

#define FRQB	1193182

typedef struct{
	thread_t *thread;
	uint64_t timeout;
}timer_t;

static list_t Timerlist;

void pit_Init(uint32_t freq)
{
	pit_InitChannel(0, 2, (uint64_t)(FRQB / freq));

	Timerlist = NULL;
	Uptime = 0;
}

void pit_InitChannel(uint8_t channel, uint8_t mode, uint16_t data)
{
	if(channel > 2) return;
	outb(REGINIT, 0 | ((mode & 0x7) << 1) | (0x3 << 4) | (channel << 6));
	outb(CH_BASE + channel, data & 0xFF);
	outb(CH_BASE + channel, data >> 8);
}

//Registriert einen Timer
void pit_RegisterTimer(thread_t *thread, uint64_t msec)
{
	if(msec != 0)
	{
		timer_t *Timer;
		size_t i;

		if(Timerlist == NULL)
			Timerlist = list_create();

		Timer = malloc(sizeof(timer_t));

		Timer->thread = thread;
		Timer->timeout = Uptime + msec;

		//Timerliste sortiere, sodass das Element vorne immer das Element ist, welches
		//zuerst abläuft
		timer_t *item;
		for(i = 0; (item = list_get(Timerlist, i)); i++)
		{
			if(item->timeout > Timer->timeout)
				break;
		}

		list_insert(Timerlist, i, Timer);

		//Entsprechenden Thread schlafen legen
		thread_block(thread);
	}

	//Thread switchen
	yield();
}

void pit_Handler(void)
{
	timer_t *Timer;
	size_t i = 0;
	Uptime++;
	if(Timerlist)
	{
		while((Timer = list_get(Timerlist, i)))
		{
			if(Timer->timeout > Uptime)
				break;
			thread_unblock(Timer->thread);
			free(list_remove(Timerlist, i));
			i++;
		}
	}
}

#endif
