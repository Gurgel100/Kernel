/*
 * pit.c
 *
 *  Created on: 16.03.2014
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "pit.h"
#include "util.h"

#define CH0		0x40
#define CH1		0x41
#define CH2		0x42
#define CH_BASE	CH0
#define REGINIT	0x43

#define FRQB	1193182

typedef struct{
	pid_t PID;
	uint64_t msec;
	void *Next;
}timerlist_t;

static timerlist_t *Timerlist, *prevTimer;

void pit_Init(uint32_t freq)
{
	pit_InitChannel(0, 2, (uint64_t)(FRQB / freq));

	Timerlist = prevTimer = NULL;
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
void pit_RegisterTimer(pid_t PID, uint64_t msec)
{
	timerlist_t *Timer;
	if(Timerlist == NULL)
	{
		Timerlist = malloc(sizeof(timerlist_t));
		Timer = Timerlist;
	}
	else
		Timer = malloc(sizeof(timerlist_t));

	Timer->Next = NULL;
	Timer->PID = PID;
	Timer->msec = msec;

	if(prevTimer != NULL)
		prevTimer->Next = Timer;
	prevTimer = Timer;

	//Entsprechenden Task schlafen legen
	pm_SleepTask(PID);
}

void pit_Handler(void)
{
	timerlist_t *Timer;
	Uptime++;
	if(Timerlist)
	{
		for(Timer = Timerlist; Timer->Next != NULL; Timer = Timer->Next)
		{
			Timer->msec--;
			if(Timer->msec == 0)
				pm_WakeTask(Timer->PID);
		}
	}
}

#endif
