/*
 * stdio.h
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#ifndef STDIO_H_
#define STDIO_H_

#include <bits/types.h>
#include "stdarg.h"
#include "stdbool.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EOF
#define EOF		(-1)
#endif

#define _IONBF	0
#define _IOLBF	1
#define _IOFBF	2

#ifndef SEEK_SET
#define SEEK_SET 1
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 2
#endif
#ifndef SEEK_END
#define SEEK_END 3
#endif

#define FILENAME_MAX	4096

#define BUFSIZ	65536

typedef _size_t size_t;

typedef enum{
	IO_MODE_NO_BUFFER, IO_MODE_LINE_BUFFER, IO_MODE_FULL_BUFFER
}bufMode_t;

typedef enum{
	IO_NO_ERROR
}io_error_t;


typedef struct filestream FILE;

typedef size_t fpos_t;

extern FILE* stderr;
extern FILE* stdin;
extern FILE* stdout;

extern FILE *fopen(const char *filename, const char *mode);
extern int fclose(FILE *stream);
extern size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int fflush(FILE *stream);
extern void setbuf(FILE *stream, char *buffer);
extern int setvbuf(FILE *stream, char *buffer, int mode, size_t size);
extern int fseek(FILE *stream, long int offset, int whence);
extern long int ftell(FILE *stream);
extern int fsetpos(FILE *stream, const fpos_t *pos);
extern int fgetpos(FILE *stream, fpos_t *pos);
extern void rewind(FILE *stream);
extern int feof(FILE *stream);

extern int fgetc(FILE *stream);
extern char *fgets(char *str, int n, FILE *stream);

extern int fputc(int ch, FILE *stream);
extern int fputs(const char *str, FILE *stream);

extern int vfprintf(FILE *stream, const char *format, va_list arg);
extern int vprintf(const char *format, va_list arg);
extern int vsprintf(char *str, const char *format, va_list arg);
extern int vsnprintf(char *str, int bufsz, const char *format, va_list arg);
extern int vasprintf(char **str, const char *format, va_list arg);

extern int fprintf(FILE *stream, const char *format, ...);
extern int printf(const char *format, ...);
extern int sprintf(char *str, const char *format, ...);
extern int snprintf(char *str, int bufsz, const char *format, ...);
extern int asprintf(char **str, const char *format, ...);

extern int vfscanf(FILE *stream, const char *format, va_list arg);
extern int vscanf(const char *format, va_list arg);
extern int vsscanf(const char *str, const char *format, va_list arg);

extern int fscanf(FILE *stream, const char *format, ...);
extern int scanf(const char *format, ...);
extern int sscanf(const char *str, const char *format, ...);

extern int ungetc(int c, FILE *stream);
extern int getc(FILE *stream);
extern int getchar(void);
extern char *gets(char *str);

extern int putc(int ch, FILE *stream);
extern int putchar(int zeichen);
extern int puts(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* STDIO_H_ */
