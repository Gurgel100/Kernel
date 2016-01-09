/*
 * misc.c
 *
 *  Created on: 30.07.2013
 *      Author: pascal
 */

#include "misc.h"
#include "isr.h"

typedef struct{
		uint8_t IRQ;
		void (*Handler)(struct cdi_device*);
		struct cdi_device *Device;
}Handler_t;

cdi_list_t IRQHandlers;
uint64_t IRQCount[NUM_IRQ];

void cdi_irq_handler(uint8_t irq)
{
	IRQCount[irq]++;
	size_t Size = cdi_list_size(IRQHandlers);
	if(Size)
	{
		Handler_t *Handler;
		size_t i;
		for(i = 0; i < Size; i++)
		{
			Handler = cdi_list_get(IRQHandlers, i);
			if(Handler && Handler->IRQ == irq)
				Handler->Handler(Handler->Device);
		}
	}
}

/**
 * Registiert einen neuen IRQ-Handler.
 *
 * @param irq Nummer des zu reservierenden IRQ
 * @param handler Handlerfunktion
 * @param device Geraet, das dem Handler als Parameter uebergeben werden soll
 */
void cdi_register_irq(uint8_t irq, void (*handler)(struct cdi_device*), struct cdi_device* device)
{
	if(irq > NUM_IRQ)
		return;

	Handler_t *Handler;
	Handler = malloc(sizeof(Handler_t));
	*Handler = (Handler_t){
			.Handler = handler,
			.Device = device
	};
	cdi_list_push(IRQHandlers, Handler);
}

/**
 * Setzt den IRQ-Zaehler fuer cdi_wait_irq zurueck.
 *
 * @param irq Nummer des IRQ
 *uint64_t IRQCount[NUM_IRQ];
 * @return 0 bei Erfolg, -1 im Fehlerfall
 */
int cdi_reset_wait_irq(uint8_t irq)
{
	if(irq > NUM_IRQ)
		return -1;
	IRQCount[irq] = 0;
	return 0;
}

/**
 * Wartet bis der IRQ aufgerufen wurde. Der interne Zähler muss zuerst mit
 * cdi_reset_wait_irq zurückgesetzt werden. Damit auch die IRQs abgefangen
 * werden können, die kurz vor dem Aufruf von dieser Funktion aufgerufen
 * werden, sieht der korrekte Ablauf wie folgt aus:
 *
 * -# cdi_reset_wait_irq
 * -# Hardware ansprechen und Aktionen ausführen, die schließlich den IRQ
 *    auslösen
 * -# cdi_wait_irq
 *
 * Der entsprechende IRQ muss zuvor mit cdi_register_irq registriert worden
 * sein. Der registrierte Handler wird ausgeführt, bevor diese Funktion
 * erfolgreich zurückkehrt.
 *
 * @param irq       Nummer des IRQ auf den gewartet werden soll
 * @param timeout   Anzahl der Millisekunden, die maximal gewartet werden sollen
 *
 * @return 0 wenn der irq aufgerufen wurde, -1 sonst.
 */
int cdi_wait_irq(uint8_t irq, uint32_t timeout)
{
	uint64_t Time = 0;
	if(irq > NUM_IRQ)
		return -1;

	while(!IRQCount[irq])
	{
		cdi_sleep_ms(10);
		Time += 10;
		if(Time > timeout)
			return -1;
	}
	return 0;
}

//TODO: Eventuell ist es besser die Ports auch zu verwalten

/**
 * Reserviert IO-Ports
 *
 * @return 0 wenn die Ports erfolgreich reserviert wurden, -1 sonst.
 */
int cdi_ioports_alloc(uint16_t start, uint16_t count)
{
	return 0;
}

/**
 * Gibt reservierte IO-Ports frei
 *
 * @return 0 wenn die Ports erfolgreich freigegeben wurden, -1 sonst.
 */
int cdi_ioports_free(uint16_t start, uint16_t count)
{
	return 0;
}

/**
 * Unterbricht die Ausfuehrung fuer mehrere Millisekunden
 */
void cdi_sleep_ms(uint32_t ms)
{
	Sleep((uint64_t)ms);
}
