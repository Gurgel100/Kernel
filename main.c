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
#include "version.h"
#include "loader.h"
#include "stdio.h"
#include "stdlib.h"
#include "scheduler.h"

static multiboot_structure static_MBS;

void Init(void);

void __attribute__((noreturn)) main(void *mbsAdresse)
{
	MBS = mbsAdresse;
	Init();		//Initialisiere System
	if(MBS->mbs_flags & 0x1)
			printf("Bootdevice: %X\n", MBS->mbs_bootdevice);
	sound_Play(10000, 1000);
	printf("Kernel version: %i.%i.%i - %s %s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_BUGFIX, BUILD_DATE, BUILD_TIME);
	if(vfs_MountRoot())
		SysLogError("KERNEL", "Could not find root directory\n");
	else
	{
		loader_load("/mount/0/bin", "init", "/dev/tty01", "/dev/tty01", "/dev/tty01");
		scheduler_activate();
	}

	//Der Kernel wird durch ein Interrupt aufgeweckt
	while(1) asm volatile("hlt;");	//Wir wollen diese Funktion nicht verlassen
}

void Init()
{
	//MBS zwischenspeichern, bis Speicherverwaltung initialisiert ist, wenn nötig noch andere Strukturen sichern
	MBS = memcpy(&static_MBS, MBS, sizeof(multiboot_structure));
	MBS->mbs_mmap_addr = memcpy(__builtin_alloca(MBS->mbs_mmap_length), MBS->mbs_mmap_addr, MBS->mbs_mmap_length);

	Display_Init();		//Anzeige Intialisieren
	cpu_Init();			//CPU Initialisieren
	fpu_Init();			//FPU Initialisieren
	GDT_Init();			//GDT initialisieren
	IDT_Init();			//IDT initialisieren
	TSS_Init();			//TSS initialisieren
	pit_Init(1000);		//PIT initialisieren mit 1kHz
	pic_Init();			//PIC initialisieren
	cmos_Init();		//CMOS initialisieren
	syscall_Init();		//Syscall initialisieren
	#ifdef DEBUGMODE
	Debug_Init();
	#endif
	if(!mm_Init())		//Speicherverwaltung initialisieren
		SysLogError("MM", "Fehler beim Initialisieren der Speicherverwaltung");
	#ifdef DEBUGMODE
	asm volatile("int $0x1");
	#endif
	//isr_Init();
	keyboard_Init();	//Tastatur(treiber) initialisieren
	apic_Init();
	vfs_Init();			//VFS initialisieren
	pci_Init();			//PCI-Treiber initialisieren
	dmng_Init();
	pm_Init();			//Tasks initialisieren
	console_Init();

	//MBS an einen richtigen Ort sichern
	MBS->mbs_mmap_addr = memcpy(malloc(MBS->mbs_mmap_length), MBS->mbs_mmap_addr, MBS->mbs_mmap_length);

	SysLog("SYSTEM", "Initialisierung abgeschlossen");
	setColor(BG_BLACK | CL_WHITE);
	#ifdef DEBUGMODE
	printf("Aktiviere Interrupts\n\r");
	#endif
	asm volatile("sti");	//Interrupts aktivieren
	cdi_init();			//CDI und -Treiber initialisieren
}

#endif
