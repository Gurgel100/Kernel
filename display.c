/*
 * display.c
 *
 *  Created on: 07.07.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "display.h"
#include "util.h"
#include "string.h"
#include "console.h"
#include "pm.h"
#include "stdio.h"

#define GRAFIKSPEICHER 0xB8000

uint8_t Spalte, Zeile;

void Display_Init()
{
	Display_Clear();
	setColor(BG_BLACK | CL_WHITE);
	/*uint8_t Register;
	inb(0x3D8, Register);
	Register &= 0xDF;
	outb(0x3D8, Register);*/
}

void display_refresh(const uint16_t *buffer, uint8_t cursor_x, uint8_t cursor_y)
{
	memcpy((void*)GRAFIKSPEICHER, buffer, DISPLAY_COLS * DISPLAY_ROWS * sizeof(uint16_t));
	setCursor(cursor_x, cursor_y);
}

void __attribute__((deprecated)) setColor(uint8_t Color)
{
	//console_changeColor(pm_getConsole(), Color);
}

void setCursor(uint8_t x, uint8_t y)
{
	//Diese Funktion kann auch extern aufgerufen werden, deshalb müssen hier die
	//Variablen "Spalte" und "Zeile" auch gesetzt werden.
	Spalte = (x <= 79) ? x : 79;
	Zeile = (y <= 24) ? y : 24;
	//Cursorposition verändern
	uint16_t CursorAddress = Zeile * 80 + Spalte;
	outb(0x3D4, 15);			//Lowbyte der Cursorposition an Grafikkarte senden
	outb(0x3D5, CursorAddress & 0xFF);
	outb(0x3D4, 14);			//Highbyte der Cursorposition an GK senden
	outb(0x3D5, CursorAddress >> 8);
}

void SysLog(char *Device, char *Text)
{
	printf("[ \e[32m%s\e[37m ] %s\n", Device, Text);
}

void SysLogError(char *Device, char *Text)
{
	printf("[ \e[31m%s\e[37m ] \e[31m%s\e[37m\n", Device, Text);
}

/*
 * Löscht den Bildschirminhalt
 */
void Display_Clear()
{
	int i;
	char *gs = (char*)GRAFIKSPEICHER;
	for(i = 0; i < 4096; i++)
		gs[i] = 0;
	Zeile = Spalte = 0;
}

/*
 * Gibt eine Fehlermeldung aus und hält die CPU an (für immer).
 */
void Panic(char *Device, char *Text)
{
	printf("\e[41;37m[ %s ] %s", Device, Text);
	console_switch(0);
	asm("cli;hlt");
}

void hideCursor()
{
	outb(0x3D4, 10);
	outb(0x3D5, 0x32);	//Cursor deaktiviert
}

void showCursor()
{
	outb(0x3D4, 10);
	outb(0x3D5, 0x64);	//Cursor aktiviert
}

#endif
