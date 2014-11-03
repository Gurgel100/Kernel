/*
 * kernel.c
 *
 *  Created on: 27.06.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "main.h"
#include "config.h"
#include "multiboot.h"
#include "mm.h"
#include "gdt.h"
#include "idt.h"
#include "display.h"
#include "cpu.h"
#include "pic.h"
#include "keyboard.h"
#include "fpu.h"
#include "pm.h"
#include "debug.h"
#include "elf.h"
#include "cmos.h"
#include "lock.h"
#include "cdi.h"
#include "vfs.h"
#include "pit.h"
#include "sound.h"

#include "stdio.h"

void Init(void);

void __attribute__((noreturn)) main(void *mbsAdresse)
{
	MBS = mbsAdresse;
	initLib();	//Library initialisieren
	Init();		//Initialisiere System
	if(MBS->mbs_flags & 0x1)
			printf("Bootdevice: %X\n", MBS->mbs_bootdevice);
	sound_Play(10000, 1000);
	vfs_MountRoot();

	//Der Kernel wird durch ein Interrupt aufgeweckt
	while(1) asm volatile("hlt;");	//Wir wollen diese Funktion nicht verlassen
}

void Init()
{
	Display_Init();		//Anzeige Intialisieren
	cpu_Init();			//CPU Initialisieren
	fpu_Init();			//FPU Initialisieren
	GDT_Init();			//GDT initialisieren
	IDT_Init();			//IDT initialisieren
	TSS_Init();			//TSS initialisieren
	pit_Init(1000);		//PIT initialisieren mit 1kHz
	pic_Init();			//PIC initialisieren
	cmos_Init();		//CMOS initialisieren
	keyboard_Init();	//Tastatur(treiber) initialisieren
	#ifdef DEBUGMODE
	Debug_Init();
	#endif
	if(!mm_Init())		//Speicherverwaltung initialisieren
		SysLogError("MM", "Fehler beim Initialisieren der Speicherverwaltung");
	#ifdef DEBUGMODE
	asm volatile("int $0x1");
	#endif
	//isr_Init();
	vfs_Init();			//VFS initialisieren
	pci_Init();			//PCI-Treiber initialisieren
	dmng_Init();
	cdi_init();			//CDI und -Treiber initialisieren
	pm_Init();			//Tasks initialisieren
	SysLog("SYSTEM", "Initialisierung abgeschlossen");
	setColor(BG_BLACK | CL_WHITE);
	#ifdef DEBUGMODE
	printf("Aktiviere Interrupts\n\r");
	#endif
	asm volatile("sti");	//Interrupts aktivieren
}

#endif
