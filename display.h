/*
 * display.h
 *
 *  Created on: 07.07.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "stdint.h"
#include "stddef.h"

//Hintergrundfarben
#define BG_BLACK			0x00
#define BG_BLUE				0x10
#define BG_GREEN			0x20
#define BG_CYAN				0x30
#define BG_RED				0x40
#define BG_MAGENTA			0x50
#define BG_BRAUN			0x60
#define BG_LIGHT_GREY		0x70

//Textfarben
#define CL_BLACK			0x0	//Schwarz
#define CL_BLUE				0x1	//Blau
#define CL_GREEN			0x2	//Grün
#define CL_CYAN				0x3	//Cyan
#define CL_RED				0x4	//Rot
#define CL_MAGENTA			0x5	//Magenta
#define CL_BRAUN			0x6	//Braun
#define CL_LIGHT_GREY		0x7	//Hellgrau
#define CL_DARK_GREY		0x8	//Dunkelgrau
#define CL_LIGHT_BLUE		0x9	//Hellblau
#define CL_LIGHT_GREEN		0xA	//Hellgrün
#define CL_LIGHT_CYAN		0xB	//Hellcyan
#define CL_LIGHT_RED		0xC	//Hellrot
#define CL_LIGHT_MAGENTA	0xD	//Hellmagenta
#define CL_YELLOW			0xE	//Gelb
#define CL_WHITE			0xF	//Weiss

#define DISPLAY_ROWS		25
#define DISPLAY_COLS		80

extern uint8_t Spalte, Zeile;

void Display_Init(void);
void display_refresh(const uint16_t *buffer, uint8_t cursor_x, uint8_t cursor_y);
void setColor(uint8_t Color);
void putch(unsigned char c);
void setCursor(uint8_t x, uint8_t y);
void SysLog(char *Device, char *Text);
void SysLogError(char *Device, char *Text);
void Display_Clear(void);
void Panic(char *Device, char *Text) __attribute__((noreturn));
void hideCursor(void);
void showCursor(void);

size_t Display_FileHandler(char *name, uint64_t start, size_t length, const void *buffer);

#endif /* DISPLAY_H_ */

#endif
