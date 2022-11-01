/*
 * kernel.c
 *
 *  Created on: 27.06.2012
 *      Author: pascal
 */

#include "config.h"
#include "multiboot.h"
#include "mm.h"
#include "gdt.h"
#include "idt.h"
#include "display.h"
#include "cpu.h"
#include "pic.h"
#include "keyboard.h"
#include "pm.h"
#include "debug.h"
#include "cmos.h"
#include "cdi.h"
#include "vfs.h"
#include "pit.h"
#include "sound.h"
#include "version.h"
#include "loader.h"
#include "stdio.h"
#include "scheduler.h"
#include "apic.h"
#include "tss.h"
#include "pci.h"
#include "drivermanager.h"
#include "devicemng.h"
#include "console.h"
#include "syscalls.h"
#include <dispatcher.h>

static multiboot_structure static_MBS;

multiboot_structure *Init(multiboot_structure *MBS);

void __attribute__((noreturn)) main(multiboot_structure *MBS)
{
	MBS = Init(MBS);		//Initialisiere System
	if(MBS->mbs_flags & 0x1)
			printf("Bootdevice: %X\n", MBS->mbs_bootdevice);

	printf("Kernel version   : %u.%u.%u\n", version_major, version_minor, version_bugfix);
	printf("Kernel commit    : %s %s\n", version_branch, version_commitId);
	printf("Kernel build date: %s %s\n", version_buildDate, version_buildTime);
	printf("Kernel compiler  : %s\n", version_compiler);
	if(vfs_MountRoot() || vfs_Mount("/dev", NULL, "devfs"))
		SysLogError("KERNEL", "Could not find root directory\n");
	else
	{
		const char **env = {NULL};
		loader_load("/bin", "init", env, "/dev/tty01", "/dev/tty01", "/dev/tty01");
		scheduler_activate();
	}

	//Der Kernel wird durch ein Interrupt aufgeweckt
	while(1) CPU_HALT();	//Wir wollen diese Funktion nicht verlassen
}

multiboot_structure *Init(multiboot_structure *MBS)
{
	Display_Init();		//Anzeige Intialisieren
	cpu_init(true);		//CPU Initialisieren
	GDT_Init();			//GDT initialisieren
	IDT_Init();			//IDT initialisieren
	TSS_Init();			//TSS initialisieren

	//MBS zwischenspeichern, bis Speicherverwaltung initialisiert ist, wenn nötig noch andere Strukturen sichern
	static_MBS = *MBS;
	MBS = &static_MBS;

	pit_Init(1000);		//PIT initialisieren mit 1kHz
	pic_Init();			//PIC initialisieren
	cmos_Init();		//CMOS initialisieren
	syscall_Init();		//Syscall initialisieren
	#ifdef DEBUGMODE
	Debug_Init();
	#endif
	if(!mm_Init((mmap*)(uintptr_t)MBS->mbs_mmap_addr, MBS->mbs_mmap_length))		//Speicherverwaltung initialisieren
		SysLogError("MM", "Fehler beim Initialisieren der Speicherverwaltung");
	#ifdef DEBUGMODE
	asm volatile("int $0x1");
	#endif
	//isr_Init();
	keyboard_Init();	//Tastatur(treiber) initialisieren
	apic_Init();
	vfs_Init();			//VFS initialisieren
	pci_Init();			//PCI-Treiber initialisieren
	drivermanager_init();
	dmng_Init();
	pm_Init();			//Tasks initialisieren
	console_Init();
	dispatcher_init(100);

	SysLog("SYSTEM", "Initialisierung abgeschlossen");
	setColor(BG_BLACK | CL_WHITE);
	#ifdef DEBUGMODE
	printf("Aktiviere Interrupts\n\r");
	#endif
	CPU_ENABLE_INTERRUPTS();
	cdi_init();			//CDI und -Treiber initialisieren
	return MBS;
}
