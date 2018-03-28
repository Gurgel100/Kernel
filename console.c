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
#define MAX_ROWS		(MAX_PAGES * ROWS)

#define TAB_HORIZONTAL	4

#define INPUT_BUFFER_SIZE	16

#define MAX(a, b)		((a > b) ? a : b)
#define MIN(a, b)		((a < b) ? a : b)

#define CONSOLE_NUM		12

#define ASCII_ESC		'\e'

typedef enum{
	INVALID, NEED_MORE, SUCCESS
}esc_seq_status_t;

//Wandelt eine Taste in ein ASCII-Zeichen um
static const char KeyToAscii_default[] =
{
	0,
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q',
	'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0,
	'\t', 0, 0, 0, 0, 0, 0, 0,
	0, ' ', 0, 0, '\n', '\b', 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0,
	L'ä', L'ö', L'ü', L'¨', '$',
	L'§', '\'', L'^',
	'<', ',', '.', '-',
	//Keypad-Tasten
	0, '/', '*', '-', '+', '\n', '.',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
};

static const char KeyToAscii_shift[] =
{
	0,
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q',
	'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'=', '+', '"', '*', L'ç', '%', '&', '/', '(', ')',
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, ' ', 0, 0, '\n', '\b', 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0,
	L'à', L'é', L'è', '!', L'£',
	L'°', '?', '`',
	'>', ';', ':', '_',
	//Keypad-Tasten
	0, '/', '*', '-', '+', '\n', 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

//TODO: Richtige Zeicheṇ
static const char KeyToAscii_altgr[] =
{
	0,
	'æ', '”', '¢', 'ð', '€', 'đ', 'ŋ', 'ħ', '→', 'j', 'ĸ', 'ł', 'µ',
	'n', 'ø', 'þ', '@', '¶', 'ß', 'ŧ', '↓', '“', 'ł', '»', '«', '←',
	'}', '|', '@', '#', '¼', '½', '¬', '|', '¢', ']',
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0,
	'\t', 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, '\n', '\b', 0,
	0, 0, 0, 0, 0, 0,
	0, 0, 0, 0,
	'{', L'´', '[', ']', '}',
	L'¬', L'´', '~',
	'\\', 0, L'·', '̣',
	//Keypad-Tasten
	0, '/', '*', '-', '+', '\n', '.',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
};

console_t initConsole = {
	.buffer = (void*)GRAFIKSPEICHER,
	.screenBuffer = (void*)GRAFIKSPEICHER,
	.color = BG_BLACK | CL_WHITE,
	.flags = CONSOLE_AUTOREFRESH | CONSOLE_AUTOSCROLL | CONSOLE_ECHO,
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

static void scroll(console_t *console, int i, bool add)
{
	if(add && i < 0 && console->currentRow == 0)
	{
		i = MIN(-i, MAX_ROWS);
		memmove(console->buffer, console->buffer + SIZE_PER_ROW * i, MAX_PAGES * PAGE_SIZE - SIZE_PER_ROW * i);

		console->usedRows = MIN(console->usedRows + i, MAX_ROWS);

		//Letzte Zeile löschen
		for(uint32_t j = 1; j <= (uint32_t)i; j++)
		{
			for(size_t k = 0; k < SIZE_PER_ROW / sizeof(uint16_t); k++)
			{
				((uint16_t*)(console->buffer + MAX_PAGES * PAGE_SIZE - j * SIZE_PER_ROW))[k] = ' ' | (console->color << 8);
			}
		}
	}
	else
	{
		if(i < 0 && console->currentRow + i > console->currentRow)
			console->currentRow = 0;
		else
			console->currentRow = MIN(console->currentRow + i, console->usedRows);
	}

	if(console == activeConsole && (console->flags & CONSOLE_AUTOREFRESH))
		display_refresh(console->buffer + (MAX_PAGES - 1) * PAGE_SIZE - SIZE_PER_ROW * console->currentRow, console->cursor.x, console->cursor.y);
}

static char convertKeyToAscii(console_key_status_t *key_status)
{
	if(key_status->shift)
		return KeyToAscii_shift[key_status->key];
	else if(key_status->altgr)
		return KeyToAscii_altgr[key_status->key];
	else
		return KeyToAscii_default[key_status->key];
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
		else if(key == KEY_PGUP)
			scroll(console, 10, false);
		else if(key == KEY_PGDOWN)
			scroll(console, -10, false);
		else if(key == KEY_UP)
			scroll(console, 1, false);
		else if(key == KEY_DOWN)
			scroll(console, -1, false);
		return;
	}

	console_key_status_t *key_status = &console->inputBuffer[console->inputBufferEnd++];
	if(console->inputBufferEnd == console->inputBufferSize)
		console->inputBufferEnd = 0;
	if(console->inputBufferEnd == console->inputBufferStart)
		console->inputBufferStart = (console->inputBufferStart + 1 < console->inputBufferSize) ? console->inputBufferStart + 1 : 0;
	assert(console->inputBufferStart != console->inputBufferEnd);

	key_status->shift = (keyboard_isKeyPressed(KEY_LSHIFT) || keyboard_isKeyPressed(KEY_RSHIFT)) != keyboard_isKeyPressed(KEY_CAPS);
	key_status->altgr = keyboard_isKeyPressed(KEY_ALTGR);
	key_status->key = key;

	if(console->id != 0 && console->waitingThread != NULL)
	{
		thread_unblock(console->waitingThread);
		console->waitingThread = NULL;
	}
}

static void handler_keyUp(void *opaque, KEY_t key)
{
	console_t *console = *(console_t**)opaque;
}

static void clear(console_t *console, uint8_t mode)
{
	size_t start = 0;
	size_t end = console->width * console->height;
	switch(mode)
	{
		case 0:
			start = console->cursor.y * console->width + console->cursor.x;
		break;
		case 1:
			end = console->cursor.y * console->width + console->cursor.x;
		break;
		case 2:
			console_setCursor(console, (cursor_t){0, 0});
		break;
	}

	for(size_t i = start; i < end; i++)
	{
		((uint16_t*)console->screenBuffer)[i] = ' ' | (console->color << 8);
	}

	console->currentRow = 0;

	if(activeConsole == console && (console->flags & CONSOLE_AUTOREFRESH))
		display_refresh(console->screenBuffer, console->cursor.x, console->cursor.y);
}

static void clearLine(console_t *console, uint8_t mode)
{
	size_t start = 0;
	size_t end = console->width;
	switch(mode)
	{
		case 0:
			start = console->cursor.x;
		break;
		case 1:
			end = console->cursor.x;
		break;
		case 2:
			//Clear entire line but do not change cursor
		break;
	}

	for(size_t i = start; i < end; i++)
		((uint16_t*)console->screenBuffer)[console->cursor.y * console->width + i] = ' ' | (console->color << 8);

	console->currentRow = 0;

	if(activeConsole == console && (console->flags & CONSOLE_AUTOREFRESH))
		display_refresh(console->screenBuffer, console->cursor.x, console->cursor.y);
}

static void updateCursor(console_t *console)
{
	if(console == activeConsole && (console->flags & CONSOLE_AUTOREFRESH))
		setCursor(console->cursor.x, console->cursor.y);
}

void console_Init()
{
	initConsole.inputBufferSize = INPUT_BUFFER_SIZE;
	initConsole.inputBuffer = malloc(initConsole.inputBufferSize * sizeof(*initConsole.inputBuffer));

	//Aktuellen Bildschirminhalt in neuen Buffer kopieren
	initConsole.buffer = memcpy(malloc(MAX_PAGES * PAGE_SIZE), initConsole.buffer, PAGE_SIZE);
	initConsole.screenBuffer = initConsole.buffer + (MAX_PAGES - 1) * PAGE_SIZE;

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
	console_t *console = calloc(1, sizeof(console_t));
	if(console == NULL)
		return NULL;

	console->name = strdup(name);
	if(console->name == NULL)
	{
		free(console);
		return NULL;
	}

	console->buffer = calloc(MAX_PAGES, PAGE_SIZE);
	if(console->buffer == NULL)
	{
		free(console->name);
		free(console);
		return NULL;
	}
	console->screenBuffer = console->buffer + (MAX_PAGES - 1) * PAGE_SIZE;
	console->color = color;

	console->inputBufferSize = INPUT_BUFFER_SIZE;
	console->inputBufferStart = console->inputBufferEnd = 0;
	console->inputBuffer = malloc(console->inputBufferSize * sizeof(*initConsole.inputBuffer));
	if(console->inputBuffer == NULL)
	{
		free(console->name);
		free(console->buffer);
		free(console);
		return NULL;
	}

	console->outputBuffer = queue_create();
	if(console->outputBuffer == NULL)
	{
		free(console->inputBuffer);
		free(console->name);
		free(console->buffer);
		free(console);
		return NULL;
	}

	console->flags = CONSOLE_AUTOREFRESH | CONSOLE_AUTOSCROLL | CONSOLE_ECHO;
	console->height = ROWS;
	console->width = COLS;
	console->id = nextID++;

	clear(console, 2);
	unlock(&console->lock);

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

static esc_seq_status_t console_ansi_parse(console_t *console, const unsigned char *ansi_buf, uint8_t ansi_buf_len)
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
				if(handle_ansi_formatting(console, have_n1 ? n1 : 0) != SUCCESS)
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
				updateCursor(console);
				return SUCCESS;
			case 'B':
				if(!have_n1)
					n1 = 1;
				console->cursor.y = MIN(console->height, console->cursor.y + n1);
				updateCursor(console);
				return SUCCESS;
			case 'C':
				if(!have_n1)
					n1 = 1;
				console->cursor.x = MIN(console->width, console->cursor.x + n1);
				updateCursor(console);
				return SUCCESS;
			case 'D':
				if(!have_n1)
					n1 = 1;
				if(console->cursor.x > n1)
					console->cursor.x -= n1;
				updateCursor(console);
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

			case 'J':	//Clear console
				clear(console, have_n1 ? n1 : 0);
				return SUCCESS;
			case 'K':	//Clear line
				clearLine(console, have_n1 ? n1 : 0);
				return SUCCESS;
		}
	}

	return NEED_MORE;
}

void console_ansi_write(console_t *console, unsigned char c)
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

void console_write(console_t *console, unsigned char c)
{
	if(console != NULL)
	{
		uint8_t Farbwert = console->color;
		uint16_t *gs = (uint16_t*)GRAFIKSPEICHER;
		uint16_t *buffer = (uint16_t*)console->screenBuffer;
		console->currentRow = 0;
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
				buffer[console->cursor.y * COLS + console->cursor.x] = (' ' | (Farbwert << 8));	//Das vorhandene Zeichen "löschen"
				if(console == activeConsole && (console->flags & CONSOLE_AUTOREFRESH))
					gs[console->cursor.y * COLS + console->cursor.x] = (' ' | (Farbwert << 8));	//Screen updaten
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
		updateCursor(console);
	}
}

void displayConsole(console_t *console)
{
	if(console != NULL)
	{
		activeConsole = console;
		if(console->flags & CONSOLE_AUTOREFRESH)
			display_refresh(console->screenBuffer, console->cursor.x, console->cursor.y);
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
	console->currentRow = 0;
	scroll(console, -1, true);
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
		updateCursor(console);
	}
}

static void processKeyInput(console_t *console, char code)
{
	console->currentInputBufferSize += 3;
	console->currentInputBuffer = realloc(console->currentInputBuffer, console->currentInputBufferSize + 1);
	console->currentInputBuffer[console->currentInputBufferSize - 3] = '\e';
	console->currentInputBuffer[console->currentInputBufferSize - 2] = '[';
	console->currentInputBuffer[console->currentInputBufferSize - 1] = code;
	console->currentInputBuffer[console->currentInputBufferSize] = '\0';

	if(console->flags & CONSOLE_ECHO)
	{
		console_write(console, '^');
		console_write(console, '[');
		console_write(console, code);
	}

	if(console->flags & CONSOLE_RAW)
	{
		if(console->currentOutputBuffer == NULL)
			console->currentOutputBuffer = console->currentInputBuffer;
		else
			queue_enqueue(console->outputBuffer, console->currentInputBuffer);

		console->currentInputBuffer = NULL;
		console->currentInputBufferSize = 0;
	}
}

static void processAnsiInput(console_t *console, console_key_status_t *key_status)
{
	switch(key_status->key)
	{
		case KEY_UP:
			processKeyInput(console, 'A');
		break;
		case KEY_DOWN:
			processKeyInput(console, 'B');
		break;
		case KEY_RIGHT:
			processKeyInput(console, 'C');
		break;
		case KEY_LEFT:
			processKeyInput(console, 'D');
		break;
		default:
		break;
	}
}

static char processInput(console_t *console)
{
	assert(console->inputBufferStart != console->inputBufferEnd);
	console_key_status_t key_status = console->inputBuffer[console->inputBufferStart++];
	if(console->inputBufferStart == console->inputBufferSize)
		console->inputBufferStart = 0;

	char c = convertKeyToAscii(&key_status);

	if(c != 0 && (console->flags & CONSOLE_ECHO) && (console->currentInputBufferSize > 0 || c != '\b'))
		console_write(console, c);
	if(c == 0)
	{
		processAnsiInput(console, &key_status);
	}
	else if(!(console->flags & CONSOLE_RAW))
	{
		if(c == '\b')
		{
			if(console->currentInputBufferSize > 0)
				console->currentInputBuffer[--console->currentInputBufferSize] = '\0';
		}
		else
		{
			console->currentInputBuffer = realloc(console->currentInputBuffer, ++console->currentInputBufferSize + 1);
			console->currentInputBuffer[console->currentInputBufferSize - 1] = c;
			console->currentInputBuffer[console->currentInputBufferSize] = '\0';

			if(c == '\n')
			{
				queue_enqueue(console->outputBuffer, console->currentInputBuffer);
				console->currentInputBuffer = NULL;
				console->currentInputBufferSize = 0;
			}
		}
	}

	return c;
}

char console_getch(console_t *console)
{
	char c;
	if(console->currentOutputBuffer != NULL || queue_size(console->outputBuffer))
	{
		if(console->currentOutputBuffer == NULL)
			console->currentOutputBuffer = queue_dequeue(console->outputBuffer);

		c = console->currentOutputBuffer[console->currentOutputBufferPos++];
		if(console->currentOutputBuffer[console->currentOutputBufferPos] == '\0')
		{
			free(console->currentOutputBuffer);
			console->currentOutputBuffer = NULL;
			console->currentOutputBufferPos = 0;
		}

		if(console->inputBufferStart != console->inputBufferEnd)
			processInput(console);

		return c;
	}

	do
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

		c = processInput(console);
	}
	while(c != '\n' && !(console->flags & CONSOLE_RAW));

	return (console->flags & CONSOLE_RAW) && c != 0 ? c : console_getch(console);
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
	char *buf = buffer;

	size_t size;
	for(size = 0; size < length; size++)
	{
		buf[size] = console_getch(console);
	}

	return size;
}

static void *console_functionHandler(void *c, vfs_device_function_t function, ...)
{
	console_t *console = c;
	void *val = NULL;
	va_list arg;
	va_start(arg, function);

	switch(function)
	{
		case VFS_DEV_FUNC_TYPE:
			val = (void*)VFS_DEVICE_VIRTUAL;
		break;
		case VFS_DEV_FUNC_NAME:
			val = console->name;
		break;
		case VFS_DEV_FUNC_GET_ATTR:
			val = (void*)(uintptr_t)console->flags;
		break;
		case VFS_DEV_FUNC_SET_ATTR:
			console->flags = va_arg(arg, uint32_t);
		break;
		default:
			val = NULL;
	}

	va_end(arg);
	return val;
}

static vfs_device_capabilities_t console_getCapabilitiesHandler(void *c)
{
	console_t *console = c;

	return VFS_DEV_CAP_ATTRIBUTES;
}
