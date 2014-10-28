/*
 * isr.h
 *
 *  Created on: 15.07.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#ifndef ISR_H_
#define ISR_H_

#include "stdint.h"

#define NUM_IRQ	16

typedef struct{
		uint64_t gs, fs, es, ds;
		uint64_t rsi, rdi, rbp;
		uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
		uint64_t rdx, rcx, rbx, rax;
		uint64_t interrupt, error;
		uint64_t rip, cs, rflags, rsp, ss;
}__attribute__((packed)) ihs_t;	//Interrupt-Handler Stack

void isr_Init(void);
void isr_RegisterIRQHandler(uint16_t irq, void *Handler);

ihs_t *isr_Handler(ihs_t *ihs);
void exception_DivideByZero(ihs_t *ihs);
ihs_t *exception_Debug(ihs_t *ihs);
void exception_NonMaskableInterrupt(ihs_t *ihs);
void exception_BreakPoint(ihs_t *ihs);
void exception_Overflow(ihs_t *ihs);
void exception_BoundRange(ihs_t *ihs);
void exception_InvalidOpcode(ihs_t *ihs);
void exception_DeviceNotAvailable(ihs_t *ihs);
void exception_DoubleFault(ihs_t *ihs);
void exception_CoprocessorSegmentOverrun(ihs_t *ihs);
void exception_InvalidTSS(ihs_t *ihs);
void exception_SegmentNotPresent(ihs_t *ihs);
void exception_StackFault(ihs_t *ihs);
void exception_GeneralProtection(ihs_t *ihs);
void exception_PageFault(ihs_t *ihs);
void exception_MF(ihs_t *ihs);
void exception_AlignmentCheck(ihs_t *ihs);
void exception_MachineCheck(ihs_t *ihs);
void exception_XF(ihs_t *ihs);

#endif /* ISR_H_ */

#endif
