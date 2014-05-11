/*
 * userlib.h
 *
 *  Created on: 11.08.2012
 *      Author: pascal
 */

#ifndef USERLIB_H_
#define USERLIB_H_

#include "stdint.h"

typedef struct{
		uint64_t	physSpeicher;
		uint64_t	physFree;
}SIS;	//"SIS" steht für "System Information Structure"

void initLib(void);

void getSysInfo(SIS *Struktur);

void reverse(char *s);

#endif /* USERLIB_H_ */
