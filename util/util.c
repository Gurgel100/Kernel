/*
 * util.c
 *
 *  Created on: 14.07.2012
 *      Author: pascal
 */

#include "util.h"
#include "pit.h"

void outb(uint16_t Port, uint8_t Data)
{
	asm volatile("outb %0,%1" : :"a" (Data), "d" (Port));
}

uint8_t inb(uint16_t Port)
{
	uint8_t Data;
	asm volatile("inb %1,%0" :"=a"(Data) :"Nd" (Port));
	return Data;
}

void outw(uint16_t Port, uint16_t Data)
{
	asm volatile("outw %0,%1" : :"a" (Data), "d" (Port));
}

uint16_t inw(uint16_t Port)
{
	uint16_t Data;
	asm volatile("inw %1,%0" :"=a"(Data) :"Nd" (Port));
	return Data;
}

void outd(uint16_t Port, uint32_t Data)
{
	asm volatile("out %0,%1" : :"a" (Data), "d" (Port));
}

uint32_t ind(uint16_t Port)
{
	uint32_t Data;
	asm volatile("in %1,%0" :"=a"(Data) :"Nd" (Port));
	return Data;
}

void outq(uint16_t Port, uint64_t Data)
{
	//asm volatile("outl %0,%1" : :"a" (Data), "d" (Port));
}

uint64_t inq(uint16_t Port)
{
	uint64_t Data;
	//asm volatile("inl %1,%0" :"=a"(Data) :"Nd" (Port));
	return Data;
}

void Sleep(uint64_t msec)
{
	uint64_t start = Uptime;
	while(1)
	{
		if(start + msec <= Uptime)
			break;
		asm volatile("hlt");
	}
}
