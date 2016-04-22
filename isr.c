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
#include "vmm.h"
#include "paging.h"
#include "thread.h"
#include "cpu.h"

typedef struct{
		void (*Handler)(ihs_t *ihs);
		void *Next;
}HandlerList_t;

typedef struct{
		HandlerList_t *Handlers;
		uint64_t numHandlers;
}irqHandlers;

static irqHandlers *Handlers[NUM_IRQ];

thread_t *fpuThread = NULL;
extern thread_t *currentThread;

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
			{
				static uint64_t nextSchedule = 50;
				pit_Handler();
				if(Uptime == nextSchedule)
				{
					new_ihs = pm_Schedule(ihs);
					nextSchedule += 50;				//Alle 50ms wird der Task gewechselt
				}
			}
			break;
			case 33:
				keyboard_Handler(ihs);
			break;
		}
		pic_SendEOI(ihs->interrupt - 32);		//PIC sagen, dass IRQ behandelt wurde
	}
	else
	{
		switch(ihs->interrupt)
		{
			case 48:
				new_ihs = syscall_Handler(ihs);
			break;
			case 255:
				new_ihs = pm_Schedule(ihs);
		}
	}
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
			oldState->rflags |= 0x10100;
		if(ihs->rax == DEBUG_CONTINUE)
			oldState->rflags = oldState->rflags & ~0x100 | 0x10000;
		Debugged = false;
		return oldState;
	}
	#else
	return ihs;
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
	traceStack(ihs->rsp, 22);
	asm volatile("cli;hlt");
}

//Device not available
void exception_DeviceNotAvailable(ihs_t *ihs)
{
	//XXX: Because malloc does not align to 16-byte boundary we have to do it manually
	//Reset TS-Flag
	asm volatile("clts");

	if(cpuInfo.fxsr)
	{
		//Speichere aktuellen FPU Status, wenn ein Prozess gelaufen ist
		if(fpuThread != NULL)
			asm volatile("fxsave (%0)": :"r"((((uintptr_t)fpuThread->fpuState) + 15) & ~0xF));

		fpuThread = currentThread;

		//FPU Status laden
		if(fpuThread->fpuState == NULL)
		{
			//Speicher reservieren, um Status speichern zu können
			fpuThread->fpuState = malloc(512 + 16);
		}
		else
		{
			asm volatile("fxrstor (%0)": :"r"((((uintptr_t)fpuThread->fpuState) + 15) & ~0xF));
		}
	}
}

//Double Fault
void exception_DoubleFault(ihs_t *ihs)
{
	static char Ausgabe[20];
	Display_Clear();
	setColor(BG_BLACK | CL_RED);
	printf("Exception 8: Double Fault\n\r");
	traceRegisters(ihs);
	printf("Stack-backtrace:\n");
	traceStack(ihs->rsp, 22);
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
	console_switch(0);
	printf("\e[31mException 13: General Protection\e[37m\n");
	printf("Errorcode: 0x%X%X\n", ihs->error >> 32, ihs->error & 0xFFFFFFFF);
	traceRegisters(ihs);
	printf("Stack-backtrace:\n");
	traceStack(ihs->rsp, 22);
	asm volatile("cli;hlt");
}

//Page fault
void exception_PageFault(ihs_t *ihs)
{
	//Virtuelle Adressen der Tabellen
	#define VMM_PML4_ADDRESS		0xFFFFFFFFFFFFF000
	#define VMM_PDP_ADDRESS			0xFFFFFFFFFFE00000
	#define VMM_PD_ADDRESS			0xFFFFFFFFC0000000
	#define VMM_PT_ADDRESS			0xFFFFFF8000000000

	PML4_t *PML4 = (PML4_t*)VMM_PML4_ADDRESS;
	PDP_t *PDP = (PDP_t*)VMM_PDP_ADDRESS;
	PD_t *PD = (PD_t*)VMM_PD_ADDRESS;
	PT_t *PT = (PT_t*)VMM_PT_ADDRESS;

	uint64_t CR2;
	asm volatile("mov %%cr2,%0" : "=r"(CR2));

	//Einträge in die Page Tabellen
	uint16_t PML4i = (CR2 & PG_PML4_INDEX) >> 39;
	uint16_t PDPi = (CR2 & PG_PDP_INDEX) >> 30;
	uint16_t PDi = (CR2 & PG_PD_INDEX) >> 21;
	uint16_t PTi = (CR2 & PG_PT_INDEX) >> 12;

	PDP = (void*)PDP + (PML4i << 12);
	PD = (void*)PD + ((PML4i << 21) | (PDPi << 12));
	PT = (void*)PT + (((uint64_t)PML4i << 30) | (PDPi << 21) | (PDi << 12));

	//Wenn diese Page eine unused page ist, dann wird diese aktiviert
	if(!vmm_getPageStatus(CR2) && (PG_AVL(PT->PTE[PTi]) & VMM_UNUSED_PAGE))
	{
		vmm_usePages((void*)CR2, 1);
	}
	else
	{
		console_switch(0);
		printf("\e[31mException 14: Page Fault\e[37m  ");
		if(currentProcess != NULL && currentThread != NULL)
		{
			printf("PID: %lu, TID: %lu ", currentProcess->PID, currentThread->tid);
		}
		printf("\nErrorcode: 0x%X%X\n", ihs->error >> 32, ihs->error & 0xFFFFFFFF);


		printf("CR2: 0x%X%X\n", CR2 >> 32, CR2 & 0xFFFFFFFF);

		traceRegisters(ihs);
		printf("Stack-backtrace:\n");
		traceStack(ihs->rsp, 20);
		printf("PML4e: 0x%lX             ", PML4->PML4E[PML4i]);
		if(PML4->PML4E[PML4i] & 1)
		{
			printf("PDPe: 0x%lX\n", PDP->PDPE[PDPi]);
			if(PDP->PDPE[PDPi] & 1)
			{
				printf("PDe:   0x%lX             ", PD->PDE[PDi]);
				if(PD->PDE[PDi] & 1)
					printf("PTe:  0x%lX", PT->PTE[PTi]);
			}
		}
		asm volatile("cli;hlt");
	}
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

{
}

{
}

#endif
