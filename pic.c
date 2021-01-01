/*
 * PIC.c
 *
 *  Created on: 14.07.2012
 *      Author: pascal
 */

#include "pic.h"
#include "util.h"
#include "display.h"

//Master
#define PIC_MASTER_COMMAND	0x20
#define PIC_MASTER_DATA		0x21
#define PIC_MASTER_IMR		0x21

//Slave
#define PIC_SLAVE_COMMAND	0xA0
#define PIC_SLAVE_DATA		0xA1
#define PIC_SLAVE_IMR		0xA1

//EOI
#define EOI					0x20

/*
 * Intitialisiert den PIC
 */
void pic_Init()
{
	//IRQs fangen bei Interrupt 32 an
	pic_Remap(32);

	//Maskiere alle nicht gebrauchten IRQs (alle ausser PS/2 Port 1 und 2)
	pic_MaskIRQ(0x0);
	SysLog("PIC", "Initialisierung abgeschlossen");
}

/*
 * Initialisiert den PIC auf eine bestimmte Interruptnummer
 * Parameter:	Interrupt = Nummer des Interrupts, an den der IRQ 0 sein soll (vielfaches von 8)
 */
void pic_Remap(uint8_t Interrupt)
{
	//Master PIC
	outb(PIC_MASTER_COMMAND, 0x11);		//0x11 bedeuted, dass wir Initialisieren wollen und es
										//zwei PICs gibt
	outb(PIC_MASTER_DATA, Interrupt);	//Interruptnummer senden
	outb(PIC_MASTER_DATA, 0x04);		//Interruptnummer des Slaves übergeben (Bit-Maske)
	outb(PIC_MASTER_DATA, 0x01);		//Wir befinden uns im 8086-Mode

	//Slave PIC
	outb(PIC_SLAVE_COMMAND, 0x11);		//0x11 bedeuted, dass wir Initialisieren wollen und es
										//zwei PICs gibt
	outb(PIC_SLAVE_DATA, Interrupt + 8);//Interruptnummer senden
	outb(PIC_SLAVE_DATA, 2);			//Interruptnummer des Slaves übergeben
	outb(PIC_SLAVE_DATA, 0x01);		//Wir befinden uns im 8086-Mode
}

/*
 * Maskiert einzelne IRQs (deaktiviert sie)
 * Parameter:	Mask = Bitmaske (0 = entsprechender IRQ aktiviert)
 */
void pic_MaskIRQ(uint16_t Mask)
{
	outb(PIC_MASTER_IMR, (uint8_t)Mask);
	outb(PIC_SLAVE_IMR, (uint8_t)Mask >> 8);
}

/*
 * Sendet den EOI (End of Interrupt) an den PIC
 * Parameter:	irq = IRQ-Nummer
 */
void pic_SendEOI(uint8_t irq)
{
	outb(PIC_MASTER_COMMAND, EOI);
	if(irq > 7)
		outb(PIC_SLAVE_COMMAND, EOI);
}
