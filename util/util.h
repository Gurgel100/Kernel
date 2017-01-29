/*
 * util.h
 *
 *  Created on: 14.07.2012
 *      Author: pascal
 */

#ifndef UTIL_H_
#define UTIL_H_

#include "stdint.h"

void outb(uint16_t Port, uint8_t Data);
uint8_t inb(uint16_t Port);

void outw(uint16_t Port, uint16_t Data);
uint16_t inw(uint16_t Port);

void outd(uint16_t Port, uint32_t Data);
uint32_t ind(uint16_t Port);

void outq(uint16_t Port, uint64_t Data);
uint64_t inq(uint16_t Port);

void IntToHex(uint64_t Value, char *Destination);

void Sleep(uint64_t msec);

#endif /* UTIL_H_ */
