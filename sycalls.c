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

extern void putch(char);
extern char getch(void);
//extern uintptr_t mm_SysAlloc(uint64_t);
//extern void mm_SysFree(uintptr_t, uint64_t);
extern void setColor(uint8_t);
extern void setCursor(uint16_t, uint16_t);

/*
 * Funktionsnummer wird im Register rax übergeben
 */
ihs_t *syscall_Handler(ihs_t *ihs)
{
	switch(ihs->rax)
	{
		//Parameter: 	rbx = Grösse
		//Rückgabewert:	rax = Adresse
		case MALLOC:
			ihs->rax = mm_Alloc(ihs->rbx);
		break;

		//Parameter: 	rbx = Adresse, rcx = Grösse
		case FREE:
			mm_Free(ihs->rbx, ihs->rcx);
		break;

		//Parameter: 	rbx = Adresse, rcx = Grösse
		case UNUSE:
			vmm_unusePages((void*)ihs->rbx, ihs->rcx);
		break;

		//Rückgabewert:	al = ASCII-Zeichen
		case GETCH:
			ihs->rax = (uint64_t)getch();
		break;

		//Parameter: 	bl = ASCII-Zeichen
		case PUTCH:
			putch((char)(ihs->rbx & 0xFF));
		break;

		//Parameter: 	ebx = Farbwert
		case COLOR:
			setColor((uint8_t)(ihs->rbx & 0xFFFF));
		break;

		//Parameter:	bx = X, cx = Y
		case CURSOR:
			setCursor(ihs->rbx & 0xFFFF, ihs->rcx & 0xFFFF);
		break;

		//Parameter:	rbx = Filepointer
		//Rückgabewert: rax = PID des neuen Prozesses
		case EXEC:
			ihs->rax = loader_load((void*)ihs->rbx);
		break;

		//Parameter:	rbx = Rückgabecode
		case EXIT:
			ihs = pm_ExitTask(ihs, ihs->rbx);
		break;

		//Parameter:	rbx = Pfad, rcx = Addresse zur Modusstruktur
		//Rückgabewert:	rax = Pointer zur Systemdateistruktur
		case FOPEN:
			ihs->rax = (uintptr_t)vfs_Open((char*)ihs->rbx, *((vfs_mode_t*)ihs->rcx));
		break;

		//Parameter:	rbx = Pointer zur Systemdateistruktur
		case FCLOSE:
			vfs_Close((vfs_stream_t*)ihs->rbx);
		break;

		//Parameter:	rbx = Systemdateistruktur, rcx = Start, rdx = Länge, rdi = Buffer
		//Rückgabewert:	rax = Bytes, die erfolgreich gelesen wurden
		case FREAD:
			ihs->rax = vfs_Read((vfs_stream_t*)ihs->rbx, ihs->rcx, ihs->rdx, (void*)ihs->rdi);
		break;

		//Parameter:	rbx = Systemdateistruktur, rcx = Start, rdx = Länge, rdi = Buffer
		//Rückgabewert:	rax = Bytes, die erfolgreich geschrieben wurden
		case FWRITE:
			ihs->rax = vfs_Write((vfs_stream_t*)ihs->rbx, ihs->rcx, ihs->rdx, (void*)ihs->rdi);
		break;

		//Parameter:	rbx = Systemdateistruktur, rcx = Informationstyp
		//Rückgabewert:	rax = Wert für den entsprechenden Informationstyp
		case FINFO:
			ihs->rax = vfs_getFileinfo((vfs_stream_t*)ihs->rbx, ihs->rcx);
		break;

		//Parameter:	rax = Adresse
		//Rückgabewert:	rax = Adresse
		case TIME:
			ihs->rax = cmos_GetTime(ihs->rax);
		break;

		//Parameter:	rax = Adresse
		//Rückgabewert:	rax = Adresse
		case DATE:
			ihs->rax = cmos_GetDate(ihs->rax);
		break;

		//Rückgabewert:	rax = Adresse zur Informationsstruktur
		case SYSINF:
			getSystemInformation(ihs->rbx);
		break;
	}
	return ihs;
}

#endif
