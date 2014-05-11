/*
 * fpu.c
 *
 *  Created on: 19.08.2012
 *      Author: pascal
 */

#include "fpu.h"
#include "display.h"

void fpu_Init()
{
	//FPU aktivieren
	/*
	 * Wenn die CPU Long Mode unterstützt wird mindestens auch SSE und SSE2 unterstützt
	 * Deshalb ist hier (noch) keine Kontrolle durchzuführen, ob SSE unterstützt wird.
	 */
	asm(
			"mov %cr0,%rax;"
			"and $0xFB,%al;"	//CR0.EM-Bit löschen
			"or $0x2,%al;"		//CR0.MP-Bit setzen
			"mov %rax,%cr0;"
			"mov %cr4,%rax;"
			"or $0x6,%ah;"		//CR4.OSFXSR- und CR4.OSXMMEXCPT-Bit setzen
			"mov %rax,%cr4;"
			"finit;"			//FPU initialisieren
	);
	SysLog("FPU", "Initialisierung abgeschlossen");
}
