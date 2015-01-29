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
#include "pm.h"

typedef struct{
	uint8_t x, y;
}cursor_t;

typedef struct{
	pid_t pid;
	uint8_t page;

	void *buffer;
	uint8_t color;
	cursor_t cursor;
}console_t;

extern console_t initConsole;

void console_Init();
console_t *console_create(uint8_t page);
void console_write(console_t *console, char c);
void console_clear(console_t *console);
void console_switch(uint8_t page);
void console_changeColor(console_t *console, uint8_t color);
void console_setCursor(console_t *console, cursor_t cursor);

#endif /* CONSOLE_H_ */
