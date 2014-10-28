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

extern void putch(char);
extern char getch(void);
//extern uintptr_t mm_SysAlloc(uint64_t);
//extern void mm_SysFree(uintptr_t, uint64_t);
extern void setColor(uint8_t);
extern void setCursor(uint16_t, uint16_t);

/*
 * Funktionsnummer wird im Register rax übergeben
 */
void syscall_Handler(ihs_t *ihs)
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
}

#endif
