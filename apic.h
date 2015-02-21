/*
 * apic.h
 *
 *  Created on: 18.02.2015
 *      Author: pascal
 */

#ifndef APIC_H_
#define APIC_H_

#include "stdbool.h"
#include "stdint.h"

void apic_Init();
bool apic_available();
uint32_t apic_Read(uintptr_t offset);
void apic_Write(uintptr_t offset, uint32_t value);

#endif /* APIC_H_ */
