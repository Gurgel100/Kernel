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
#include "scheduler.h"
#include "keyboard.h"
#include "assert.h"

#define GRAFIKSPEICHER	0xB8000
#define MAX_PAGES		12
#define ROWS			25
#define COLS			80
#define SIZE_PER_CHAR	2
#define SIZE_PER_ROW	(COLS * SIZE_PER_CHAR)
#define DISPLAY_PAGE_OFFSET(page) ((page) * ((ROWS * COLS + 0xff) & 0xff00))
#define PAGE_SIZE		ROWS * SIZE_PER_ROW

#define TAB_HORIZONTAL	4

#define INPUT_BUFFER_SIZE	16

#define MIN(a, b)		((a < b) ? a : b)

#define CONSOLE_NUM		12

#define ASCII_ESC		'\e'

typedef enum{
	INVALID, NEED_MORE, SUCCESS
}esc_seq_status_t;

console_t initConsole = {
	.buffer = (void*)GRAFIKSPEICHER,
	.color = BG_BLACK | CL_WHITE,
	.flags = CONSOLE_AUTOREFRESH | CONSOLE_AUTOSCROLL,
	.height = ROWS,
	.width = COLS
};

static size_t nextID = 1;
console_t *activeConsole = &initConsole;
static console_t* consoles[CONSOLE_NUM];
static char *console_names[CONSOLE_NUM] = {
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
static size_t console_readHandler(void *c, uint64_t start, size_t length, void *buffer);
static size_t console_writeHandler(void *c, uint64_t start, size_t length, const void *buffer);
static void *console_functionHandler(void *c, vfs_device_function_t function, ...);
static vfs_device_capabilities_t console_getCapabilitiesHandler(void *c);

static void handler_charPress(void *opaque, char c)
{
	console_t *console = *(console_t**)opaque;

	console->inputBuffer[console->inputBufferEnd++] = c;
	if(console->inputBufferEnd == console->inputBufferSize)
		console->inputBufferEnd = 0;
	if(console->inputBufferEnd == console->inputBufferStart)
		console->inputBufferStart = (console->inputBufferStart + 1 < console->inputBufferSize) ? console->inputBufferStart + 1 : 0;
	assert(console->inputBufferStart != console->inputBufferEnd);

	if(console->id != 0 && console->waitingThread != NULL)
	{
		thread_unblock(console->waitingThread);
		console->waitingThread = NULL;
	}
}

static void handler_keyDown(void *opaque, KEY_t key)
{
	console_t *console = *(console_t**)opaque;

	if(keyboard_isKeyPressed(KEY_LALT))
	{
		if(key == KEY_F1)
			console_switch(1);
		else if(key == KEY_F2)
			console_switch(2);
		else if(key == KEY_F3)
			console_switch(3);
		else if(key == KEY_F4)
			console_switch(4);
		else if(key == KEY_F5)
			console_switch(5);
		else if(key == KEY_F6)
			console_switch(6);
		else if(key == KEY_F7)
			console_switch(7);
		else if(key == KEY_F8)
			console_switch(8);
		else if(key == KEY_F9)
			console_switch(9);
		else if(key == KEY_F10)
			console_switch(10);
		else if(key == KEY_F11)
			console_switch(11);
		else if(key == KEY_F12)
			console_switch(12);
		else if(key == KEY_ESC)
			console_switch(0);
	}
}

static void handler_keyUp(void *opaque, KEY_t key)
{
	console_t *console = *(console_t**)opaque;
}

void console_Init()
{
	initConsole.inputBufferSize = INPUT_BUFFER_SIZE;
	initConsole.inputBuffer = malloc(initConsole.inputBufferSize);

	//Aktuellen Bildschirminhalt in neuen Buffer kopieren
	initConsole.buffer = memcpy(malloc(PAGE_SIZE), initConsole.buffer, PAGE_SIZE);

	//Alle Konsolen anlegen
	uint64_t i;
	for(i = 0; i < CONSOLE_NUM; i++)
	{
		console_t *console = console_create(console_names[i], BG_BLACK | CL_LIGHT_GREY);
		consoles[i] = console;

		vfs_device_t *tty = malloc(sizeof(vfs_device_t));
		tty->opaque = console;
		tty->read = console_readHandler;
		tty->write = console_writeHandler;
		tty->function = console_functionHandler;
		tty->getCapabilities = console_getCapabilitiesHandler;
		vfs_RegisterDevice(tty);
	}

	//Keyboardhandler registrieren
	keyboard_registerCharHandler(handler_charPress, &activeConsole);
	keyboard_registerKeydownHandler(handler_keyDown, &activeConsole);
	keyboard_registerKeyupHandler(handler_keyUp, &activeConsole);
}

/*
 * Erstellt eine neue Konsole.
 * Paramater:	name = Name der Konsole
 * 				width = Breite der Konsole
 * 				height = Höhe der Konsole
 * 				color = Farbwerte der Konsole
 * Rückgabe:	Neue Konsole oder NULL bei Fehler
 */
console_t *console_create(char *name, uint8_t color)
{
	console_t *console = malloc(sizeof(console_t));
	if(console == NULL)
		return NULL;

	console->name = strdup(name);
	if(console->name == NULL)
	{
		free(console);
		return NULL;
	}

	console->buffer = calloc(SIZE_PER_ROW, ROWS);
	if(console->buffer == NULL)
	{
		free(console->name);
		free(console);
		return NULL;
	}
	console->color = color;

	console->inputBufferSize = INPUT_BUFFER_SIZE;
	console->inputBufferStart = console->inputBufferEnd = 0;
	console->inputBuffer = malloc(console->inputBufferSize);
	if(console->inputBuffer == NULL)
	{
		free(console->name);
		free(console->buffer);
		free(console);
		return NULL;
	}

	console->flags = CONSOLE_AUTOREFRESH | CONSOLE_AUTOSCROLL;
	console->height = ROWS;
	console->width = COLS;
	console->id = nextID++;
	unlock(&console->lock);

	console_clear(console);

	return console;
}

console_t *console_getByName(char *name)
{
	size_t i;
	for(i = 0; i < CONSOLE_NUM; i++)
	{
		if(strcmp(name, consoles[i]->name) == 0)
			return consoles[i];
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
			case 'A':
				if(!have_n1)
					n1 = 1;
				if(console->cursor.y > n1)
					console->cursor.y -= n1;
				return SUCCESS;
			case 'B':
				if(!have_n1)
					n1 = 1;
				console->cursor.y = MIN(console->height, console->cursor.y + n1);
				return SUCCESS;
			case 'C':
				if(!have_n1)
					n1 = 1;
				console->cursor.x = MIN(console->width, console->cursor.x + n1);
				return SUCCESS;
			case 'D':
				if(!have_n1)
					n1 = 1;
				if(console->cursor.x > n1)
					console->cursor.x -= n1;
				return SUCCESS;
			case 'H':	//Setze Cursor Position an n1,n2, wobei n1 und n2 1-basiert sind
			case 'f':
				if(!have_n1)
					n1 = 1;
				else if(n1 == 0)
					return INVALID;
				if(!have_n2)
					n2 = 1;
				else if(n2 == 0)
					return INVALID;
				console_setCursor(console, (cursor_t){MIN(console->width, n1 - 1), MIN(console->height, n2 - 1)});
				return SUCCESS;
			case 's':	//Cursor speichern
				console->saved_cursor = console->cursor;
				return SUCCESS;
			case 'u':	//Cursor wiederherstellen
				console_setCursor(console, console->saved_cursor);
				return SUCCESS;

			//TODO: Deadlock beheben durch reentrant locks z.B.
			/*case 'J':	//ESC[2J Konsole löschen
				if(!have_n1 || n1 != 2)
					return INVALID;
				console_clear(console);
				return SUCCESS;
			case 'K':	//Zeile löschen
				console_clearLine(console, console->cursor.y);
				printf("line cleared\n");
				return SUCCESS;*/
		}
	}

	return NEED_MORE;
}

void console_ansi_write(console_t *console, char c)
{
	if(console != NULL)
	{
		lock(&console->lock);
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
					/* no break */
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
		unlock(&console->lock);
	}
}

void console_write(console_t *console, char c)
{
	if(console != NULL)
	{
		uint8_t Farbwert = console->color;
		uint16_t *gs = (uint16_t*)GRAFIKSPEICHER;
		uint16_t *buffer = (uint16_t*)console->buffer;
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
				buffer[console->cursor.y * COLS + console->cursor.x] = ' ';	//Das vorhandene Zeichen "löschen"
				if(console == activeConsole && (console->flags & CONSOLE_AUTOREFRESH))
					gs[console->cursor.y * COLS + console->cursor.x] = ' ';	//Screen updaten
			break;
			case '\t':
			{
				//Horizontal tabs go to the next 4 bound
				uint8_t diff = TAB_HORIZONTAL - console->cursor.x % TAB_HORIZONTAL;
				//Zeichen löschen
				uint8_t i;
				for(i = 0; i < diff; i++)
				{
					buffer[console->cursor.y * COLS + console->cursor.x + i] = (' ' | (Farbwert << 8));
					if(console == activeConsole && (console->flags & CONSOLE_AUTOREFRESH))
						gs[console->cursor.y * COLS + console->cursor.x] = (' ' | (Farbwert << 8));
				}
				console->cursor.x += diff;
				if(console->cursor.x > 79)
				{
					console->cursor.x = 0;
					if(++console->cursor.y > 24)
					{
						console_scrollDown(console);
						console->cursor.y = 24;
					}
				}
			}
			break;
			default:
				//Zeichen in den Grafikspeicher kopieren
				buffer[console->cursor.y * COLS + console->cursor.x] = (c | (Farbwert << 8));
				if(console == activeConsole && (console->flags & CONSOLE_AUTOREFRESH))
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
		if(console == activeConsole && (console->flags & CONSOLE_AUTOREFRESH))
			setCursor(console->cursor.x, console->cursor.y);
	}
}

void console_clear(console_t *console)
{
	if(console != NULL)
	{
		lock(&console->lock);
		size_t i;
		for(i = 0; i < PAGE_SIZE / 2; i++)
		{
			((uint16_t*)console->buffer)[i] = ' ' | (console->color << 8);
		}
		unlock(&console->lock);
	}
	if(activeConsole == console && (console->flags & CONSOLE_AUTOREFRESH))
		display_refresh();
}

void console_clearLine(console_t *console, uint16_t line)
{
	if(console != NULL)
	{
		lock(&console->lock);
		size_t i;
		for(i = 0; i < console->width; i++)
			((uint16_t*)console->buffer)[line * console->width + i] = ' ' | (console->color << 8);
		unlock(&console->lock);
	}
	if(activeConsole == console && (console->flags & CONSOLE_AUTOREFRESH))
		display_refresh();
}

void displayConsole(console_t *console)
{
	if(console != NULL)
	{
		activeConsole = console;
		if(console->flags & CONSOLE_AUTOREFRESH)
			display_refresh();
	}
}

void console_switch(uint8_t id)
{
	if(id == 0)
	{
		if(activeConsole != &initConsole)
			displayConsole(&initConsole);
	}
	else
	{
		size_t i;
		for(i = 0; i < CONSOLE_NUM; i++)
		{
			if(consoles[i]->id == id)
			{
				if(activeConsole != consoles[i])
					displayConsole(consoles[i]);
				return;
			}
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
		size_t i;
		for(i = 0; i < SIZE_PER_ROW / 2; i++)
		{
			((uint16_t*)(console->buffer + PAGE_SIZE - SIZE_PER_ROW))[i] = ' '| (console->color << 8);
		}

		if(console == activeConsole && (console->flags & CONSOLE_AUTOREFRESH))
			display_refresh();
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
		if(console == activeConsole && (console->flags & CONSOLE_AUTOREFRESH))
			setCursor(console->cursor.x, console->cursor.y);
	}
}

char console_getch(console_t *console)
{
	if(console->id == 0)
	{
		while(console->inputBufferStart == console->inputBufferEnd) asm volatile("hlt");
	}
	else if(console->inputBufferStart == console->inputBufferEnd)
	{
		//Auf Eingabe warten
		console->waitingThread = currentThread;
		thread_waitUserIO(console->waitingThread);
	}

	assert(console->inputBufferStart != console->inputBufferEnd);
	char c = console->inputBuffer[console->inputBufferStart++];
	if(console->inputBufferStart == console->inputBufferSize)
		console->inputBufferStart = 0;
	return c;
}

static size_t console_writeHandler(void *c, uint64_t __attribute__((unused)) start, size_t length, const void *buffer)
{
	console_t *console = c;
	size_t size = 0;
	const char *str = buffer;
	while(length-- && *str != '\0')
	{
		console_ansi_write(console, *str++);
		size++;
	}
	return size;
}

static size_t console_readHandler(void *c, uint64_t __attribute__((unused)) start, size_t length, void *buffer)
{
	console_t *console = c;
	size_t size = 0;
	char *buf = buffer;

	while(length-- != 0)
	{
		buf[length] = console_getch(console);
		size++;
	}

	return size;
}

static void *console_functionHandler(void *c, vfs_device_function_t function, ...)
{
	console_t *console = c;
	switch (function)
	{
		case VFS_DEV_FUNC_TYPE:
			return (void*)VFS_DEVICE_VIRTUAL;
		case VFS_DEV_FUNC_NAME:
			return console->name;
		default:
			return NULL;
	}
}

static vfs_device_capabilities_t console_getCapabilitiesHandler(void *c)
{
	console_t *console = c;

	return 0;
}
