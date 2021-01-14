/*
 * isr.c
 *
 *  Created on: 15.07.2012
 *      Author: pascal
 */

#include "isr.h"
#include "config.h"
#include "display.h"
#include "stdio.h"
#include "stdlib.h"
#include "util.h"
#include "pic.h"
#include "debug.h"
#include "pit.h"
#include "vmm.h"
#include "paging.h"
#include "thread.h"
#include "cpu.h"
#include "system.h"
#include "scheduler.h"
#include <dispatcher.h>

typedef struct{
		void (*Handler)(ihs_t *ihs);
		void *Next;
}HandlerList_t;

typedef struct{
		HandlerList_t *Handlers;
		uint64_t numHandlers;
}irqHandlers;

extern void keyboard_Handler(ihs_t *ihs);
extern void cdi_irq_handler(uint8_t irq);
extern void pit_Handler(void);

static ihs_t *irq_handler(ihs_t *ihs);
static ihs_t *exception_DivideByZero(ihs_t *ihs);
static ihs_t *exception_Debug(ihs_t *ihs);
static ihs_t *exception_NonMaskableInterrupt(ihs_t *ihs);
static ihs_t *exception_BreakPoint(ihs_t *ihs);
static ihs_t *exception_Overflow(ihs_t *ihs);
static ihs_t *exception_BoundRange(ihs_t *ihs);
static ihs_t *exception_InvalidOpcode(ihs_t *ihs);
static ihs_t *exception_DeviceNotAvailable(ihs_t *ihs);
static ihs_t *exception_DoubleFault(ihs_t *ihs);
static ihs_t *exception_CoprocessorSegmentOverrun(ihs_t *ihs);
static ihs_t *exception_InvalidTSS(ihs_t *ihs);
static ihs_t *exception_SegmentNotPresent(ihs_t *ihs);
static ihs_t *exception_StackFault(ihs_t *ihs);
static ihs_t *exception_GeneralProtection(ihs_t *ihs);
static ihs_t *exception_PageFault(ihs_t *ihs);
static ihs_t *exception_MF(ihs_t *ihs);
static ihs_t *exception_AlignmentCheck(ihs_t *ihs);
static ihs_t *exception_MachineCheck(ihs_t *ihs);
static ihs_t *exception_XF(ihs_t *ihs);
static ihs_t *nop(ihs_t *ihs);

static irqHandlers *Handlers[NUM_IRQ];

thread_t *fpuThread = NULL;
extern thread_t *currentThread;

static interrupt_handler interrupt_handlers[NUM_INTERRUPTS] = {
/* 0*/			exception_DivideByZero,
/* 1*/			exception_Debug,
/* 2*/			exception_NonMaskableInterrupt,
/* 3*/			exception_BreakPoint,
/* 4*/			exception_Overflow,
/* 5*/			exception_BoundRange,
/* 6*/			exception_InvalidOpcode,
/* 7*/			exception_DeviceNotAvailable,
/* 8*/			exception_DoubleFault,
/* 9*/			exception_CoprocessorSegmentOverrun,
/*10*/			exception_InvalidTSS,
/*11*/			exception_SegmentNotPresent,
/*12*/			exception_StackFault,
/*13*/			exception_GeneralProtection,
/*14*/			exception_PageFault,
[16]			exception_MF,
[17]			exception_AlignmentCheck,
[18]			exception_MachineCheck,
[19]			exception_XF,
[32 ... 47]		irq_handler,
[255]			scheduler_schedule
};

//Test
uint64_t Counter;

void isr_Init()
{
	uint16_t i;
	for(i = 0; i < NUM_IRQ; i++)
		Handlers[i] = malloc(sizeof(irqHandlers));
	//Setze alle nicht besetzten Interrupt-Handler auf den Nop-Handler
	for(i = 0; i < NUM_INTERRUPTS; i++)
	{
		if(interrupt_handlers[i] == NULL)
			interrupt_handlers[i] = nop;
	}
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

interrupt_handler isr_setHandler(uint8_t num, interrupt_handler handler)
{
	interrupt_handler old = interrupt_handlers[num];
	interrupt_handlers[num] = handler ? : nop;
	return old;
}

ihs_t *isr_Handler(ihs_t *ihs)
{
	Counter++;
	return dispatcher_dispatch(interrupt_handlers[ihs->interrupt](ihs));
}

static ihs_t *irq_handler(ihs_t *ihs)
{
	ihs_t *new_ihs = ihs;

	uint8_t irq = ihs->interrupt - 32;
	switch(ihs->interrupt)
	{
		case 32:
		{
			static uint64_t nextSchedule = 50;
			pit_Handler();
			if(Uptime == nextSchedule)
			{
				new_ihs = scheduler_schedule(ihs);
				nextSchedule += 50;				//Alle 50ms wird der Task gewechselt
			}
		}
		break;
		case 33:
			keyboard_Handler(ihs);
		break;
	}

	cdi_irq_handler(irq);
	pic_SendEOI(irq);		//PIC sagen, dass IRQ behandelt wurde

	return new_ihs;
}

//Divide by Zero
static ihs_t *exception_DivideByZero(ihs_t *ihs)
{
	return ihs;
}

//Debug
static ihs_t *exception_Debug(ihs_t *ihs)
{
	return ihs;
}

//non maskable interrupt
static ihs_t *exception_NonMaskableInterrupt(ihs_t *ihs)
{
	return ihs;
}

//Breakpoint
static ihs_t *exception_BreakPoint(ihs_t *ihs)
{
	return ihs;
}

//Overflow
static ihs_t *exception_Overflow(ihs_t *ihs)
{
	return ihs;
}

//Bound Range
static ihs_t *exception_BoundRange(ihs_t *ihs)
{
	return ihs;
}

//Invalid opcode
static ihs_t *exception_InvalidOpcode(ihs_t *ihs)
{
	system_panic_enter();
	uint64_t offset = 0;
	offset += sprintf(system_panic_buffer, "Exception 6: Invalid Opcode\n");
	offset += traceRegistersToString(ihs, system_panic_buffer + offset);
	offset += sprintf(system_panic_buffer + offset, "Stack-backtrace:\n");
	traceStackToString(ihs->rsp, (uint64_t*)ihs->rbp, 22, system_panic_buffer + offset);
	system_panic();
}

//Device not available
static ihs_t *exception_DeviceNotAvailable(ihs_t *ihs)
{
	//Reset TS-Flag
	asm volatile("clts");

	if(fpuThread != NULL)
		cpu_saveXState(fpuThread->fpuState);

	fpuThread = currentThread;

	cpu_restoreXState(fpuThread->fpuState);

	if(!currentThread->fpuInitialised)
	{
		const uint32_t initMXCSR = 0x1F80;
		asm volatile("fninit;"
					"ldmxcsr %0":: "m"(initMXCSR));
		currentThread->fpuInitialised = true;
	}

	return ihs;
}

//Double Fault
static ihs_t *exception_DoubleFault(ihs_t *ihs)
{
	system_panic_enter();
	uint64_t offset = 0;
	offset += sprintf(system_panic_buffer, "Exception 8: Double Fault\n");
	offset += traceRegistersToString(ihs, system_panic_buffer + offset);
	offset += sprintf(system_panic_buffer + offset, "Stack-backtrace:\n");
	traceStackToString(ihs->rsp, (uint64_t*)ihs->rbp, 22, system_panic_buffer + offset);
	system_panic();
}

//-
static ihs_t *exception_CoprocessorSegmentOverrun(ihs_t *ihs)
{
	return ihs;
}

//Invalid TSS
static ihs_t *exception_InvalidTSS(ihs_t *ihs)
{
	return ihs;
}

//Segment not present
static ihs_t *exception_SegmentNotPresent(ihs_t *ihs)
{
	return ihs;
}

//Stack fault
static ihs_t *exception_StackFault(ihs_t *ihs)
{
	return ihs;
}

//General protection
static ihs_t *exception_GeneralProtection(ihs_t *ihs)
{
	system_panic_enter();
	uint64_t offset = 0;
	offset += sprintf(system_panic_buffer, "Exception 13: General Protection\n");
	offset += sprintf(system_panic_buffer + offset, "Errorcode: 0x%lX\n", ihs->error);
	offset += traceRegistersToString(ihs, system_panic_buffer + offset);
	offset += sprintf(system_panic_buffer + offset, "Stack-backtrace:\n");
	traceStackToString(ihs->rsp, (uint64_t*)ihs->rbp, 22, system_panic_buffer + offset);
	system_panic();
}

//Page fault
static ihs_t *exception_PageFault(ihs_t *ihs)
{
	void *address;
	asm volatile("mov %%cr2,%0" : "=r"(address));

	//Try to handle page fault else panic
	if(!vmm_handlePageFault(address, ihs->error))
	{
#ifndef DEBUGMODE
		system_panic_enter();
		uint64_t offset = 0;
		offset += sprintf(system_panic_buffer, "Exception 14: Page Fault  ");
		offset += sprintf(system_panic_buffer + offset, "\nErrorcode: 0x%lX\n", ihs->error);

		offset += sprintf(system_panic_buffer + offset, "CR2: 0x%lX\n", address);

		offset += traceRegistersToString(ihs, system_panic_buffer + offset);
		offset += sprintf(system_panic_buffer + offset, "Stack-backtrace:\n");
		offset += traceStackToString(ihs->rsp, (uint64_t*)ihs->rbp, 22, system_panic_buffer + offset);
		system_panic();
#else
		return Debug_Handler(ihs);
#endif
	}
	return ihs;
}

//x87 floating point
static ihs_t *exception_MF(ihs_t *ihs)
{
	return ihs;
}

//Alignement check
static ihs_t *exception_AlignmentCheck(ihs_t *ihs)
{
	return ihs;
}

//Machine check
static ihs_t *exception_MachineCheck(ihs_t *ihs)
{
	return ihs;
}

//SIMD floating point
static ihs_t *exception_XF(ihs_t *ihs)
{
	return ihs;
}

//Nop handler
static ihs_t *nop(ihs_t *ihs)
{
	asm volatile("nop");
	return ihs;
}
