/*
 * syscalls.h
 *
 *  Created on: 17.07.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#ifndef SYSCALLS_H_
#define SYSCALLS_H_

//Alle Funktionen
//Speiherverwaltung
#define MALLOC	0
#define FREE	1

//Programmaufruf und Beendung
#define EXEC	10
#define EXIT	11

//Ein- und Ausgabe
#define GETCH	20
#define PUTCH	21
#define PIXEL	22
#define COLOR	23
#define CURSOR	24

//Dateifunktionen (TODO)
#define FOPEN	40
#define FCLOSE	41

//Verschiedene Funktionen
#define TIME	50
#define DATE	51

//Systemfunktionen
#define SYSINF	60

#endif /* SYSCALLS_H_ */

#endif
