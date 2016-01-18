/*
 * keyboard.h
 *
 *  Created on: 15.07.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#ifndef KEYBOARD_H_
#define KEYBOARD_H_

#include "stdbool.h"

typedef enum{
	__KEY_INVALID,	//Ungültige Taste
	KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
	KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
	KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
	KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11,
	KEY_F12,
	KEY_PRINT, KEY_SCROLL, KEY_PAUSE,
	KEY_TAB, KEY_CAPS, KEY_LSHIFT, KEY_LCTRL, KEY_LGUI, KEY_LALT, KEY_RSHIFT, KEY_RCTRL,
	KEY_RGUI, KEY_SPACE, KEY_ALTGR, KEY_MENU, KEY_ENTER, KEY_BACK, KEY_ESC,
	KEY_INS, KEY_DEL, KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDOWN,
	KEY_UP, KEY_DOWN, KEY_RIGHT, KEY_LEFT,
	KEY_AUML, KEY_OUML, KEY_UUML, KEY_DP, KEY_DOL,
	KEY_ST, KEY_FRAG, KEY_DACH,
	KEY_BIGGER, KEY_COMMA, KEY_DOT, KEY_MIN,
	//Keypad-Tasten
	KEY_KPNUM, KEY_KPSLASH, KEY_KPMUL, KEY_KPMIN, KEY_KPPLUS, KEY_KPENTER, KEY_KPDOT,
	KEY_KP0, KEY_KP1, KEY_KP2, KEY_KP3, KEY_KP4, KEY_KP5, KEY_KP6, KEY_KP7, KEY_KP8, KEY_KP9,
	__KEY_LAST
}KEY_t;	//Alle Tasten

/*
 *	Parameter:	Buchstabe der Taste
 */
typedef void(*char_handler)(void *opaque, char c);

/*
 *	Parameter:	Keycode der  Taste
 */
typedef void(*key_handler)(void *opaque, KEY_t key);


void keyboard_Init(void);

/*
 * Registriert einen Handler beim Tastaturtreiber, der aufgerufen wird, sobald eine Taste gedrückt wurde
 * Parameter:	Der aufzurufende Handler
 */
void keyboard_registerCharHandler(char_handler handler, void *opaque);

/*
 * Registriert einen Handler beim Tastaturtreiber, der aufgerufen wird, sobald eine Taste gedrückt wurde
 * Parameter:	Der aufzurufende Handler
 */
void keyboard_registerKeydownHandler(key_handler handler, void *opaque);

/*
 * Registriert einen Handler beim Tastaturtreiber, der aufgerufen wird, sobald eine Taste losgelassen wurde
 * Parameter:	Der aufzurufende Handler
 */
void keyboard_registerKeyupHandler(key_handler handler, void *opaque);

/*
 * Gibt zurück, ob die Taste aktuell gedrückt ist.
 * Parameter:	Die Taste, deren Status man haben will
 * Rückgabe:	Status des Taste
 */
bool keyboard_isKeyPressed(KEY_t key);

char getch(void);

#endif /* KEYBOARD_H_ */

#endif
