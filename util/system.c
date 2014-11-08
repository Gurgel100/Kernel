/*
 * system.c
 *
 *  Created on: 11.08.2012
 *      Author: pascal
 */

#include "system.h"

extern uint64_t pmm_Speicher;
extern uint64_t pmm_Speicher_Verfuegbar;
extern uint64_t Uptime;
/*
 * Speichert Systeminformationen in die übergebene Struktur
 * Parameter:	Adresse auf die Systeminformationen-Struktur
 */
void getSystemInformation(SIS *Struktur)
{
	Struktur->physSpeicher = pmm_Speicher;
	Struktur->physFree = pmm_Speicher_Verfuegbar * 4096;
	Struktur->Uptime = Uptime;
}
