/*
 * isr.c
 *
 *  Created on: 15.07.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "isr.h"
#include "config.h"
#include "display.h"
#include "stdio.h"
#include "stdlib.h"
#include "util.h"
//#include "mm.h"
#include "pic.h"
#include "debug.h"
#include "pit.h"

typedef struct{
		void (*Handler)(ihs_t *ihs);
		void *Next;
}HandlerList_t;

typedef struct{
		HandlerList_t *Handlers;
		uint64_t numHandlers;
}irqHandlers;

static irqHandlers *Handlers[NUM_IRQ];

//Test
uint64_t Counter;

void isr_Init()
{
	int16_t i;
	for(i = 0; i < NUM_IRQ; i++)
		Handlers[i] = malloc(sizeof(irqHandlers));
	Counter = 0;
}

void isr_RegisterIRQHandler(uint16_t irq, void *Handler)
{
	if(irq >= NUM_IRQ)
		return;

	HandlerList_t *Element;
	Element = malloc(sizeof(HandlerList_t));
	Element->Next = Handlers[irq]->Handlers;
	Element->Handler = Handler;
	Handlers[irq]->numHandlers++;
	Handlers[irq]->Handlers = Element;
}

extern void keyboard_Handler(ihs_t *ihs);
extern ihs_t *syscall_Handler(ihs_t *ihs);
extern ihs_t *pm_Schedule(ihs_t *ihs);
extern void cdi_irq_handler(uint8_t irq);
extern void pit_Handler(void);

void traceRegisters(ihs_t *ihs);
void traceStack(uint64_t rsp, uint8_t length);

ihs_t *isr_Handler(ihs_t *ihs)
{
	ihs_t *new_ihs = ihs;
	Counter++;
	if(ihs->interrupt < 32)			//Exceptionhandler
	{
		switch(ihs->interrupt)
		{
			case 0:
				#ifdef DEBUGMODE
					new_ihs = exception_Debug(ihs);
				#else
					exception_DivideByZero(ihs);
				#endif
			break;
			case 1:
				#ifdef DEBUGMODE
					new_ihs = exception_Debug(ihs);
				#endif
			break;
			case 2:
				exception_NonMaskableInterrupt(ihs);
			break;
			case 3:
				exception_BreakPoint(ihs);
			break;
			case 4:
				exception_Overflow(ihs);
			break;
			case 5:
				exception_BoundRange(ihs);
			break;
			case 6:
				#ifdef DEBUGMODE
					new_ihs = exception_Debug(ihs);
				#else
					exception_InvalidOpcode(ihs);
				#endif
			break;
			case 7:
				exception_DeviceNotAvailable(ihs);
			break;
			case 8:
				#ifdef DEBUGMODE
					new_ihs = exception_Debug(ihs);
				#else
					exception_DoubleFault(ihs);
				#endif
			break;
			case 9:
				exception_CoprocessorSegmentOverrun(ihs);
			break;
			case 10:
				exception_InvalidTSS(ihs);
			break;
			case 11:
				exception_SegmentNotPresent(ihs);
			break;
			case 12:
				exception_StackFault(ihs);
				break;
			case 13:
				#ifdef DEBUGMODE
					new_ihs = exception_Debug(ihs);
				#else
					exception_GeneralProtection(ihs);
				#endif
			break;
			case 14:
				#ifdef DEBUGMODE
					new_ihs = exception_Debug(ihs);
				#else
					exception_PageFault(ihs);
				#endif
			break;
			case 16:
				exception_MF(ihs);
			break;
			case 17:
				exception_AlignmentCheck(ihs);
			break;
			case 18:
				exception_MachineCheck(ihs);
			break;
			case 19:
				exception_XF(ihs);
			break;
		}
	}
	else if(ihs->interrupt < 48)	//Ab hier fangen IRQs an
	{
		uint8_t irq = ihs->interrupt - 32;
		/*uint64_t i;
		HandlerList_t *Handler;
		Handler = Handlers[irq]->Handlers;
		for(i = 0; i < Handlers[irq]->numHandlers; i++)
		{
			if(Handler)
			{
				Handler->Handler(ihs);
				Handler = Handler->Next;
			}
		}*/
		cdi_irq_handler(irq);
		switch(ihs->interrupt)
		{
			case 32:
				pit_Handler();
				if((Uptime % 50) == 0)
					new_ihs = pm_Schedule(ihs);
			break;
			case 33:
				keyboard_Handler(ihs);
			break;
		}
		pic_SendEOI(ihs->interrupt - 32);		//PIC sagen, dass IRQ behandelt wurde
	}
	else
		new_ihs = syscall_Handler(new_ihs);
	return new_ihs;
}

//Divide by Zero
void exception_DivideByZero(ihs_t *ihs)
{}

//Debug
ihs_t *exception_Debug(ihs_t *ihs)
{
	#ifdef DEBUGMODE
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
			oldState->rflags |= 0x100;
		if(ihs->rax == DEBUG_CONTINUE)
			oldState->rflags &= ~0x100;
		Debugged = false;
		return oldState;
	}
	#endif
}

//non maskable interrupt
void exception_NonMaskableInterrupt(ihs_t *ihs)
{}

//Breakpoint
void exception_BreakPoint(ihs_t *ihs)
{}

//Overflow
void exception_Overflow(ihs_t *ihs)
{}

//Bound Range
void exception_BoundRange(ihs_t *ihs)
{}

//Invalid opcode
void exception_InvalidOpcode(ihs_t *ihs)
{
	Display_Clear();
	setColor(BG_BLACK | CL_RED);
	printf("Exception 6: Invalid Opcode\n");
	traceRegisters(ihs);
	printf("Stack-backtrace:\n");
	traceStack(ihs->rsp, 10);
	asm volatile("cli;hlt");
}

//Device not available
void exception_DeviceNotAvailable(ihs_t *ihs)
{}

//Double Fault
void exception_DoubleFault(ihs_t *ihs)
{
	static char Ausgabe[20];
	Display_Clear();
	setColor(BG_BLACK | CL_RED);
	printf("Exception 8: Double Fault\n\r");
	traceRegisters(ihs);
	printf("Stack-backtrace:\n");
	traceStack(ihs->rsp, 10);
	asm volatile("cli;hlt");
}

//-
void exception_CoprocessorSegmentOverrun(ihs_t *ihs)
{}

//Invalid TSS
void exception_InvalidTSS(ihs_t *ihs)
{}

//Segment not present
void exception_SegmentNotPresent(ihs_t *ihs)
{}

//Stack fault
void exception_StackFault(ihs_t *ihs)
{}

//General protection
void exception_GeneralProtection(ihs_t *ihs)
{
	Display_Clear();
	setColor(BG_BLACK | CL_RED);
	printf("Exception 13: General Protection\n");
	setColor(BG_BLACK | CL_WHITE);
	printf("Errorcode: 0x%X%X\n", ihs->error >> 32, ihs->error & 0xFFFFFFFF);
	traceRegisters(ihs);
	printf("Stack-backtrace:\n");
	traceStack(ihs->rsp, 10);
	asm volatile("cli;hlt");
}

//Page fault
void exception_PageFault(ihs_t *ihs)
{
	uint64_t CR2;
	Display_Clear();
	setColor(BG_BLACK | CL_RED);
	printf("Exception 14: Page Fault\n");
	setColor(BG_BLACK | CL_WHITE);
	printf("Errorcode: 0x%X%X\n", ihs->error >> 32, ihs->error & 0xFFFFFFFF);

	asm volatile("mov %%cr2,%0" : "=r"(CR2));
	printf("CR2: 0x%X%X\n", CR2 >> 32, CR2 & 0xFFFFFFFF);

	traceRegisters(ihs);
	printf("Stack-backtrace:\n");
	traceStack(ihs->rsp, 10);
	asm volatile("cli;hlt");
}

//x87 floating point
void exception_MF(ihs_t *ihs)
{}

//Alignement check
void exception_AlignmentCheck(ihs_t *ihs)
{}

//Machine check
void exception_MachineCheck(ihs_t *ihs)
{}

//SIMD floating point
void exception_XF(ihs_t *ihs)
{}

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
	for(i = 0; i < length; i++)
	{
		uint64_t *Value = (rsp + i * 8);
		printf("%u: 0x%X%X\n", i + 1, (*Value) >> 32, (*Value) & 0xFFFFFFFF);
	}
}

#endif
