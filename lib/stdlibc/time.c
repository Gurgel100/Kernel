/*
 * time.c
 *
 *  Created on: 06.06.2017
 *      Author: pascal
 */

#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stddef.h>
#ifdef BUILD_KERNEL
#include "cmos.h"
#else
#include <syscall.h>
#endif

#define START_C_YEAR	1900
#define START_YEAR		1970

#define SECS_PER_MIN	60
#define MINS_PER_HOUR	60
#define HOURS_PER_DAY	24
#define DAYS_PER_YEAR	365
#define DAYS_PER_LEAP	(DAYS_PER_YEAR + 1)

#define SECS_PER_HOUR	(SECS_PER_MIN * MINS_PER_HOUR)
#define SECS_PER_DAY	(SECS_PER_HOUR * HOURS_PER_DAY)

static bool isLeapYear(int64_t year)
{
	return (year % 4 == 0) && (!(year % 100 == 0) || (year % 400 == 0));
}

static time_t getYearSeconds(uint64_t year)
{
	time_t secs = 0;
	for(uint64_t i = START_YEAR; i < year; i++)
	{
		if(isLeapYear(i))
			secs += DAYS_PER_LEAP * SECS_PER_DAY;
		else
			secs += DAYS_PER_YEAR * SECS_PER_DAY;
	}
	return secs;
}

static uint16_t getDaysOfMonth(uint64_t year, uint8_t month)
{
	assert(month > 0 && month <= 12);
	switch(month)
	{
		case 2:
			return isLeapYear(year) ? 29 : 28;
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
			return 31;
		default:
			return 30;
	}
}

static time_t getMonthSeconds(uint64_t year, uint8_t month)
{
	assert(month > 0 && month <= 12);
	time_t secs = 0;
	for(uint8_t i = 1; i < month; i++)
	{
		secs += getDaysOfMonth(year, i) * SECS_PER_DAY;
	}
	return secs;
}

static void calcWDays(struct tm *time)
{
	uint64_t year = time->tm_year + START_C_YEAR;
	uint8_t month = time->tm_mon + 1;

	//Use Schwerdtfeger's method to calculate day of week
	static uint8_t e_vals[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	static uint8_t f_vals[] = {0, 5, 3, 1};
	uint64_t c;
	uint8_t g;
	if(month >= 3)
	{
		c = year / 100;
		g = year % 100;
	}
	else
	{
		c = (year - 1) / 100;
		g = (year - 1) % 100;
	}
	time->tm_wday = (time->tm_mday + e_vals[month - 1] + f_vals[c % 4] + g + g / 4) % 7;
}

static void calcYDays(struct tm *time)
{
	time->tm_yday = 0;
	for(int i = 0; i < time->tm_mon; i++)
	{
		time->tm_yday += getDaysOfMonth(time->tm_year + START_C_YEAR, i + 1);
	}
	time->tm_yday += time->tm_mday - 1;
}

/**
 * \brief Checks if str has enough place to hold the formatted string and if yes writes the formatted string to str
 *
 * \param str Target buffer to write to
 * \param count Size of buffer
 * \param pos Pointer to actual written characters counter
 * \param format Format to be used for snprintf
 * \return True if write was successfull otherwise false
 */
static bool formatted_write(char *restrict str, size_t count, size_t *pos, const char *restrict format, ...)
{
	va_list arg;
	va_start(arg, format);
	if((size_t)vsnprintf(NULL, 0, format, arg) >= count)
	{
		va_end(arg);
		return false;
	}
	*pos += vsnprintf(str, count, format, arg);
	va_end(arg);
	return true;
}


double difftime(time_t time_end, time_t time_beg)
{
	return time_end - time_beg;
}

time_t time(time_t *res)
{
	time_t ret;
#ifdef BUILD_KERNEL
	ret = cmos_syscall_timestamp();
#else
	ret = syscall_getTimestamp();
#endif
	if(res != NULL)
		*res = ret;
	return ret;
}

//TODO
clock_t clock()
{
	return 0;
}

//TODO: handle negative values
struct tm *gmtime(const time_t *time)
{
	static struct tm t;
	time_t secs = *time;

	for(t.tm_year = 70; ; t.tm_year++)
	{
		time_t secs_of_year = (isLeapYear(START_C_YEAR + t.tm_year) ? DAYS_PER_LEAP : DAYS_PER_YEAR) * SECS_PER_DAY;
		if(secs < secs_of_year)
			break;
		secs -= secs_of_year;
	}

	for(t.tm_mon = 0; ; t.tm_mon++)
	{
		time_t secs_of_month = getDaysOfMonth(t.tm_year, t.tm_mon + 1) * SECS_PER_DAY;
		if(secs < secs_of_month)
			break;
		secs -= secs_of_month;
	}

	t.tm_mday = secs / SECS_PER_DAY + 1;
	secs %= SECS_PER_DAY;
	t.tm_hour = secs / SECS_PER_HOUR;
	secs %= SECS_PER_HOUR;
	t.tm_min = secs / SECS_PER_MIN;
	secs %= SECS_PER_MIN;
	t.tm_sec = secs;

	//Recalculate wday and yday
	calcWDays(&t);
	calcYDays(&t);

	return &t;
}

//TODO: determine DST
//TODO: handle negative values
time_t mktime(struct tm *time)
{
	if(time->tm_year < 0 || time->tm_mon < 0 || time->tm_mday < 0
		|| time->tm_hour < 0 || time->tm_min < 0 || time->tm_sec < 0)
		return -1;

	uint64_t year = time->tm_year + START_C_YEAR;
	time_t secs = getYearSeconds(year);
	secs += getMonthSeconds(year, time->tm_mon + 1);
	secs += (time->tm_mday - 1) * SECS_PER_DAY;
	secs += time->tm_hour * SECS_PER_HOUR;
	secs += time->tm_min * SECS_PER_MIN;
	secs += time->tm_sec;

	//Recalculate wday and yday
	calcWDays(time);
	calcYDays(time);

	return secs;
}

char *asctime(const struct tm *time)
{
	static char res[sizeof("Www Mmm dd hh:mm:ss yyyy\n")];
	static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	sprintf(res, "%s %s %2d %.2d:%.2d:%.2d %4d\n", days[time->tm_wday], months[time->tm_mon], time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec, time->tm_year + START_C_YEAR);

	return res;
}

//TODO: localization
size_t strftime(char *restrict str, size_t count, const char *restrict format, const struct tm *restrict time)
{
	static const char *day_names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thusday", "Friday", "Saturday"};
	static const char *day_abr_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	static const char *month_names[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
	static const char *month_abr_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	static const char *day_periods[] = {"a.m.", "p.m."};

	size_t pos = 0;

	for(; *format != '\0' && count > pos; format++)
	{
		switch(*format)
		{
			case '%':
				format++;
				//XXX: ignored because localization is not supported yet
				switch(*format)
				{
					case 'E':
						format++;
					break;
					case '0':
						format++;
					break;
				}

				switch(*format)
				{
					case '%':
						str[pos++] = *format;
					break;
					case 'n':
						str[pos++] = '\n';
					break;
					case 't':
						str[pos++] = '\t';
					break;
					case 'Y':
					{
						const char *format = "%i";
						int year = time->tm_year + START_C_YEAR;
						if(!formatted_write(str + pos, count - pos, &pos, format, year))
							return 0;
					}
					break;
					case 'y':
					{
						const char *format = "%02i";
						int year = time->tm_year % 100;
						if(!formatted_write(str + pos, count - pos, &pos, format, year))
							return 0;
					}
					break;
					case 'C':
					{
						const char *format = "%02i";
						int year = (time->tm_year + START_C_YEAR - time->tm_year % 100) / 100;
						if(!formatted_write(str + pos, count - pos, &pos, format, year))
							return 0;
					}
					break;
					//TODO
					case 'G':
					{
					}
					break;
					//TODO
					case 'g':
					{
					}
					break;

					//Abbreviated month name
					case 'b':
					case 'h':
					{
						const char *format = "%s";
						const char *name = month_abr_names[time->tm_mon];
						if(!formatted_write(str + pos, count - pos, &pos, format, name))
							return 0;
					}
					break;
					//Full month name
					case 'B':
					{
						const char *format = "%s";
						const char *name = month_names[time->tm_mon];
						if(!formatted_write(str + pos, count - pos, &pos, format, name))
							return 0;
					}
					break;
					//Month as number
					case 'm':
					{
						const char *format = "%02i";
						int month = time->tm_mon + 1;
						if(!formatted_write(str + pos, count - pos, &pos, format, month))
							return 0;
					}
					break;

					//Week of the year (Sunday as first day of week)
					//TODO
					case 'U':
					{
					}
					break;
					//Week of the year (Monday as first day of week)
					//TODO
					case 'W':
					{
					}
					break;
					//ISO 8601 week of the year
					//TODO
					case 'V':
					{
					}
					break;

					//Day of the year
					case 'j':
					{
						const char *format = "%03i";
						int day = time->tm_yday + 1;
						if(!formatted_write(str + pos, count - pos, &pos, format, day))
							return 0;
					}
					break;
					//Day of the month
					case 'd':
					{
						const char *format = "%02i";
						int day = time->tm_mday;
						if(!formatted_write(str + pos, count - pos, &pos, format, day))
							return 0;
					}
					break;
					//Day of the month
					case 'e':
					{
						const char *format = "%2i";
						int day = time->tm_mday;
						if(!formatted_write(str + pos, count - pos, &pos, format, day))
							return 0;
					}
					break;

					//Abbreviated weekday
					case 'a':
					{
						const char *format = "%s";
						const char *day = day_abr_names[time->tm_wday];
						if(!formatted_write(str + pos, count - pos, &pos, format, day))
							return 0;
					}
					break;
					//Full weekday
					case 'A':
					{
						const char *format = "%s";
						const char *day = day_names[time->tm_wday];
						if(!formatted_write(str + pos, count - pos, &pos, format, day))
							return 0;
					}
					break;
					//Weekday
					case 'w':
					{
						const char *format = "%i";
						int day = time->tm_wday;
						if(!formatted_write(str + pos, count - pos, &pos, format, day))
							return 0;
					}
					break;
					//Weekday (ISO 8601 format)
					case 'u':
					{
						const char *format = "%i";
						int day = time->tm_wday ? : 7;	//0 -> 7 (Sunday)
						if(!formatted_write(str + pos, count - pos, &pos, format, day))
							return 0;
					}
					break;

					//Hour (24h)
					case 'H':
					{
						const char *format = "%02i";
						int hour = time->tm_hour;
						if(!formatted_write(str + pos, count - pos, &pos, format, hour))
							return 0;
					}
					break;
					//Hour (12h)
					case 'I':
					{
						const char *format = "%02i";
						int hour = time->tm_hour % 12 + 1;
						if(!formatted_write(str + pos, count - pos, &pos, format, hour))
							return 0;
					}
					break;
					//Minute
					case 'M':
					{
						const char *format = "%02i";
						int minute = time->tm_min;
						if(!formatted_write(str + pos, count - pos, &pos, format, minute))
							return 0;
					}
					break;
					//Second
					case 'S':
					{
						const char *format = "%02i";
						int second = time->tm_sec;
						if(!formatted_write(str + pos, count - pos, &pos, format, second))
							return 0;
					}
					break;

					//Standard date and time string
					case 'c':
					{
						const char *format = "%a %b %e %H:%M:%S %Y";
						size_t written = strftime(str + pos, count - pos, format, time);
						if(written == 0)
							return 0;
						pos += written;
					}
					break;
					//Date representation
					case 'x':
					{
						const char *format = "%b %e %Y";
						size_t written = strftime(str + pos, count - pos, format, time);
						if(written == 0)
							return 0;
						pos += written;
					}
					break;
					//Time representation
					case 'X':
					{
						const char *format = "%H:%M:%S";
						size_t written = strftime(str + pos, count - pos, format, time);
						if(written == 0)
							return 0;
						pos += written;
					}
					break;
					case 'D':
					{
						const char *format = "%m/%d/%y";
						size_t written = strftime(str + pos, count - pos, format, time);
						if(written == 0)
							return 0;
						pos += written;
					}
					break;
					case 'F':
					{
						const char *format = "%Y-%m-%d";
						size_t written = strftime(str + pos, count - pos, format, time);
						if(written == 0)
							return 0;
						pos += written;
					}
					break;
					case 'r':
					{
						const char *format = "%I:%M:%S %p";
						size_t written = strftime(str + pos, count - pos, format, time);
						if(written == 0)
							return 0;
						pos += written;
					}
					break;
					case 'R':
					{
						const char *format = "%H:%M";
						size_t written = strftime(str + pos, count - pos, format, time);
						if(written == 0)
							return 0;
						pos += written;
					}
					break;
					case 'T':
					{
						const char *format = "%H:%M:%S";
						size_t written = strftime(str + pos, count - pos, format, time);
						if(written == 0)
							return 0;
						pos += written;
					}
					break;
					case 'p':
					{
						const char *format = "%s";
						const char *period = day_periods[time->tm_hour >= 12];
						if(!formatted_write(str + pos, count - pos, &pos, format, period))
							return 0;
					}
					break;
					//Offset from UTC
					//TODO
					case 'z':
					{
					}
					break;
					//Time zone name
					//TODO
					case 'Z':
					{
					}
					break;
				}
			break;
			default:
				str[pos++] = *format;
		}
	}

	return pos;
}
