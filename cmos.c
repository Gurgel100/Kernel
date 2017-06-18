/*
 * cmos.c
 *
 *  Created on: 30.06.2013
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "cmos.h"
#include "util.h"

#define CMOS_ADDRESS	0x70
#define CMOS_DATA		0x71

#define SECOND		0x00	//Sekunde (BCD)
#define ALARM_SEC	0x01	//Alarmsekunde (BCD)
#define MINUTE		0x02	//Minute (BCD)
#define ALARM_MIN	0x03	//Alarmminute (BCD)
#define HOUR		0x04	//Stunde (BCD)
#define ALARM_H		0x05	//Alarmstunde (BCD)
#define DAY_OF_WEEK	0x06	//Tag der Woche (BCD)
#define DAY_OF_M	0x07	//Tag des Monats (BCD)
#define MONTH		0x08	//Monat (BCD)
#define YEAR		0x09	//Jahr (letzte zwei Stellen) (BCD)

//Statusregister
#define STATUS_A	0x0A
#define STATUS_B	0x0B
#define STATUS_C	0x0C	//Schreibgeschützt
#define STATUS_D	0x0D	//Schreibgeschützt

#define POST		0x0E	//POST-Diagnosestatusbyte
#define SHUTDOWN	0x0F	//Shutdown-Statusbyte
#define FDD			0x10	//Typ der Diskettenlaufwerke
//#define STATUS_A	0x11	//Reserviert
#define HDD			0x12	//Typ der Festplattenlaufwerke
//#define STATUS_A	0x13	//Reserviert
#define DEVBYTE		0x14	//Gerätebyte
#define SBASLOW		0x15	//Grösse des Basisspeichers in kB (niederwertiges Byte)
#define SBASHI		0x16	//Grösse des Basisspeichers in kB (höherwertiges Byte)
#define SERWLOW		0x17	//Grösse des Erweiterungsspeichers in kB (niederwertiges Byte)
#define SERWHI		0x18	//Grösse des Erweiterungsspeichers in kB (höherwertiges Byte)
#define ERWB1HDD	0x19	//Erweiterungsbyte 1.Festplatte
#define ERWB2HDD	0x1A	//Erweiterungsbyte 2.Festplatte
//#define STATUS_A	0x1B	//Reserviert / vom BIOS abhängig
#define CMOSCRCL	0x2E	//CMOS-Prüfsumme (höherwertiges Byte)
#define CMOSCRCH	0x2F	//CMOS-Prüfsumme (niederwertiges Byte)
#define ERWMEML		0x30	//Erweiterter Speicher (niederwertiges Byte)
#define ERWMEMH		0x31	//Erweiterter Speicher (höherwertiges Byte)
#define JHR			0x32	//Jahrhundert (BCD)
//#define STATUS_A	0x33 - 0x3F	//Reserviert / vom BIOS abhängig

#define CURRENT_CENTURY	2000

uint8_t Read(uint8_t Offset);
void Write(uint8_t Offset, uint8_t Data);

void cmos_Init()
{
	Date_t Date;
	Time_t Time;
	cmos_GetTime(&Time);
	cmos_GetDate(&Date);
}

/*
 * Liest ein Byte aus einem CMOS-Register
 * Parameter:	Offset = Offset bzw. das Register
 * Rückgabewert: Gelesener Wert
 */
uint8_t Read(uint8_t Offset)
{
	uint8_t tmp = inb(CMOS_ADDRESS);
	outb(CMOS_ADDRESS, (tmp & 0x80) | (Offset & 0x7F));	//Oberstes Bit darf nicht verändert werden, untere 7 Bits dienen als Offset
	return inb(CMOS_DATA);
}

/*
 * Liest ein Byte aus einem CMOS-Register
 * Parameter:	Offset = Offset bzw. das Register
 * 				Data = Zu schreibender Wert
 */
void Write(uint8_t Offset, uint8_t Data)
{
	uint8_t tmp = inb(CMOS_ADDRESS);
	outb(CMOS_ADDRESS, (tmp & 0x80) | (Offset & 0x7F));	//Oberstes Bit darf nicht verändert werden, untere 7 Bits dienen als Offset
	outb(CMOS_DATA, Data);
}

Time_t *cmos_GetTime(Time_t *Time)
{
	uint8_t tmp = Read(SECOND);
	Time->Second = (tmp & 0xF) + ((tmp >> 4) & 0xF) * 10;
	tmp = Read(MINUTE);
	Time->Minute = (tmp & 0xF) + ((tmp >> 4) & 0xF) * 10;
	tmp = Read(HOUR);
	Time->Hour = (tmp & 0xF) + ((tmp >> 4) & 0xF) * 10;
	return Time;
}

Date_t *cmos_GetDate(Date_t *Date)
{
	uint8_t tmp = Read(DAY_OF_WEEK);
	Date->DayOfWeek = (tmp & 0xF) + ((tmp >> 4) & 0xF) * 10;
	tmp = Read(DAY_OF_M);
	Date->DayOfMonth = (tmp & 0xF) + ((tmp >> 4) & 0xF) * 10;
	tmp = Read(MONTH);
	Date->Month = (tmp & 0xF) + ((tmp >> 4) & 0xF) * 10;
	tmp = Read(YEAR);
	Date->Year = (tmp & 0xF) + ((tmp >> 4) & 0xF) * 10;
	return Date;
}

void cmos_Reboot()
{
	Write(SHUTDOWN, 0x2);
}

time_t cmos_syscall_timestamp()
{
	struct tm t;
	Date_t date;
	Time_t time;

	cmos_GetDate(&date);
	cmos_GetTime(&time);

	t.tm_year = CURRENT_CENTURY + date.Year - 1900;
	t.tm_mon = date.Month - 1;
	t.tm_mday = date.DayOfMonth;

	t.tm_hour = time.Hour;
	t.tm_min = time.Minute;
	t.tm_sec = time.Second;

	return mktime(&t);
}

#endif
