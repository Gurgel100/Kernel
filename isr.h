/*
 * isr.h
 *
 *  Created on: 15.07.2012
 *      Author: pascal
 */

#ifndef ISR_H_
#define ISR_H_

#include "stdint.h"

#define NUM_IRQ	16
#define NUM_INTERRUPTS 256

typedef struct{
		uint64_t gs, fs, es, ds;
		uint64_t rsi, rdi, rbp;
		uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
		uint64_t rdx, rcx, rbx, rax;
		uint64_t interrupt, error;
		uint64_t rip, cs, rflags, rsp, ss;
}__attribute__((packed)) ihs_t;	//Interrupt-Handler Stack

typedef ihs_t*(*interrupt_handler)(ihs_t *ihs);

void isr_Init(void);
void isr_RegisterIRQHandler(uint16_t irq, void *Handler);

interrupt_handler isr_setHandler(uint8_t num, interrupt_handler handler);
ihs_t *isr_Handler(ihs_t *ihs);

#endif /* ISR_H_ */
