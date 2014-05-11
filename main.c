/*
 * kernel.c
 *
 *  Created on: 27.06.2012
 *      Author: pascal
 */

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

void Init(void);

/*static uint64_t StackA[1000];
static uint64_t StackB[1000];
static uint64_t StackC[1000];
void TaskA();
void TaskB();
void TaskC();*/
extern uint64_t Counter;

void main(void *mbsAdresse)
{
	MBS = mbsAdresse;
	initLib();	//Library initialisieren
	Init();		//Initialisiere System
	if(MBS->mbs_flags & 0x1)
			printf("Bootdevice: %X\n", MBS->mbs_bootdevice);
	sound_Play(10000, 1000);
	while(1)
	{
		printf("%u Interrupts pro Sekunde\r", Counter);
		Counter = 0;
		Sleep(1000);
	}
	cmos_Reboot();
	/*if(MBS->mbs_flags & 0x1)
		printf("Startgeraet: %X", MBS->mbs_bootdevice);*/

	//Test
	/*void *tmp[3];
	tmp[1] = malloc(0x1000 - 0x20);
	tmp[2] = (void*)vmm_SysAlloc(0, 1, true);
	tmp[3] = malloc(0x1500);
	free(tmp[1]);
	free(tmp[3]);*/
	//vfs_Read("./home/pascal/Dokumente/YourOS/Git/tyndur/src/modules/lib/stdlibc/math/fmod.c", StackC);
	//pm_InitTask(TaskA, StackA);
	//pm_InitTask(TaskB, StackB);
	//Ende Test
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
	cdi_init();			//CDI und -Treiber initialisieren
	pm_Init();			//Tasks initialisieren
	SysLog("SYSTEM", "Initialisierung abgeschlossen");
	setColor(BG_BLACK | CL_WHITE);
	#ifdef DEBUGMODE
	printf("Aktiviere Interrupts\n\r");
	#endif
	asm volatile("sti");	//Interrupts aktivieren
}

//Testbereich
/*lock_t prt;
void TaskA(void)
{
	while(true)
	{
		lock(&prt);
		printf("Task A: Ich gehe jetzt schlafen...\n");
		unlock(&prt);
		Sleep(10);
		lock(&prt);
		printf("Task A: So, bin wieder wach...\n");
		unlock(&prt);
		Sleep(6);
	}
}

void TaskB(uint64_t PID)
{
	while(true)
	{
		lock(&prt);
		printf("Task B: Ich gehe jetzt schlafen...\n");
		unlock(&prt);
		Sleep(4);
		lock(&prt);
		printf("Task B: So, bin wieder wach...\n");
		unlock(&prt);
		Sleep(8);
	}
}*/
