/*
 * console.h
 *
 *  Created on: 27.01.2015
 *      Author: pascal
 */

#ifndef CONSOLE_H_
#define CONSOLE_H_

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "lock.h"
#include "queue.h"
#include "keyboard.h"

#define	CONSOLE_AUTOREFRESH	1
#define CONSOLE_AUTOSCROLL	2
#define CONSOLE_RAW			(1 << 2)
#define CONSOLE_ECHO		(1 << 3)

typedef struct{
	uint8_t x, y;
}cursor_t;

typedef struct{
	KEY_t key;
	bool shift, altgr;
}console_key_status_t;

typedef struct{
	uint8_t id;
	char *name;
	uint8_t flags;

	void* waitingThread;

	lock_t lock;

	void *buffer;
	uint16_t height, width;
	uint8_t color;
	cursor_t cursor;

	char ansi_buf[16];
	uint8_t ansi_buf_ofs;
	cursor_t saved_cursor;

	console_key_status_t *inputBuffer;
	size_t inputBufferSize, inputBufferStart, inputBufferEnd;
	char *currentInputBuffer;
	size_t currentInputBufferSize;

	queue_t *outputBuffer;
	char *currentOutputBuffer;
	size_t currentOutputBufferPos;
}console_t;

extern console_t initConsole;

void console_Init();
console_t *console_create(char *name, uint8_t color);
console_t *console_getByName(char *name);
void console_ansi_write(console_t *console, char c);
void console_write(console_t *console, char c);
void console_switch(uint8_t page);
void console_changeColor(console_t *console, uint8_t color);
void console_setCursor(console_t *console, cursor_t cursor);

#endif /* CONSOLE_H_ */
