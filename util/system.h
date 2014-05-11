/*
 * system.h
 *
 *  Created on: 11.08.2012
 *      Author: pascal
 */

#ifndef SYSTEM_H_
#define SYSTEM_H_

#include "stdint.h"

typedef struct{
		uint64_t	physSpeicher;
		uint64_t	physFree;
}SIS;	//"SIS" steht für "System Information Structure"

void getSystemInformation(SIS *Struktur);

#endif /* SYSTEM_H_ */
