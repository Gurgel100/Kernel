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

#define ASCII_ESC		'\e'

typedef enum{
	INVALID, NEED_MORE, SUCCESS
}esc_seq_status_t;

static cursor_t initCursor = {
	.x = 0,
	.y = 0
};

console_t initConsole = {
	.page = 0,
	.buffer = (void*)GRAFIKSPEICHER,
	.color = BG_BLACK | CL_WHITE,
	.cursor = &initCursor
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

console_t *console_createChild(console_t *parent)
{
	console_t *console;
	uint8_t page = 0;
	if(parent != NULL)
		page = parent->page;
	console = console_create(page);
	console->cursor = parent->cursor;
	console->color = parent->color;
	return console;
}

static esc_seq_status_t handle_ansi_formatting(console_t *console, uint8_t n)
{
	static char colors[8] = {CL_BLACK, CL_RED, CL_GREEN, CL_YELLOW, CL_BLUE, CL_MAGENTA, CL_CYAN, CL_WHITE};

	switch(n)
	{
		case 0:	//Alles zurücksetzen
			console->color = BG_BLACK | CL_LIGHT_GREY;
		break;
		case 30 ... 37:
			console->color = (console->color & 0xF0) | colors[n - 30];
		break;
		case 40 ... 47:
		console->color = (console->color & 0x0F) | (colors[n - 40] << 4);
		break;
		default:
			return INVALID;
	}

	return SUCCESS;
}

static esc_seq_status_t console_ansi_parse(console_t *console, const char *ansi_buf, uint8_t ansi_buf_len)
{
	uint8_t i;
	uint8_t n1 = 0, n2 = 0;
	bool delimiter = false;
	bool have_n1 = false, have_n2 = false;

	if(ansi_buf_len == 0)
	{
		return NEED_MORE;
	}
	if(ansi_buf[0] != ASCII_ESC)
	{
		return INVALID;
	}
	if(ansi_buf_len == 1)
	{
		return NEED_MORE;
	}
	if(ansi_buf[1] != '[')
	{
		return INVALID;
	}
	if(ansi_buf_len == 2)
	{
		return NEED_MORE;
	}

	for(i = 0; i < ansi_buf_len; i++)
	{
		switch(ansi_buf[i])
		{
			case '0' ... '9':
				if(!delimiter)
				{
					n1 = n1 * 10 + ansi_buf[i] - '0';
					have_n1 = true;
				}
				else
				{
					n2 = n2 * 10 + ansi_buf[i] - '0';
					have_n2 = true;
				}
			break;
			case ';':
				if(delimiter)
					return INVALID;
				delimiter = true;
			break;
			case 'm':	//ESC[#;#m oder ESC[#m für Vordergrund und/oder Hintergrundfarbe
				if(!have_n1)
					return INVALID;
				if(handle_ansi_formatting(console, n1) != SUCCESS)
					return INVALID;
				if(have_n2)
				{
					if(handle_ansi_formatting(console, n2) != SUCCESS)
						return INVALID;
				}
			return SUCCESS;
		}
	}

	return NEED_MORE;
}

void console_ansi_write(console_t *console, char c)
{
	if(console != NULL)
	{
		if(c == ASCII_ESC || console->ansi_buf_ofs > 0)
		{
			uint8_t i;
			console->ansi_buf[console->ansi_buf_ofs++] = c;
			esc_seq_status_t status = console_ansi_parse(console, console->ansi_buf, console->ansi_buf_ofs);
			switch(status)
			{
				case NEED_MORE:
					if(console->ansi_buf_ofs <= sizeof(console->ansi_buf))
						break;
				case INVALID:
					for(i = 0; i < console->ansi_buf_ofs; i++)
						console_write(console, console->ansi_buf[i]);

					console->ansi_buf_ofs = 0;
				break;
				case SUCCESS:
					console->ansi_buf_ofs = 0;
				break;
			}
		}
		else
		{
			console_write(console, c);
		}
	}
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
				console->cursor->y++;
				if(console->cursor->y > 24)
				{
					console_scrollDown(console);
					console->cursor->y = 24;
				}
				//bei '\n' soll auch an den Anfang der Zeile gesprungen werden.
				/* no break */
			case '\r':
				console->cursor->x = 0;
			break;
			case '\b':
				if(console->cursor->x == 0)
				{
					console->cursor->x = 79;
					console->cursor->y -= (console->cursor->y == 0) ? 0 : 1;
				}
				else
					console->cursor->x--;
				gs[console->cursor->y * COLS + console->cursor->x] = ' ';	//Das vorhandene Zeichen "löschen"
			break;
			default:
				//Zeichen in den Grafikspeicher kopieren
				gs[console->cursor->y * COLS + console->cursor->x] = (c | (Farbwert << 8));
				if(++console->cursor->x > 79)
				{
					console->cursor->x = 0;
					if(++console->cursor->y > 24)
					{
						console_scrollDown(console);
						console->cursor->y = 24;
					}
				}
			break;
		}
		setCursor(console->cursor->x, console->cursor->y);
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
		*console->cursor = cursor;
	}
}
