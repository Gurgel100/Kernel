/*
 * console.c
 *
 *  Created on: 27.01.2015
 *      Author: pascal
 */

#include "console.h"
#include "list.h"
#include "stdlib.h"
#include "display.h"
#include "string.h"
#include "util.h"

#define GRAFIKSPEICHER	0xB8000
#define MAX_PAGES		8
#define ROWS			25
#define COLS			80
#define SIZE_PER_CHAR	2
#define SIZE_PER_ROW	(COLS * SIZE_PER_CHAR)
#define DISPLAY_PAGE_OFFSET(page) ((page) * ((ROWS * COLS + 0xff) & 0xff00))
#define PAGE_SIZE		ROWS * SIZE_PER_ROW

console_t initConsole = {
	.page = 0,
	.buffer = (void*)GRAFIKSPEICHER,
	.color = BG_BLACK | CL_WHITE,
	.cursor = {
		.x = 0,
		.y = 0
	}
};

static list_t consoles;

static void console_scrollDown();

void console_Init()
{
	consoles = list_create();
	list_push(consoles, &initConsole);
}

console_t *console_create(uint8_t page)
{
	console_t *console = calloc(sizeof(console_t), 1);

	page %= MAX_PAGES;

	console->page = page;

	//80 * 25 Zeichen
	console->buffer = (void*)GRAFIKSPEICHER + 0x1000 * page;
	console->color = BG_BLACK | CL_LIGHT_GREY;

	list_push(consoles, console);

	return console;
}

void console_write(console_t *console, char c)
{
	if(console != NULL)
	{
		uint8_t Farbwert = console->color;
		uint16_t *gs = (uint16_t*)console->buffer;
		switch(c)
		{
			case '\n':
				console->cursor.y++;
				if(console->cursor.y > 24)
				{
					console_scrollDown(console);
					console->cursor.y = 24;
				}
				//bei '\n' soll auch an den Anfang der Zeile gesprungen werden.
				/* no break */
			case '\r':
				console->cursor.x = 0;
			break;
			case '\b':
				if(console->cursor.x == 0)
				{
					console->cursor.x = 79;
					console->cursor.y -= (console->cursor.y == 0) ? 0 : 1;
				}
				else
					console->cursor.x--;
				gs[console->cursor.y * COLS + console->cursor.x] = ' ';	//Das vorhandene Zeichen "löschen"
			break;
			default:
				//Zeichen in den Grafikspeicher kopieren
				gs[console->cursor.y * COLS + console->cursor.x] = (c | (Farbwert << 8));
				if(++console->cursor.x > 79)
				{
					console->cursor.x = 0;
					if(++console->cursor.y > 24)
					{
						console_scrollDown(console);
						console->cursor.y = 24;
					}
				}
			break;
		}
		setCursor(console->cursor.x, console->cursor.y);
	}
}

void console_clear(console_t *console)
{
	if(console != NULL)
	{
		memset(console->buffer, 0, PAGE_SIZE);
	}
}

void console_switch(uint8_t page)
{
	page = page % MAX_PAGES;
	//Anstatt rumzukopieren wechseln wir einfach die Adresse
	outb(0x3D4, 13);
	outb(0x3D5, DISPLAY_PAGE_OFFSET(page));
	outb(0x3D4, 12);
	outb(0x3D5, DISPLAY_PAGE_OFFSET(page) >> 8);
}

static void console_scrollDown(console_t *console)
{
	if(console != NULL)
	{
		//Letzte 24 Zeilen eine Zeile nach oben verschieben
		memmove(console->buffer, console->buffer + SIZE_PER_ROW, PAGE_SIZE - SIZE_PER_ROW);

		//Letzte Zeile löschen
		memset(console->buffer + PAGE_SIZE - SIZE_PER_ROW, 0, SIZE_PER_ROW);
	}
}

void console_changeColor(console_t *console, uint8_t color)
{
	if(console != NULL)
	{
		console->color = color;
	}
}

void console_setCursor(console_t *console, cursor_t cursor)
{
	if(console != NULL)
	{
		console->cursor = cursor;
	}
}
