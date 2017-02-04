/*
 * system.c
 *
 *  Created on: 11.08.2012
 *      Author: pascal
 */

#include "system.h"
#include "pmm.h"

extern uint64_t Uptime;
/*
 * Speichert Systeminformationen in die übergebene Struktur
 * Parameter:	Adresse auf die Systeminformationen-Struktur
 */
void getSystemInformation(SIS *Struktur)
{
	Struktur->physSpeicher = pmm_getTotalPages() * 4096;
	Struktur->physFree = pmm_getFreePages() * 4096;
	Struktur->Uptime = Uptime;
}
