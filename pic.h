/*
 * pic.h
 *
 *  Created on: 14.07.2012
 *      Author: pascal
 */

#ifndef PIC_H_
#define PIC_H_

#include "stdint.h"

void pic_Init(void);
void pic_Remap(uint8_t Interrupt);
void pic_MaskIRQ(uint16_t Mask);
void pic_SendEOI(uint8_t irq);

#endif /* PIC_H_ */
