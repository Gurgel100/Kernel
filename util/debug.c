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
#include "string.h"

#ifdef DEBUGMODE

static uint64_t DebugRegister[4];

static ihs_t *Debug_Handler(ihs_t *ihs)
{
	static uint64_t Stack[1000];
	static ihs_t State;
	static ihs_t *oldState;
	if(Debugged == false)	//Wenn noch nicht debugged wurde
	{
							//vorübergehend einen neuen Zustand herstellen
		ihs_t new_state = {
					.cs = 0x8,						//Kernelspace
					.ss = 0x10,
					.es = 0x10,
					.ds = 0x10,
					.gs = 0x10,
					.fs = 0x10,

					.rdi = ihs,						//Als Parameter die Adresse zum Zustand des Programms geben

					.rip = (uint64_t)Debug_Main,	//Einsprungspunkt der Debugfunktion

					.rsp = &Stack[999],

					//IRQs einschalten (IF = 1)
					.rflags = 0x202
		};
		memmove(&State, &new_state, sizeof(ihs_t));
		oldState = ihs;
		return &State;
	}
	else	//Wenn schon Debugged wurde wieder normalen Zustand herstellen
	{
		if(ihs->rax == DEBUG_SINGLESTEP)
			oldState->rflags |= 0x10100;
		if(ihs->rax == DEBUG_CONTINUE)
			oldState->rflags = oldState->rflags & ~0x100 | 0x10000;
		Debugged = false;
		return oldState;
	}
}

void Debug_Init()
{
	//Set handlers
	isr_setHandler(0, Debug_Handler);
	isr_setHandler(1, Debug_Handler);
	isr_setHandler(6, Debug_Handler);
	isr_setHandler(8, Debug_Handler);
	isr_setHandler(13, Debug_Handler);
	isr_setHandler(14, Debug_Handler);
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

uint64_t traceRegistersToString(ihs_t *ihs, char *out)
{
	uint64_t offset = 0;
	offset += sprintf(out + offset, "RAX: 0x%08lX                RBX: 0x%08lX\n", ihs->rax, ihs->rbx);
	offset += sprintf(out + offset, "RCX: 0x%08lX                RDX: 0x%08lX\n", ihs->rcx, ihs->rdx);
	offset += sprintf(out + offset, "RSI: 0x%08lX                RDI: 0x%08lX\n", ihs->rsi, ihs->rdi);
	offset += sprintf(out + offset, "RSP: 0x%08lX                RBP: 0x%08lX\n", ihs->rsp, ihs->rbp);
	offset += sprintf(out + offset, "R8 : 0x%08lX                R9 : 0x%08lX\n", ihs->r8, ihs->r9);
	offset += sprintf(out + offset, "R10: 0x%08lX                R11: 0x%08lX\n", ihs->r10, ihs->r11);
	offset += sprintf(out + offset, "R12: 0x%08lX                R13: 0x%08lX\n", ihs->r12, ihs->r13);
	offset += sprintf(out + offset, "R14: 0x%08lX                R15: 0x%08lX\n", ihs->r14, ihs->r15);
	offset += sprintf(out + offset, "RIP: 0x%08lX\n", ihs->rip);
	return offset;
}

uint64_t traceStackToString(uint64_t rsp, uint64_t *rbp, uint8_t length, char *out)
{
	uint64_t rip, size;
	uint64_t i = 0;
	uint64_t offset = 0;

	while(rbp != NULL &&  i++ < length)
	{
		rip = *(rbp + 1);
		rsp = *rbp;
		size = rsp - (uintptr_t)rbp;
		offset += sprintf(out + offset, "0x%08lX(%zu)%s", rip, size, (i % 2 == 0) ? "\n" : "                ");
		rbp = (uint64_t*)rsp;
	}
	if(i % 2 == 0) offset += sprintf(out + offset, "\n");
	return offset;
}
