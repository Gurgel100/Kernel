/*
 * elf.h
 *
 *  Created on: 25.06.2012
 *      Author: pascal
 *
 *  Nach dem Tutorial "http://www.lowlevel.eu/wiki/ELF-Tutorial"
 */

#ifndef ELF_H_
#define ELF_H_

#include "stdint.h"

//Funktionen
char elfLoad(void *Datei, uint16_t Segment);	//Par.: Datei = Addresse der Datei im Speicher; Segment = Segment in das kopiert werden soll (GDT)

#endif /* ELF_H_ */
