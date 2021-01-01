/*
 * keyboard.c
 *
 *  Created on: 15.07.2012
 *      Author: pascal
 */

#include "keyboard.h"
#include "isr.h"
#include "stdint.h"
#include "stddef.h"
#include "util.h"
#include "display.h"
#include "console.h"
#include "ctype.h"
#include "stdlib.h"
#include "list.h"
#include "assert.h"
#include <dispatcher.h>

//Ports
#define KEYBOARD_PORT		0x60
#define KBC_COMMAND			0X64
#define KBC_BUFFER			0x60

//Statusregister
#define STATUS_OUT			0x1		//Status des Ausgabepuffers 0 = leer
#define STATUS_IN			0x2		//Status des Eingabepuffers 0 = leer
#define STATUS_SELF_TEST	0x4		//1 = Erfolgreicher Selbstest (sollte immer 1 sein)

//Outputport
#define KBC_RD_OUTPUT		0xD0	//Damit liest man den Outputport aus
#define KBC_WR_OUTPUT		0xD1	//Damit schreibt man in den Outputport
#define OUTPUT_CPU_RESET	0x1
#define OUTPUT_A20_GATE		0x2

//Controller Command Byte (CCB)
#define KBC_RD_CCB			0x20	//Befehl zum Lesen des CCD
#define KBC_WR_CCB			0x60	//Befehl zum Schreiben des CCD
#define CCB_IRQ1			0x1		//1 = Erzeuge einen IRQ1 wenn PSAUX0 Daten ausgibt
#define CCB_IRQ12			0x2		//1 = Erzeuge einen IRQ12 wenn PSAUX1 Daten ausgibt

//KBC-Befehle
#define KBC_SELF_TEST		0xAA	//Tastatur-Selftest. Sollte 0x55 auf Port 0x60 zurückgeben.
#define KBC_TEST_CONNECT	0xAB	//Testen des Tastaturanschlusses. Byte auf Port 0x60
	#define KBC_TEST_NOERR	0X00
	#define KBC_TEST_CL_L	0X01
	#define KBC_TEST_CL_H	0X02
	#define KBC_TEST_DAT_L	0X03
	#define KBC_TEST_DAT_H	0X04
	#define KBC_TEST_ERROR	0XFF
#define KBC_AKTIVATE		0xAE	//Aktiviert die Tastatur
#define KBC_DEAKTIVATE		0xAD	//Deaktiviert die Tastatur

//Tastaturbefehle
#define KEYBOARD_LED		0xED	//Setzt die LEDs
#define KEYBOARD_AKTIVATE	0xFA	//Aktiviert die Tastatur

typedef struct{
		uint8_t Char;
		void *Next;
} Puffer_t;

typedef struct{
	key_handler handler;
	void *opaque;
}key_handler_entry_t;

//Tabellen um den Scancode in einen Keycode zu übersetzen
static const KEY_t ScancodeToKey_default[] =
//		.0			.1			.2			.3			.4			.5			.6			.7			.8			.9			.A			.B			.C			.D			.E			.F
{
		0,			KEY_ESC,	KEY_1,		KEY_2,		KEY_3,		KEY_4,		KEY_5,		KEY_6,		KEY_7,		KEY_8,		KEY_9,		KEY_0,		KEY_FRAG,	KEY_DACH,	KEY_BACK,	KEY_TAB,	//0.
		KEY_Q,		KEY_W,		KEY_E,		KEY_R,		KEY_T,		KEY_Z,		KEY_U,		KEY_I,		KEY_O,		KEY_P,		KEY_UUML,	KEY_DP,		KEY_ENTER,	KEY_LCTRL,	KEY_A,		KEY_S,		//1.
		KEY_D,		KEY_F,		KEY_G,		KEY_H,		KEY_J,		KEY_K,		KEY_L,		KEY_OUML,	KEY_AUML,	KEY_DOL,	KEY_LSHIFT,	KEY_BIGGER,	KEY_Y,		KEY_X,		KEY_C,		KEY_V,		//2.
		KEY_B,		KEY_N,		KEY_M,		KEY_COMMA,	KEY_DOT,	KEY_MIN,	KEY_RSHIFT,	KEY_KPMUL,	KEY_LALT,	KEY_SPACE,	KEY_CAPS,	KEY_F1,		KEY_F2,		KEY_F3,		KEY_F4,		KEY_F5,		//3.
		KEY_F6,		KEY_F7,		KEY_F8,		KEY_F9,		KEY_F10,	KEY_KPNUM,	KEY_SCROLL,	KEY_KP7,	KEY_KP8,	KEY_KP9,	KEY_KPMIN,	KEY_KP4,	KEY_KP5,	KEY_KP6,	KEY_KPPLUS,	KEY_KP1,	//4.
		KEY_KP2,	KEY_KP3,	KEY_KP0,	KEY_KPDOT,	0,			0,			0,			KEY_F11,	KEY_F12,	0,			0,			0,			0,			0,			0,			0,			//5.
		0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			//6.
		0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			//7.

};

static const KEY_t ScancodeToKey_extended[] =
//		.0			.1			.2			.3			.4			.5			.6			.7			.8			.9			.A			.B			.C			.D			.E			.F
{
/*0.*/	0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,
/*1.*/	0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			KEY_KPENTER,KEY_RCTRL,	0,			0,
/*2.*/	0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,
/*3.*/	0,			0,			0,			0,			0,			KEY_KPSLASH,0,			0,			KEY_ALTGR,	0,			0,			0,			0,			0,			0,			0,
/*4.*/	0,			0,			0,			0,			0,			0,			0,			KEY_HOME,	KEY_UP,		KEY_PGUP,	0,			KEY_LEFT,	0,			KEY_RIGHT,	0,			KEY_END,
/*5.*/	KEY_DOWN,	KEY_PGDOWN,	KEY_INS,	KEY_DEL,	0,			0,			0,			0,			0,			0,			0,			KEY_LGUI,	KEY_RGUI,	KEY_MENU,	0,			0,
/*6.*/	0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,
/*7.*/	0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,			0,
};


static bool PressedKeys[__KEY_LAST];
//static char volatile Puffer;
static volatile Puffer_t *Puffer;		//Anfang des Zeichenpuffers
static volatile Puffer_t *ActualPuffer;	//Ende des Zeichenpuffers

//Handlers
static list_t keydown_handlers;
static list_t keyup_handlers;


/*
 * Wandelt den Scancode in eine Taste um
 * Parameter:	Scancode = der umzuwandelne Scancode
 * Rückgabe:	Die entsprechende Taste
 */
static KEY_t keyboard_ScancodeToKey(uint8_t Scancode, uint8_t set)
{
	switch(set)
	{
		case 0:
			return ScancodeToKey_default[Scancode & 0x7F];
		case 1:
			return ScancodeToKey_extended[Scancode & 0x7F];
		default:
			return __KEY_INVALID;
	}
}

/*
 * Sendet einen Befehl an die Tastatur
 * Parameter:	Command = Befehl, der an die Tastatur geschickt werden soll
 */
static void keyboard_SendCommand(uint8_t Command)
{
	while(inb(KBC_COMMAND) & STATUS_IN);
	outb(KEYBOARD_PORT, Command);
}

/*
 * Holt die Ausgabe der Tastatur, die sie in Port 0x60 geschrieben hat
 * Rückgabe:	Keycode
 */
static uint8_t keyboard_getScanCode()
{
	if(inb(KBC_COMMAND) | STATUS_OUT) return inb(KBC_BUFFER);
	return 0;
}

static void keyboard_SetLEDs()
{
	keyboard_SendCommand(KEYBOARD_LED);
	while(inb(KBC_COMMAND) & STATUS_IN);
	outb(KEYBOARD_PORT, 0x0 | ((PressedKeys[KEY_CAPS] & 0x1) << 2) | ((PressedKeys[KEY_KPNUM] & 0x1) << 1)
			| (PressedKeys[KEY_SCROLL] & 0x1));
}

static void notify_keyDown(void *k)
{
	KEY_t key = (KEY_t)k;
	size_t i = 0;
	key_handler_entry_t *e;
	while((e = list_get(keydown_handlers, i++)))
	{
		e->handler(e->opaque, key);
	}
}

static void notify_keyUp(void *k)
{
	KEY_t key = (KEY_t)k;
	size_t i = 0;
	key_handler_entry_t *e;
	while((e = list_get(keyup_handlers, i++)))
	{
		e->handler(e->opaque, key);
	}
}

/*
 * Initialisiert die Tastatur und den KBC (Keyboard Controller)
 */
void keyboard_Init()
{
	//Leeren des Ausgabepuffers
	while(inb(KBC_COMMAND) & STATUS_OUT)//Solange auslesen, bis der Puffer leer ist
	{
		inb(KBC_BUFFER);
	}
	Puffer = ActualPuffer = NULL;

	//Handlerlisten initialisieren
	keydown_handlers = list_create();
	keyup_handlers = list_create();

	//Tastatur aktivieren
	//keyboard_SendCommand(KEYBOARD_AKTIVATE);

	//NumLock aktivieren
	PressedKeys[KEY_KPNUM] = true;
	PressedKeys[KEY_CAPS] = false;
	PressedKeys[KEY_SCROLL] = false;
	keyboard_SetLEDs();
	SysLog("Tastatur", "Initialisierung abgeschlossen");
}

/*
 * Registriert einen Handler beim Tastaturtreiber, der aufgerufen wird, sobald eine Taste gedrückt wurde
 * Parameter:	Der aufzurufende Handler
 */
void keyboard_registerKeydownHandler(key_handler handler, void *opaque)
{
	assert(keydown_handlers != NULL);
	key_handler_entry_t *e = malloc(sizeof(key_handler_entry_t));
	e->handler = handler;
	e->opaque = opaque;

	list_push(keydown_handlers, e);
}

/*
 * Registriert einen Handler beim Tastaturtreiber, der aufgerufen wird, sobald eine Taste losgelassen wurde
 * Parameter:	Der aufzurufende Handler
 */
void keyboard_registerKeyupHandler(key_handler handler, void *opaque)
{
	assert(keyup_handlers != NULL);
	key_handler_entry_t *e = malloc(sizeof(key_handler_entry_t));
	e->handler = handler;
	e->opaque = opaque;

	list_push(keyup_handlers, e);
}

bool keyboard_isKeyPressed(KEY_t key)
{
	return PressedKeys[key];
}

/*
 * Handler für IRQ 1
 * Parameter:	*ihs = Zeiger auf Struktur des Stacks
 */
void keyboard_Handler(ihs_t *ihs)
{
	static bool e0_code = false;
	uint8_t Scancode = keyboard_getScanCode();

	if(Scancode == 0xE0)
	{
		e0_code = true;
	}
	else
	{
		KEY_t key;
		//Ignore fake shift
		if(e0_code && (Scancode == 0x2A || Scancode == 0x36))
		{
			e0_code = false;
			return;
		}
		key = keyboard_ScancodeToKey(Scancode, e0_code);
		e0_code = false;
		if(key == __KEY_INVALID) return;
		bool make = !(Scancode & 0x80);			//Make = Taste gedrückt; Break = Taste losgelassen
		//Wenn CapsLock, NumLock oder ScrollLock gedrückt wurde, dann den Status nur ändern beim erneuten Drücken
		//und nicht beim Break.
		if(key == KEY_CAPS || key == KEY_KPNUM || key == KEY_SCROLL)
		{
			if(make)
			{
				PressedKeys[key] = !PressedKeys[key];
				//printf(" new Status: %u\n", PressedKeys[Key]);
				keyboard_SetLEDs();
				return;
			}
		}
		else
		{
			PressedKeys[key] = make;
		}

		if(make)
			dispatcher_enqueue(notify_keyDown, (void*)key);
		else
			dispatcher_enqueue(notify_keyUp, (void*)key);
	}
}
