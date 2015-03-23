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
#include "vfs.h"

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
console_t *activeConsole;

static void console_scrollDown();
static size_t console_readHandler(void *data, uint64_t start, size_t length, const void *buffer);
static size_t console_writeHandler(void *data, uint64_t start, size_t length, const void *buffer);
static void *console_stdin_getValue(void *data, vfs_device_function_t function);
static void *console_stdout_getValue(void *data, vfs_device_function_t function);

void console_Init()
{
	consoles = list_create();
	list_push(consoles, &initConsole);
	activeConsole = &initConsole;
	initConsole.input = list_create();
	vfs_device_t *stdin = malloc(sizeof(vfs_device_t));
	stdin->read = console_readHandler;
	stdin->write = NULL;
	stdin->getValue = console_stdin_getValue;
	stdin->opaque = NULL;
	vfs_RegisterDevice(stdin);
	vfs_device_t *stdout = malloc(sizeof(vfs_device_t));
	stdout->read = NULL;
	stdout->write = console_writeHandler;
	stdout->getValue = console_stdout_getValue;
	stdout->opaque = NULL;
	vfs_RegisterDevice(stdout);
}

console_t *console_create(uint8_t page)
{
	console_t *console = calloc(sizeof(console_t), 1);

	page %= MAX_PAGES;

	console->page = page;

	//80 * 25 Zeichen
	console->buffer = (void*)GRAFIKSPEICHER + 0x1000 * page;
	console->color = BG_BLACK | CL_LIGHT_GREY;

	console->input = list_create();

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

	console_t *console;
	size_t i = 0;
	while((console = list_get(consoles, i++)))
	{
		if(console->page == page)
		{
			activeConsole = console;
			break;
		}
	}
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

char console_getch(console_t *console)
{
	//Auf Eingabe warten
	while(list_empty(console->input)) asm volatile("hlt");

	return (char)list_remove(console->input, list_size(console->input) - 1);
}

void console_keyboardHandler(console_t *console, char c)
{
	list_push(console->input, (void*)c);
}

static size_t console_writeHandler(void *data, uint64_t start, size_t length, const void *buffer)
{
	size_t size = 0;
	console_t *console = pm_getConsole();
	char *str = buffer;
	if(console != NULL)
	{
		while(length-- && *str != '\0')
		{
			console_ansi_write(console, *str++);
		}
	}
	return size;
}

static size_t console_readHandler(void *data, uint64_t start, size_t length, const void *buffer)
{
	size_t size = 0;
	char *buf = buffer;

	while(length-- != 0)
	{
		buf[length] = getch();
		size++;
	}

	return size;
}

static void *console_stdin_getValue(void *data, vfs_device_function_t function)
{
	switch(function)
	{
		case FUNC_TYPE:
			return VFS_DEVICE_VIRTUAL;
			break;
		case FUNC_NAME:
			return "stdin";
			break;
		default:
			return NULL;
	}
}

static void *console_stdout_getValue(void *data, vfs_device_function_t function)
{
	switch(function)
	{
		case FUNC_TYPE:
			return VFS_DEVICE_VIRTUAL;
			break;
		case FUNC_NAME:
			return "stdout";
			break;
		default:
			return NULL;
	}
}
