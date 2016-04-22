/*
 * debug.c
 *
 *  Created on: 15.03.2013
 *      Author: pascal
 */

#include "debug.h"
#include "config.h"
#include "display.h"
#include "stdio.h"
#include "stdlib.h"
#include "disasm.h"

#ifdef DEBUGMODE

static uint64_t DebugRegister[4];

void Debug_Init()
{
	Debugged = false;
	DebugRegister[0] = 0;
	DebugRegister[1] = 0;
	DebugRegister[2] = 0;
	DebugRegister[3] = 0;
	asm volatile("mov %0,%%db7" : : "r"((uint64_t)0x400));
}

void setBreak(uint64_t BreakPoint);
void deleteBreak(uint8_t Index);
uint64_t hextou(char *s);
uint64_t atou(char *s);

void Debug_Main(ihs_t *ihs)
{
	static char lastInput[50];
	static char buffer[50];
	uint64_t retvalue = 0;
	//Debugconsole
	setColor(BG_GREEN | CL_WHITE);
	printf("Debugconsole\n");

	//Falls Fehler:
	if(ihs->interrupt != 1)
	{
		setColor(BG_BLACK | CL_RED);
		printf("Exception %u!\n", ihs->interrupt);
		setColor(BG_BLACK | CL_WHITE);
		printf("Errorcode: 0x%X%X\n", ihs->error >> 32, ihs->error & 0xFFFFFFFF);
	}

	traceRegisters(ihs);
	printf("RFLAGS: 0x%X%X\n", ihs->rflags >> 32, ihs->rflags & 0xFFFFFFFF);

	printf("%u : %X%X: ", ihs->cs, ihs->rip >> 32, ihs->rip & 0xFFFFFFFF);
	disasm(ihs->rip);
	//printf("%u : %X%X: 0x%X%X\n", ihs->cs, ihs->rip >> 32, ihs->rip & 0xFFFFFFFF, (*value) >> 32, (*value) & 0xFFFFFFFF);

	while(true)
	{
		char c;
		int i = 0;
		printf(">");
		while((c = getchar()) != '\n' && c != '\0')
		{
			switch(c)
			{
				case '\b':	//Backspace
					if(i > 0)
						buffer[--i] = 0;
				break;
				default:
					buffer[i++] = c;
				break;
			}
			putchar(c);
		}
		if(i)
			buffer[i] = '\0';
		putchar('\n');
		if(strcmp(buffer, "s") == 0)
		{
			retvalue = DEBUG_SINGLESTEP;
			break;
		}
		else if(strcmp(buffer, "c") == 0)
		{
			retvalue = DEBUG_CONTINUE;
			break;
		}
		else if((buffer[0] == 'l') && (buffer[1] == 'b'))
			setBreak(hextou(&buffer[5]));
		else if(buffer[0] == 'd')
			deleteBreak(atou(&buffer[2]));
		else if(strcmp(buffer, "info b") == 0)
		{
			uint8_t i;
			for(i = 0; i < 3; i++)
			{
				if(DebugRegister[i])
					printf("%u: 0x%X%X\n", i, DebugRegister[i] >> 32, DebugRegister[i]);
				else
					printf("%u: not defined\n", i);
			}
		}
		else if(buffer[0] == 'x')	//Speicher anzeigen
		{
			uint64_t *a = strtol(&buffer[2], NULL, 0);
			printf("%s: 0x%X%X\n", &buffer[2], *a & 0xFFFFFFFF, *a >> 32);
		}
		else
			printf("Syntax error at '%s'\n", buffer);
	}

	Debugged = true;

	asm volatile("int $0x1" : : "a"(retvalue));	//return retvalue;
}

void setBreak(uint64_t BreakPoint)
{
	uint8_t i;
	for(i = 0; i <= 3; i++)
	{
		if(DebugRegister[i] == 0)
			break;
	}
	asm volatile(
			"mov %0,%%db0;"
			"mov %%db7,%%rax;"
			"or %1,%%rax;"
			"mov %%rax,%%db7"
			: : "r"(BreakPoint), "r"((uint64_t)0x3 << (2 * i)) : "rax");
	DebugRegister[i] = BreakPoint;
}

void deleteBreak(uint8_t Index)
{
	DebugRegister[Index] = 0;
	asm volatile(
			"mov %%db7,%%rax;"
			"and %0,%%rax;"
			"mov %%rax,%%db7"
			: : "r"(~((uint64_t)0x3 << (2 * Index))) : "rax");
}

uint64_t hextou(char *s)
{
	uint64_t i = 0;
	uint64_t r = 0;
	do
	{
		r <<= 4;
		if(s[i] <= '9' && s[i] >= '0')
		{
			r += s[i] - '0';
		}
		else if(s[i] <= 'F' && s[i] >= 'A')
		{
			r += s[i] - 'A' + 10;
		}
		else if(s[i] <= 'f' && s[i] >= 'a')
		{
			r += s[i] - 'a' + 10;
		}
	}
	while(++i < strlen(s));
	return r;
}

uint64_t atou(char *s)
{
	uint64_t i = 0;
	uint64_t x = 0;
	do
	{
		x *= 10;
		x += s[i++] - '0';
	}
	while(i < strlen(s));
	return x;
}

#endif

void traceRegisters(ihs_t *ihs)
{
	setColor(BG_BLACK | CL_WHITE);
	//RAX
	printf("RAX: 0x%X%X                ", ihs->rax >> 32, ihs->rax & 0xFFFFFFFF);

	//RBX
	printf("RBX: 0x%X%X\n", ihs->rbx >> 32, ihs->rbx & 0xFFFFFFFF);

	//RCX
	printf("RCX: 0x%X%X                ", ihs->rcx >> 32, ihs->rcx & 0xFFFFFFFF);

	//RDX
	printf("RDX: 0x%X%X\n", ihs->rdx >> 32, ihs->rdx & 0xFFFFFFFF);

	//RSI
	printf("RSI: 0x%X%X                ", ihs->rsi >> 32, ihs->rsi & 0xFFFFFFFF);

	//RDI
	printf("RDI: 0x%X%X\n", ihs->rdi >> 32, ihs->rdi & 0xFFFFFFFF);

	//RSP
	printf("RSP: 0x%X%X                ", ihs->rsp >> 32, ihs->rsp & 0xFFFFFFFF);

	//RBP
	printf("RBP: 0x%X%X\n", ihs->rbp >> 32, ihs->rbp & 0xFFFFFFFF);

	//R8
	printf("R8 : 0x%X%X                ", ihs->r8 >> 32, ihs->r8 & 0xFFFFFFFF);

	//R9
	printf("R9 : 0x%X%X\n", ihs->r9 >> 32, ihs->r9 & 0xFFFFFFFF);

	//R10
	printf("R10: 0x%X%X                ", ihs->r10 >> 32, ihs->r10 & 0xFFFFFFFF);

	//R11
	printf("R11: 0x%X%X\n", ihs->r11 >> 32, ihs->r11 & 0xFFFFFFFF);

	//R12
	printf("R12: 0x%X%X                ", ihs->r12 >> 32, ihs->r12 & 0xFFFFFFFF);

	//R13
	printf("R13: 0x%X%X\n", ihs->r13 >> 32, ihs->r13 & 0xFFFFFFFF);

	//R14
	printf("R14: 0x%X%X                ", ihs->r14 >> 32, ihs->r14 & 0xFFFFFFFF);

	//R15
	printf("R15: 0x%X%X\n", ihs->r15 >> 32, ihs->r15 & 0xFFFFFFFF);

	//RIP
	printf("RIP: 0x%X%X\n", ihs->rip >> 32, ihs->rip & 0xFFFFFFFF);
}

void traceStack(uint64_t rsp, uint8_t length)
{
	uint8_t i;
	for(i = 0; i < length; i += 2)
	{
		uint64_t *Value = (rsp + i * 8);
		printf("%u: 0x%X%X                ", i + 1, (*Value) >> 32, (*Value) & 0xFFFFFFFF);
		Value = (rsp + (i + 1) * 8);
		printf("%u: 0x%X%X\n", i + 2, (*Value) >> 32, (*Value) & 0xFFFFFFFF);
	}
}
