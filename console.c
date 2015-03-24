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
#include "stdio.h"

#define GRAFIKSPEICHER	0xB8000
#define MAX_PAGES		12
#define ROWS			25
#define COLS			80
#define SIZE_PER_CHAR	2
#define SIZE_PER_ROW	(COLS * SIZE_PER_CHAR)
#define DISPLAY_PAGE_OFFSET(page) ((page) * ((ROWS * COLS + 0xff) & 0xff00))
#define PAGE_SIZE		ROWS * SIZE_PER_ROW

#define CONSOLE_NUM		20

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
static char *pages[12] = {
		"tty01",
		"tty02",
		"tty03",
		"tty04",
		"tty05",
		"tty06",
		"tty07",
		"tty08",
		"tty09",
		"tty10",
		"tty11",
		"tty12"
};

static void console_scrollDown();
static size_t console_readHandler(console_t *console, uint64_t start, size_t length, const void *buffer);
static size_t console_writeHandler(console_t *console, uint64_t start, size_t length, const void *buffer);
static void *console_getValue(console_t *console, vfs_device_function_t function);

void console_Init()
{
	consoles = list_create();
	list_push(consoles, &initConsole);
	activeConsole = &initConsole;
	initConsole.input = list_create();

	//stdin
	console_t *stdin = console_create("stdin", COLS, ROWS, BG_BLACK | CL_LIGHT_GREY);
	vfs_device_t *dev = malloc(sizeof(vfs_device_t));
	dev->read = console_readHandler;
	dev->write = console_writeHandler;
	dev->getValue = console_getValue;
	dev->opaque = stdin;
	vfs_RegisterDevice(dev);

	//stdout
	console_t *stdout = console_create("stdout", COLS, ROWS, BG_BLACK | CL_LIGHT_GREY);
	dev = malloc(sizeof(vfs_device_t));
	dev->read = console_readHandler;
	dev->write = console_writeHandler;
	dev->getValue = console_getValue;
	dev->opaque = stdout;
	vfs_RegisterDevice(dev);

	//stderr
	console_t *stderr = console_create("stderr", COLS, ROWS, BG_BLACK | CL_LIGHT_GREY);
	dev = malloc(sizeof(vfs_device_t));
	dev->read = console_readHandler;
	dev->write = console_writeHandler;
	dev->getValue = console_getValue;
	dev->opaque = stderr;
	vfs_RegisterDevice(dev);

	//Alle Konsolen anlegen
	uint64_t i;
	for(i = 1; i <= CONSOLE_NUM; i++)
	{
		char *name;
		asprintf(&name, "tty%02u", i);
		console_t *console = console_create(name, COLS, ROWS, BG_BLACK | CL_LIGHT_GREY);
		free(name);

		vfs_device_t *tty = malloc(sizeof(vfs_device_t));
		tty->opaque = console;
		tty->read = console_readHandler;
		tty->write = console_writeHandler;
		tty->getValue = console_getValue;
		vfs_RegisterDevice(tty);
	}
}

/*
 * Erstellt eine neue Konsole.
 * Paramater:	name = Name der Konsole
 * 				width = Breite der Konsole
 * 				height = Höhe der Konsole
 * 				color = Farbwerte der Konsole
 * Rückgabe:	Neue Konsole oder NULL bei Fehler
 */
console_t *console_create(char *name, uint16_t width, uint16_t height, uint8_t color)
{
	console_t *console = calloc(sizeof(console_t), 1);
	if(console == NULL)
		return NULL;

	console->name = strdup(name);
	if(console->name == NULL)
	{
		free(console);
		return NULL;
	}

	console->buffer = calloc(height, width * 2);
	if(console->buffer == NULL)
	{
		free(console->name);
		free(console);
		return NULL;
	}
	console->color = color;

	console->input = list_create();
	if(console->input == NULL)
	{
		free(console->name);
		free(console->buffer);
		free(console);
		return NULL;
	}

	console->cursor = calloc(1, sizeof(cursor_t));
	if(console->cursor == NULL)
	{
		list_destroy(console->input);
		free(console->name);
		free(console->buffer);
		free(console);
		return NULL;
	}

	list_push(consoles, console);

	return console;
}

console_t *console_createChild(console_t *parent)
{
	if(parent != NULL)
	{
		console_t *console = console_create(parent->name, parent->width, parent->height, parent->color);
		free(console->buffer);
		console->buffer = parent->buffer;
		free(console->cursor);
		console->cursor = parent->cursor;
		return console;
	}
	return NULL;
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

	console_t *console;
	size_t i = 0;
	while((console = list_get(consoles, i++)))
	{
		if(strcmp(console->name, pages[page]) == 0)
		{
			activeConsole = console;
			//Buffer reinkopieren
			memcpy(GRAFIKSPEICHER, console->buffer, PAGE_SIZE);
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

static size_t console_writeHandler(console_t *console, uint64_t start, size_t length, const void *buffer)
{
	size_t size = 0;
	char *str = (char*)buffer;
	while(length-- && *str != '\0')
	{
		console_ansi_write(console, *str++);
		size++;
	}
	return size;
}

static size_t console_readHandler(console_t *console, uint64_t start, size_t length, const void *buffer)
{
	size_t size = 0;
	char *buf = (char*)buffer;

	while(length-- != 0)
	{
		buf[length] = getch();
		size++;
	}

	return size;
}

static void *console_getValue(console_t *console, vfs_device_function_t function)
{
	switch (function)
	{
		case FUNC_TYPE:
			return VFS_DEVICE_VIRTUAL;
		case FUNC_NAME:
			return console->name;
		default:
			return NULL;
	}
}
