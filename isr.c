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
#include "console.h"

typedef struct{
		void (*Handler)(ihs_t *ihs);
		void *Next;
}HandlerList_t;

typedef struct{
		HandlerList_t *Handlers;
		uint64_t numHandlers;
}irqHandlers;

extern void keyboard_Handler(ihs_t *ihs);
extern ihs_t *syscall_Handler(ihs_t *ihs);
extern ihs_t *pm_Schedule(ihs_t *ihs);
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
/*16*/			exception_MF,
/*17*/			exception_AlignmentCheck,
/*18*/			exception_MachineCheck,
/*19*/			exception_XF,
[32 ... 47]		irq_handler,
[48]			syscall_Handler,
[255]			pm_Schedule
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
	return interrupt_handlers[ihs->interrupt](ihs);
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
				new_ihs = pm_Schedule(ihs);
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
	Display_Clear();
	setColor(BG_BLACK | CL_RED);
	printf("Exception 6: Invalid Opcode\n");
	traceRegisters(ihs);
	printf("Stack-backtrace:\n");
	traceStack(ihs->rsp, (uint64_t*)ihs->rbp, 22);
	asm volatile("cli;hlt");
	return NULL;
}

//Device not available
static ihs_t *exception_DeviceNotAvailable(ihs_t *ihs)
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
	return ihs;
}

//Double Fault
static ihs_t *exception_DoubleFault(ihs_t *ihs)
{
	Display_Clear();
	setColor(BG_BLACK | CL_RED);
	printf("Exception 8: Double Fault\n\r");
	traceRegisters(ihs);
	printf("Stack-backtrace:\n");
	traceStack(ihs->rsp, (uint64_t*)ihs->rbp, 22);
	asm volatile("cli;hlt");
	return NULL;
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
	console_switch(0);
	printf("\e[31mException 13: General Protection\e[37m\n");
	printf("Errorcode: 0x%X%X\n", ihs->error >> 32, ihs->error & 0xFFFFFFFF);
	traceRegisters(ihs);
	printf("Stack-backtrace:\n");
	traceStack(ihs->rsp, (uint64_t*)ihs->rbp, 22);
	asm volatile("cli;hlt");
	return NULL;
}

//Page fault
static ihs_t *exception_PageFault(ihs_t *ihs)
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
	if(!vmm_getPageStatus((void*)CR2) && (PG_AVL(PT->PTE[PTi]) & VMM_UNUSED_PAGE))
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
		traceStack(ihs->rsp, (uint64_t*)ihs->rbp, 22);
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
		return NULL;
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

#endif
