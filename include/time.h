/*
 * time.h
 *
 *  Created on: 18.07.2015
 *      Author: pascal
 */

#ifndef TIME_H_
#define TIME_H_

#include <bits/types.h>

#define CLOCKS_PER_SEC	1000000l

typedef _size_t		size_t;
typedef _clock_t	clock_t;
typedef _time_t		time_t;

struct tm{
	int tm_sec;		//Seconds [0,60]
	int tm_min;		//Minutes [0,59]
	int tm_hour;	//Hour [0,23]
	int tm_mday;	//Day of month [1,31]
	int tm_mon;		//Month of year [0,11]
	int tm_year;	//Years since 1900
	int tm_wday;	//Day of week [0,6] (Sunday =0)
	int tm_yday;	//Day of year [0,365]
	int tm_isdst;	//Daylight Savings flag
};

struct timespec{
	time_t	tv_sec;		//Seconds.
	long	tv_nsec;	//Nanoseconds.
};

extern double difftime(time_t time_end, time_t time_beg);
extern time_t time(time_t *res);
extern clock_t clock();

extern struct tm *gmtime(const time_t *time);
extern time_t mktime(struct tm *time);

extern char *asctime(const struct tm *time);
extern size_t strftime(char *restrict str, size_t count, const char *restrict format, const struct tm *restrict time);

#endif /* TIME_H_ */
