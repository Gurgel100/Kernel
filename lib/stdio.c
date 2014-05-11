/*
 * stdio.c
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#include "stdio.h"
#include "stdint.h"
#include "stdbool.h"

#define ASPRINTF_INITIAL_BUFFER_SIZE 64

struct asprintf_args{
		char *buffer;
		size_t buflen;
		size_t bytes_written;
};

FILE* stderr = NULL;
FILE* stdin = NULL;
FILE* stdout = NULL;

int sputchar(char **dest, int zeichen);
int sputs(char **dest, const char *str);

char *itoa(int64_t x, char *s);							//Int nach String
char *utoa(uint64_t x, char *s);						//UInt nach String
char *ftoa(float x, char *s);							//Float nach String
char *i2hex(uint64_t val, char* dest, uint64_t len);	//Int nach Hexadezimal

//TODO: alle print-Funktionen fertigstellen

int fprintf(FILE *stream, const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	int pos = vfprintf(stream, format, arg);
	va_end(arg);
	return pos;
}

int printf(const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	int pos = vprintf(format, arg);
	va_end(arg);
	return pos;
}

int sprintf(char *str, const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	int pos = vsprintf(str, format, arg);
	va_end(arg);
	return pos;
}

int asprintf(char **str, const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	int pos = vasprintf(str, format, arg);
	va_end(arg);
	return pos;
}

int vasprintf(char **str, const char *format, va_list arg)
{
	uint64_t pos = 0;
	char lpad = ' ';
	uint64_t width = 0;
	static char buffer[64];
	*str = calloc(1, 1);
	for(; *format; format++)
	{
		switch(*format)
		{
			case '%':	//Formatieren?
				format++;
				//% überspringen
				if(*format == '%')
				{
					sputchar(str, '%');
					pos++;
					format++;
				}

				//Flags
				switch(*format)
				{
					case '-':case '+':case ' ': case '#':
						format++;
					break;
					case '0':
						lpad = '0';
						format++;
					break;
				}

				//Width
				if(*format >= '0' && *format <= '9')
					width = strtol(format, (char**)&format, 10);
				else if(*format == '*')
				{
					format++;
					width = va_arg(arg, uint64_t);
				}

				switch(*format)
				{
					case 'u':	//Unsigned int
						sputs(str, utoa(va_arg(arg, uint64_t), buffer));
						pos += strlen(buffer);
					break;
					case 'i':	//Signed int
					case 'd':
						sputs(str, itoa(va_arg(arg, int64_t), buffer));
						pos += strlen(buffer);
					break;
					case 'f':	//Float
						sputs(str, ftoa(va_arg(arg, double), buffer));
						pos += strlen(buffer);
					break;
					case 'X':	//Hex 8
						sputs(str, i2hex(va_arg(arg, int64_t), buffer, 8));
						pos += 8;
					break;
					case 'x':	//Hex 4
						sputs(str, i2hex(va_arg(arg, int64_t), buffer, 4));
						pos += 4;
					break;
					case 'y':	//Hex 2
						sputs(str, i2hex(va_arg(arg, int64_t), buffer, 2));
						pos += 2;
					break;
					case 's':	//String
					{
						char *temp = va_arg(arg, char*);
						sputs(str, temp);
						pos += strlen(temp);
					}
					break;
					case 'c':	//Char
						sputchar(str, (char)va_arg(arg, int64_t));
						pos++;
					break;
					default:	//Ansonsten ungültig
						format--;
						//pos--;
					break;
				}
			break;
			default:	//Ansonsten schreibe das aktuelle Zeichen
				sputchar(str, *format);
				pos++;
			break;
		}
	}
	return pos;
}

int vfprintf(FILE *stream, const char *format, va_list arg)
{
	return EOF;
}

int vprintf(const char *format, va_list arg)
{
	uint64_t pos = 0;
	char lpad = ' ';
	uint64_t width = 0;
	static char buffer[64];
	for(; *format; format++)
	{
		switch(*format)
		{
			case '%':	//Formatieren?
				format++;
				//% überspringen
				if(*format == '%')
				{
					putchar('%');
					pos++;
					format++;
				}

				//Flags
				switch(*format)
				{
					case '-': case '+': case ' ': case '#':
						format++;
					break;
					case '0':
						lpad = '0';
						format++;
					break;
				}

				//Width
				if(*format >= '0' && *format <= '9')
					width = strtol(format, (char**)&format, 10);
				else if(*format == '*')
				{
					format++;
					width = va_arg(arg, uint64_t);
				}

				switch(*format)
				{
					case 'u':	//Unsigned int
						puts(utoa(va_arg(arg, uint64_t), buffer));
						pos += strlen(buffer);
					break;
					case 'i':	//Signed int
					case 'd':
						puts(itoa(va_arg(arg, int64_t), buffer));
						pos += strlen(buffer);
					break;
					case 'f':	//Float
						puts(ftoa(va_arg(arg, double), buffer));
						pos += strlen(buffer);
					break;
					case 'X':	//Hex 8
						puts(i2hex(va_arg(arg, int64_t), buffer, 8));
						pos += 8;
					break;
					case 'x':	//Hex 4
						puts(i2hex(va_arg(arg, int64_t), buffer, 4));
						pos += 4;
					break;
					case 'y':	//Hex 2
						puts(i2hex(va_arg(arg, int64_t), buffer, 2));
						pos += 2;
					break;
					case 's':	//String
					{
						char *temp = va_arg(arg, char*);
						puts(temp);
						pos += strlen(temp);
					}
					break;
					case 'c':	//Char
						putchar((char)va_arg(arg, int64_t));
						pos++;
					break;
					default:	//Ansonsten ungültig
						format--;
						//pos--;
					break;
				}
			break;
			default:	//Ansonsten schreibe das aktuelle Zeichen
				putchar(*format);
				pos++;
			break;
		}
	}
	return pos;
}

int vsprintf(char *str, const char *format, va_list arg)
{
	return EOF;
}

//scanf-Funktionen
int fscanf(FILE *stream, const char *format, ...)
{
	return EOF;
}

int scanf(const char *format, ...)
{
	return EOF;
}

int sscanf(const char *str, const char *format, ...)
{
	return EOF;
}

//I/O-Funktionen
int getc(FILE *stream)
{
	if(stream == stdin)
	{
		unsigned char zeichen = getch();
		return (int)zeichen;
	}
	else
		return EOF;
}

int getchar(void)
{
	return getc(stdin);
}

char *gets(char *str)
{
	char c;
	uint64_t i = 0;
	while((c = getc(stdin)) != '\n' && c != '\0')
	{
		switch(c)
		{
			case EOF:
				return NULL;
			case '\b':	//Backspace
				if(i > 0)
					i--;
			break;
			default:
				str[i++] = c;
			break;
		}
	}
	if(i != 0)
		str[i] = '\0';
	return str;
}


int putc(int zeichen, FILE *stream)
{
	if(stream == stdout)
	{
		putch(zeichen);
		return zeichen;
	}
	else
		return EOF;
}

int putchar(int zeichen)
{
	if(stdout == NULL)
	{
		putch((char)zeichen);
		return zeichen;
	}
	else
		return putc(zeichen, stdout);
}

int puts(const char *str)
{
	unsigned int i;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(putc(str[i], stdout) == EOF)
			return EOF;
	}
	return 1;
}

//Hilfsfunktionen
int sputchar(char **dest, int zeichen)
{
	size_t length = strlen(*dest);
	*dest = realloc(*dest, length + 2);	//Speicher vergrössern
	(*dest)[length] = zeichen;
	(*dest)[length + 1] = '\0';
	return zeichen;
}

int sputs(char **dest, const char *str)
{
	unsigned int i;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(sputchar(dest, str[i]) == EOF)
			return EOF;
	}
	return 1;
}

char *itoa(int64_t x, char *s)
{
	uint64_t i = 0;
	bool sign = x < 0;	//Ist true falls die Zahl ein Vorzeichen hat (x < 0)
	if(sign)
		x = -x;
	do
	{
		s[i++] = abs((x % 10)) + '0';
	}
	while((x /= 10) != 0);
	if(sign)
		s[i++] = '-';	//Das Minus kommt an den Anfang
	s[i] = '\0';		//Ende des Strings markieren
	reverse(s);
	return s;
}

char *utoa(uint64_t x, char *s)
{
	uint64_t i = 0;
	do
	{
		s[i++] = (x % 10) + '0';
	}
	while((x /= 10) > 0);
	s[i] = '\0';		//Ende des Strings markieren
	reverse(s);
	return s;
}

char *ftoa(float x, char *s)
{
	char *buffer = s;
	itoa((int64_t)x, s);
	if(x < 0) x = -x;

	buffer += strlen(s);
	*buffer++ = '.';

	*buffer++ = ((uint64_t)(x * 10.0) % 10) + '0';
	*buffer++ = ((uint64_t)(x * 100.0) % 10) + '0';
	*buffer++ = ((uint64_t)(x * 1000.0) % 10) + '0';
	*buffer++ = ((uint64_t)(x * 10000.0) % 10) + '0';
	*buffer++ = ((uint64_t)(x * 100000.0) % 10) + '0';
	*buffer++ = ((uint64_t)(x * 1000000.0) % 10) + '0';
	*buffer = '\0';
	return s;
}

char *i2hex(uint64_t val, char* dest, uint64_t len)
{
    char* cp = &dest[len];
    while (cp > dest)
    {
        char x = val & 0xF;
        val >>= 4;
        *--cp = x + ((x > 9) ? 'A' - 10 : '0');
    }
    dest[len]='\0';
    return dest;
}
