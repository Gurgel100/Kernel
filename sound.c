/*
 * sound.c
 *
 *  Created on: 18.03.2014
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "sound.h"
#include "pit.h"

#define PIT_FRQ	1193182

void sound_Play(uint32_t freq, uint64_t sleep)
{
	pit_InitChannel(2, 3, PIT_FRQ / freq);

	switch_on();
	Sleep(sleep);
	switch_off();
}

void switch_on()
{
	outb(0x61, inb(0x61) | 0x3);
}

void switch_off()
{
	outb(0x61, inb(0x61) & ~0x3);
}

#endif
