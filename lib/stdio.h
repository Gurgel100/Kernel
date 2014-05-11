/*
 * stdio.h
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#ifndef STDIO_H_
#define STDIO_H_

#include "stddef.h"
#include "stdarg.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EOF
#define EOF		(-1)
#endif

typedef struct
{
		int				_cnt;	//Anzahl der Bytes im Buffer
		unsigned char	*_ptr;	//next character from/to here in buffer */
		unsigned char	*_base;	//Der Buffer
		unsigned char	_flag;	//Status des Streams
		unsigned char	_file;	//UNIX System Datei-Descriptor
}FILE;

extern FILE* stderr;
extern FILE* stdin;
extern FILE* stdout;

extern int vfprintf(FILE *stream, const char *format, va_list arg);
extern int fprintf(FILE *stream, const char *format, ...);
extern int printf(const char *format, ...);
extern int sprintf(char *str, const char *format, ...);
extern int asprintf(char **str, const char *format, ...);

extern int scanf(const char *format, ...);
extern int sscanf(const char *str, const char *format, ...);

extern int getc(FILE *stream);
extern int getchar(void);
extern char *gets(char *str);

extern int putc(int zeichen, FILE *stream);
extern int putchar(int zeichen);
extern int puts(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* STDIO_H_ */
