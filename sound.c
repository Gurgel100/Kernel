/*
 * sound.c
 *
 *  Created on: 18.03.2014
 *      Author: pascal
 */

#include "sound.h"
#include "pit.h"
#include "util.h"

#define PIT_FRQ	1193182

static void switch_on()
{
	outb(0x61, inb(0x61) | 0x3);
}

static void switch_off()
{
	outb(0x61, inb(0x61) & ~0x3);
}

void sound_Play(uint32_t freq, uint64_t sleep)
{
	pit_InitChannel(2, 3, PIT_FRQ / freq);

	switch_on();
	Sleep(sleep);
	switch_off();
}
