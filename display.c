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

#define GRAFIKSPEICHER 0xB8000

static uint8_t Textcolor;
static uint8_t Background;
uint8_t Spalte, Zeile;

void Display_Init()
{
	Display_Clear();
	setColor(BG_BLACK | CL_WHITE);
	//hideCursor();
	showCursor();
	/*uint8_t Register;
	inb(0x3D8, Register);
	Register &= 0xDF;
	outb(0x3D8, Register);*/
}

void setColor(uint8_t Color)
{
	Textcolor = Color & 0xF;
	Background = Color & 0x70;
}

/*
 * Schreibt das Zeichen c auf den Bildschirm
 * Parameter:	c = ASCII-Zeichen, das geschrieben werden soll
 */
void putch(unsigned char c)
{
	uint8_t Farbwert = Background | Textcolor;
	uint16_t *gs = (uint16_t*)GRAFIKSPEICHER;
	switch(c)
	{
		case '\n':
			Zeile++;
			if(Zeile >= 24)
			{
				scrollScreenDown();
				Zeile = 24;
			}
			//bei '\n' soll auch an den Anfang der Zeile gesprungen werden.
			/* no break */
		case '\r':
			Spalte = 0;
		break;
		case '\b':
			if(Spalte == 0)
			{
				Spalte = 79;
				Zeile -= (Zeile == 0) ? 0 : 1;
			}
			else
				Spalte--;
			gs[Zeile * 80 + Spalte] = ' ';	//Das vorhandene Zeichen "löschen"
		break;
		default:
			//Zeichen in den Grafikspeicher kopieren
			gs[Zeile * 80 + Spalte] = (c | (Farbwert << 8));
			if(++Spalte > 79)
			{
				Spalte = 0;
				if(++Zeile > 24)
				{
					scrollScreenDown();
					Zeile = 24;
				}
			}
		break;
	}
	setCursor(Spalte, Zeile);
}

/*
 * Erste Version von der Funktion print(). Wird aber nur im Kernel benutzt und ist
 * nicht empfohlen für Anwendungen. Wird irgendeinmal mal durch printf() ersetzt werden.
 */
/*void print(char *Text, uint8_t Background, uint8_t Color)
{
	setColor(Background | Color);
	printf(Text);
}*/

/*
 * Scrollt den Bildschirm um eine Zeile nach unten.
 */
void scrollScreenDown()
{
	int zeile, spalte;
	uint16_t *gs = (uint16_t*)GRAFIKSPEICHER;
	for(zeile = 0; zeile < 24; zeile++)
		for(spalte = 0; spalte < 80; spalte++)
			gs[zeile * 80 + spalte] = gs[(zeile + 1) * 80 + spalte];
	//Letzte Zeile löschen
	zeile = 24;
	for(spalte = 0; spalte < 80; spalte++)
		gs[zeile * 80 + spalte] = 0;
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
	setColor(BG_BLACK | CL_WHITE);
	printf("[ ");
	setColor(BG_BLACK | CL_GREEN);
	printf(Device);
	setColor(BG_BLACK | CL_WHITE);
	printf(" ]");
	setCursor(30, Zeile);
	printf("%s\n\r", Text);
}

void SysLogError(char *Device, char *Text)
{
	setColor(BG_BLACK | CL_WHITE);
	printf("[ ");
	setColor(BG_BLACK | CL_RED);
	printf(Device);
	setColor(BG_BLACK | CL_WHITE);
	printf(" ]");
	setCursor(30, Zeile);
	setColor(BG_BLACK | CL_RED);
	printf("%s\n", Text);
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
	setColor(BG_RED | CL_WHITE);
	printf("[ %s ]", Device);
	setCursor(30, Zeile);
	printf(Text);
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

size_t Display_FileHandler(char *name, uint64_t start, size_t length, const void *buffer)
{
	size_t size = 0;
	char *str = (char*)buffer;

	if(strcmp(name, "stdout") == 0)
	{
		while(length-- && *str != '\0')
		{
			putch(*str++);
			size++;
		}
	}
	else if(strcmp(name, "stderr") == 0)
	{
		setColor(BG_RED | CL_BLACK);
		while(length-- && *str != '\0')
		{
			putch(*str++);
			size++;
		}
	}

	return size;
}

#endif
