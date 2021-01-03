/*
 * pit.h
 *
 *  Created on: 16.03.2014
 *      Author: pascal
 */

#ifndef PIT_H_
#define PIT_H_

#include "stdint.h"
#include "thread.h"

volatile uint64_t Uptime;						//Zeit in Milisekunden seit dem Starten des Computers

void pit_Init(uint32_t freq);
void pit_RegisterTimer(thread_t *thread, uint64_t msec);
void pit_InitChannel(uint8_t channel, uint8_t mode, uint16_t data);

#endif /* PIT_H_ */
