/*
 * cmos.h
 *
 *  Created on: 30.06.2013
 *      Author: pascal
 */

#ifndef CMOS_H_
#define CMOS_H_

#include "stdint.h"
#include <time.h>

typedef struct{
		uint8_t Second, Minute, Hour;
} Time_t;

typedef struct{
		uint8_t DayOfWeek, DayOfMonth, Month, Year;
} Date_t;

void cmos_Init(void);
Time_t *cmos_GetTime(Time_t *Time);
Date_t *cmos_GetDate(Date_t *Date);

void cmos_Reboot(void);

//Syscalls
time_t cmos_syscall_timestamp();

#endif /* CMOS_H_ */
