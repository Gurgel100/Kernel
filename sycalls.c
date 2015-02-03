/*
 * sycalls.c
 *
 *  Created on: 17.07.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "syscalls.h"
#include "isr.h"
#include "cmos.h"
#include "pm.h"
#include "vfs.h"
#include "loader.h"
#include "pit.h"
#include "system.h"
#include "cpu.h"

#define STAR	0xC0000081
#define LSTAR	0xC0000082
#define CSTAR	0xC0000083
#define SFMASK	0xC0000084

extern void putch(char);
extern char getch(void);
//extern uintptr_t mm_SysAlloc(uint64_t);
//extern void mm_SysFree(uintptr_t, uint64_t);
extern void setColor(uint8_t);
extern void setCursor(uint16_t, uint16_t);
extern void isr_syscall();

void syscall_Init()
{
	//Prüfen, ob syscall/sysret unterstützt wird
	if(cpuInfo.syscall)
	{
		cpu_MSRwrite(STAR, (0x8ul << 32) | (0x13ul << 48));	//Segementregister
		cpu_MSRwrite(LSTAR, (uintptr_t)isr_syscall);		//Einsprungspunkt
		cpu_MSRwrite(SFMASK, 0);							//Wir setzen keine Bits zurück (Interrupts bleiben auch aktiviert)

		//Syscall-Instruktion aktivieren (ansonsten #UD)
		//Bit 0
		cpu_MSRwrite(0xC0000080, cpu_MSRread(0xC0000080) | 1);
	}
}

//TODO: Nicht alles doppelt haben

uint64_t syscall_syscallHandler(uint64_t func, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	switch(func)
	{
		//Parameter: 	rbx = Grösse
		//Rückgabewert:	rax = Adresse
		case MALLOC:
			return mm_Alloc(arg1);
		break;

		//Parameter: 	rbx = Adresse, rcx = Grösse
		case FREE:
			mm_Free(arg1, arg2);
		break;

		//Parameter: 	rbx = Adresse, rcx = Grösse
		case UNUSE:
			vmm_unusePages((void*)arg1, arg2);
		break;

		//Rückgabewert:	al = ASCII-Zeichen
		case GETCH:
			return (uint64_t)getch();
		break;

		//Parameter: 	bl = ASCII-Zeichen
		case PUTCH:
			putch((char)(arg1 & 0xFF));
		break;

		//Parameter: 	ebx = Farbwert
		case COLOR:
			setColor((uint8_t)(arg1 & 0xFFFF));
		break;

		//Parameter:	bx = X, cx = Y
		case CURSOR:
			setCursor(arg1 & 0xFFFF, arg2 & 0xFFFF);
		break;

		//Parameter:	rbx = Pfad, rcx = Commandozeile
		//Rückgabewert: rax = PID des neuen Prozesses
		case EXEC:
			return loader_load((const char*)arg1, (const char*)arg2);
		break;

		//Parameter:	rbx = Pfad, rcx = Addresse zur Modusstruktur
		//Rückgabewert:	rax = Pointer zur Systemdateistruktur
		case FOPEN:
			return (uintptr_t)vfs_Open((char*)arg1, *((vfs_mode_t*)arg2));
		break;

		//Parameter:	rbx = Pointer zur Systemdateistruktur
		case FCLOSE:
			vfs_Close((vfs_stream_t*)arg1);
		break;

		//Parameter:	rbx = Systemdateistruktur, rcx = Start, rdx = Länge, rdi = Buffer
		//Rückgabewert:	rax = Bytes, die erfolgreich gelesen wurden
		case FREAD:
			return vfs_Read((vfs_stream_t*)arg1, arg2, arg3, (void*)arg4);
		break;

		//Parameter:	rbx = Systemdateistruktur, rcx = Start, rdx = Länge, rdi = Buffer
		//Rückgabewert:	rax = Bytes, die erfolgreich geschrieben wurden
		case FWRITE:
			return vfs_Write((vfs_stream_t*)arg1, arg2, arg3, (void*)arg4);
		break;

		//Parameter:	rbx = Systemdateistruktur, rcx = Informationstyp
		//Rückgabewert:	rax = Wert für den entsprechenden Informationstyp
		case FINFO:
			return vfs_getFileinfo((vfs_stream_t*)arg1, arg2);
		break;

		//Parameter:	rbx = Adresse
		//Rückgabewert:	rax = Adresse
		case TIME:
			return (uint64_t)cmos_GetTime((Time_t*)arg1);
		break;

		//Parameter:	rbx = Adresse
		//Rückgabewert:	rax = Adresse
		case DATE:
			return (uint64_t)cmos_GetDate((Date_t*)arg1);
		break;

		//Parameter:	rbx = Milisekunden
		case SLEEP:
			pit_RegisterTimer(currentProcess->PID, arg1);
		break;

		//Rückgabewert:	rbx = Adresse zur Informationsstruktur
		case SYSINF:
			getSystemInformation((SIS*)arg1);
		break;
	}
	return 0;
}

/*
 * Funktionsnummer wird im Register rax übergeben
 */
ihs_t *syscall_Handler(ihs_t *ihs)
{
	switch(ihs->rdi)
	{
		//Parameter: 	rsi = Grösse
		//Rückgabewert:	rax = Adresse
		case MALLOC:
			ihs->rax = mm_Alloc(ihs->rsi);
		break;

		//Parameter: 	rsi = Adresse, rdx = Grösse
		case FREE:
			mm_Free(ihs->rsi, ihs->rdx);
		break;

		//Parameter: 	rsi = Adresse, rdx = Grösse
		case UNUSE:
			vmm_unusePages((void*)ihs->rsi, ihs->rdx);
		break;

		//Rückgabewert:	al = ASCII-Zeichen
		case GETCH:
			ihs->rax = (uint64_t)getch();
		break;

		//Parameter: 	rsi = ASCII-Zeichen
		case PUTCH:
			putch((char)(ihs->rsi & 0xFF));
		break;

		//Parameter: 	rsi = Farbwert
		case COLOR:
			setColor((uint8_t)(ihs->rsi & 0xFFFF));
		break;

		//Parameter:	rsi = X, dx = Y
		case CURSOR:
			setCursor(ihs->rsi & 0xFFFF, ihs->rdx & 0xFFFF);
		break;

		//Parameter:	rsi = Pfad, rdx = Commandozeile
		//Rückgabewert: rax = PID des neuen Prozesses
		case EXEC:
			ihs->rax = loader_load((const char*)ihs->rsi, (const char*)ihs->rdx);
		break;

		//Parameter:	rsi = Rückgabecode
		case EXIT:
			ihs = pm_ExitTask(ihs, ihs->rbx);
		break;

		//Parameter:	rsi = Pfad, rdx = Addresse zur Modusstruktur
		//Rückgabewert:	rax = Pointer zur Systemdateistruktur
		case FOPEN:
			ihs->rax = (uintptr_t)vfs_Open((char*)ihs->rsi, *((vfs_mode_t*)ihs->rdx));
		break;

		//Parameter:	rsi = Pointer zur Systemdateistruktur
		case FCLOSE:
			vfs_Close((vfs_stream_t*)ihs->rsi);
		break;

		//Parameter:	rsi = Systemdateistruktur, rdx = Start, rcx = Länge, r8 = Buffer
		//Rückgabewert:	rax = Bytes, die erfolgreich gelesen wurden
		case FREAD:
			ihs->rax = vfs_Read((vfs_stream_t*)ihs->rsi, ihs->rdx, ihs->rcx, (void*)ihs->r8);
		break;

		//Parameter:	rsi = Systemdateistruktur, rdx = Start, rcx = Länge, r8 = Buffer
		//Rückgabewert:	rax = Bytes, die erfolgreich geschrieben wurden
		case FWRITE:
			ihs->rax = vfs_Write((vfs_stream_t*)ihs->rsi, ihs->rdx, ihs->rcx, (void*)ihs->r8);
		break;

		//Parameter:	rsi = Systemdateistruktur, rdx = Informationstyp
		//Rückgabewert:	rax = Wert für den entsprechenden Informationstyp
		case FINFO:
			ihs->rax = vfs_getFileinfo((vfs_stream_t*)ihs->rsi, ihs->rdx);
		break;

		//Parameter:	rsi = Adresse
		//Rückgabewert:	rax = Adresse
		case TIME:
			ihs->rax = (uint64_t)cmos_GetTime((Time_t*)ihs->rsi);
		break;

		//Parameter:	rsi = Adresse
		//Rückgabewert:	rax = Adresse
		case DATE:
			ihs->rax = (uint64_t)cmos_GetDate((Date_t*)ihs->rsi);
		break;

		//Parameter:	rsi = Milisekunden
		case SLEEP:
			pit_RegisterTimer(currentProcess->PID, ihs->rsi);
		break;

		//Rückgabewert:	rsi = Adresse zur Informationsstruktur
		case SYSINF:
			getSystemInformation((SIS*)ihs->rsi);
		break;
	}
	return ihs;
}

#endif
