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
#include "loader.h"
#include "userlib.h"
#include "tss.h"
#include "stdlib.h"

#include "stdio.h"

static multiboot_structure mbs_main;

void Init(void);

void __attribute__((noreturn)) main(void *mbsAdresse)
{
	MBS = &mbs_main;
	memcpy(MBS, mbsAdresse, sizeof(multiboot_structure));
	initLib();	//Library initialisieren
	Init();		//Initialisiere System
	if(MBS->mbs_flags & 0x1)
			printf("Bootdevice: %X\n", MBS->mbs_bootdevice);
	sound_Play(10000, 1000);

	/*
	 * TEST
	 */
	void *tmp = malloc(7530);
	free(tmp);
	/*
	 * END TEST
	 */

	vfs_MountRoot();

	//Ausgeben, wieviel Stack gebraucht wurden
	extern char stack;
	uint64_t *i;
	for(i = (uint64_t*)(&stack - 0x10000); *i == 0xCCCCCCCCCCCCCCCC; i++);
	printf("Used stack: %lu Bytes\n", (uintptr_t)&stack - (uintptr_t)i);

	printf("%lu\n%.8u\n", ULONG_MAX, ULONG_MAX);


	/*printf("Loading first Task\n");

	FILE *fp = fopen("/mount/0/bin/init", "rb");
	if(fp == NULL)
		Panic("KERNEL", "Konnte Initialisierungprozess nicht laden");
	loader_load(fp);
	fclose(fp);
*/
	//TEST
	/*uint64_t start, end, i, sum = 0;
	void *test = NULL;
	for(i = 0; i < 1e6; i++)
	{
		asm volatile("rdtsc": "=a"(start));
		test = realloc(test, 64);
		asm volatile("rdtsc": "=a"(end));
		sum += end - start;
	}
	printf("Anzahl Takte: %u\nDurchschnitt: %u\n", sum, (uint64_t)(sum / 1e6));*/
	//ENDTEST

	volatile double test = atof("0x2.B7E151628AED2A6ABF7");
	if(test == 0x6957148B0ABF0540)
		printf("richtig!!!\n");

	//Der Kernel wird durch ein Interrupt aufgeweckt
	size_t counter = 0;
	while(1)
	{
		asm volatile("hlt;");	//Wir wollen diese Funktion nicht verlassen
		extern uint64_t pmm_Speicher_Verfuegbar;
		if(counter++ >= 10000)	//Alle 10 Sekunden anzeigen
		{
			printf("Free physical Pages: %u\n", pmm_Speicher_Verfuegbar);
			counter = 0;
		}
	}
}

void Init()
{
	Display_Init();		//Anzeige Intialisieren
	cpu_Init();			//CPU Initialisieren
	fpu_Init();			//FPU Initialisieren
	if(!mm_Init())		//Speicherverwaltung initialisieren
		SysLogError("MM", "Fehler beim Initialisieren der Speicherverwaltung");
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
