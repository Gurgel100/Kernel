/*
 * stdio.c
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "vfs.h"

#define ASPRINTF_INITIAL_BUFFER_SIZE 64

#define MIN(x,y) ((x < y) ? x : y)

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

//Dateifunktionen
FILE *fopen(const char *filename, const char *mode)
{
	vfs_mode_t m;
	bool binary = false;

	//m auf 0 setzen weil sonst können dort irgendwelche Werte drinnen stehen
	memset(&m, false, sizeof(vfs_mode_t));

	//Modus analysieren
	switch(*mode++)
	{
	case 'r':
		m.read = true;
		while(*mode != '\0')
		{
			switch(*mode++)
			{
			case '+':
				m.write = true;
				break;
			case 'b':
				binary = true;
			}
		}
		break;
	case 'w':
		m.write = true;
		m.empty = true;
		m.create = true;
		while(*mode != '\0')
		{
			switch(*mode++)
			{
			case '+':
				m.read = true;
				break;
			case 'b':
				binary = true;
			}
		}
		break;
	case 'a':
		m.append = true;
		m.create = true;
		m.write = true;
		while(*mode != '\0')
		{
			switch(*mode++)
			{
			case '+':
				m.read = true;
				break;
			case 'b':
				binary = true;
			}
		}
		break;
	default:
		return NULL;
	}

	FILE *file = calloc(sizeof(FILE), 1);
	file->error = IO_NO_ERROR;
	file->mode.read = m.read;
	file->mode.write = m.write;
	file->mode.binary = binary;
	file->stream = vfs_Open(filename, m);
	if(file->stream == NULL)
	{
		free(file);
		return NULL;
	}
	setvbuf(file, malloc(BUFSIZ), _IOFBF, BUFSIZ);

	return file;
}

int fclose(FILE *stream)
{
	if(stream == NULL)
		return EOF;

	//Buffer noch löschen, bevor wir den Stream schliessen
	if(stream->mode.write)
		fflush(stream);

	if(setvbuf(stream, NULL, _IONBF, 0))
		return EOF;

	vfs_Close(stream->stream);
	free(stream);

	return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t length = size * nmemb;
	size_t readData = 0;
	//Überprüfen, ob erlaubt ist, diesen Stream zu lesen
	if(!stream->mode.read)
		return 0;

	//Schauen, ob die angeforderten Daten im Cache sind
	if(stream->bufMode != IO_MODE_NO_BUFFER)
	{
		if(stream->bufMode == IO_MODE_LINE_BUFFER)
		{
			if(stream->bufStart <= stream->posRead && stream->bufStart + stream->bufPos >= stream->posRead + length)
			{
				//Alles im Buffer
				memcpy(ptr, stream->buffer, length);
				stream->posRead += length;
				readData += length;
			}
			else
			{
				//Nicht alles im Buffer
				//Wir lesen hier einfach noch mal alles aus
				//TODO: Besserer Algorithmus finden
				stream->bufPos = 0;
				stream->bufStart = stream->posRead;

				uint8_t tmp;
				size_t i;
				for(i = 0; i < length; i++)
				{
					size_t size = vfs_Read(stream->stream, stream->posRead++, 1, &tmp);
					stream->buffer[stream->bufPos++] = tmp;
					if(tmp == '\n' && i < length)
					{
						memcpy(ptr, stream->buffer, stream->bufPos);
						stream->bufPos = 0;
						stream->bufStart = stream->posRead;
					}
				}
				if(tmp != '\n')
					memcpy(ptr, stream->buffer, stream->bufPos);
			}
		}
		else
		{
			if(stream->bufStart + stream->bufPos < stream->posRead && stream->bufPos >= length)
			{
				memcpy(ptr, stream->buffer + stream->bufStart - stream->posRead, length);
				readData = length;
			}
			//Ansonsten einfach den ganzen Buffer nochmal laden
			else
			{
				size_t size = 0;
				size_t i, tmp;
				for(i = 0; i < length && tmp != 0; i += stream->bufSize)
				{
					tmp = vfs_Read(stream->stream, stream->posRead, MIN(length, stream->bufSize), stream->buffer);
					stream->bufPos = tmp;
					stream->bufStart = stream->posRead;
					stream->posRead += tmp;
					memcpy(ptr + i, stream->buffer, tmp);
					size += tmp;
				}
				if(size < length)
					stream->eof = true;
				readData = size;
			}
		}
	}
	else
	{
		readData = vfs_Read(stream->stream, stream->posRead, length, ptr);
		stream->posRead += length;
	}

	//Wenn nicht im binary modus, dann letztes Zeichen durch '\0' ersetzen
	if(!stream->mode.binary)
		((char*)ptr)[size] = '\0';

	return readData / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t length = size * nmemb;
	size_t writeData = 0;

	//Überprüfen, ob es erlaubt ist, diese Datei zu schreiben
	if(!stream->mode.write)
		return 0;

	if(stream->bufMode == IO_MODE_NO_BUFFER)
	{
		writeData = vfs_Write(stream->stream, stream->posWrite, length, ptr);
	}
	else if(stream->bufMode == IO_MODE_LINE_BUFFER)
	{
		//Eine Zeile in Buffer schreiben und wenn nötig schreiben
		uint8_t *source = ptr;
		size_t i;
		for(i = 0; i < length; i++)
		{
			stream->buffer[stream->bufPos++] = source[i];
			if(source[i] == '\n')
			{
				size_t size = 0;
				size = vfs_Write(stream->stream, stream->bufStart, stream->bufPos + 1, stream->buffer);
				stream->bufPos = 0;
				stream->bufStart += size;
				writeData += size;
			}
		}
	}
	else	//Full buffer
	{
		//Alles in den Buffer schreiben
		memcpy(stream->buffer + stream->bufPos, ptr, length);
		stream->bufPos += length;
	}

	return writeData / size;
}

int fflush(FILE *stream)
{
	///TODO: wenn stream = NULL, dann sollten alle Buffer geflusht werden
	if(stream == NULL)
		return 0;

	if(stream->bufMode != IO_MODE_NO_BUFFER)
		vfs_Write(stream->stream, stream->bufStart, stream->bufPos, stream->buffer);

	return 0;
}

void setbuf(FILE *stream, char *buffer)
{
	return (void)setvbuf(stream, buffer, (buffer == NULL) ? _IONBF : _IOFBF, BUFSIZ);
}

int setvbuf(FILE *stream, char *buffer, int mode, size_t size)
{
	if(stream == NULL)
		return -1;

	switch(mode)
	{
		//Kein buffering
	case _IONBF:
		if(stream->buffer && stream->intBuf)
			free(stream->buffer);
		stream->buffer = NULL;
		stream->intBuf = false;
		stream->bufSize = 0;
		stream->posRead = 0;
		stream->posWrite = 0;
		stream->bufMode = IO_MODE_NO_BUFFER;
		break;
	case _IOLBF:
		if(buffer == NULL)
		{
			stream->intBuf = true;
			buffer = malloc(size);
		}
		stream->buffer = buffer;
		stream->bufSize = size;
		stream->bufStart = EOF;
		stream->bufPos = 0;
		stream->posRead = 0;
		stream->posWrite = 0;
		stream->bufMode = IO_MODE_LINE_BUFFER;
		break;
	case _IOFBF:
		if(buffer == NULL)
		{
			stream->intBuf = true;
			buffer = malloc(size);
		}
		stream->buffer = buffer;
		stream->bufSize = size;
		stream->bufStart = EOF;
		stream->bufPos = 0;
		stream->posRead = 0;
		stream->posWrite = 0;
		stream->bufMode = IO_MODE_FULL_BUFFER;
		break;
	default:
		return -1;
	}

	return 0;
}

int fseek(FILE *stream, long int offset, int whence)
{
	if(!stream)
		return -1;

	if(stream->mode.binary)
	{
		switch (whence)
		{
			case SEEK_CUR:
				stream->posRead += offset;
				stream->posWrite += offset;
			break;
			case SEEK_SET:
				stream->posRead = offset;
				stream->posWrite = offset;
			break;
			case SEEK_END:
			{	//Klammern müssen da sein, denn sonst kann man keine Variablen definieren
				//Grösse der Datei ermitteln
				size_t filesize = vfs_getFileinfo(stream->stream, VFS_INFO_FILESIZE);
				stream->posRead = stream->posWrite = filesize - offset;
			}
			break;
		}
	}
	else
	{
		if(whence == SEEK_SET && offset >= 0)
		{
			stream->posRead = offset;
			stream->posWrite = offset;
		}
	}

	stream->eof = false;

	return 0;
}

long int ftell(FILE *stream)
{
	if(stream == NULL)
		return -1L;
	return stream->posRead;
}

int fsetpos(FILE *stream, const fpos_t *pos)
{
	if(stream == NULL || pos == NULL)
		return -1;
	stream->posRead = stream->posWrite = *pos;
	return 0;
}

int fgetpos(FILE *stream, fpos_t *pos)
{
	if(stream == NULL || pos == NULL)
		return -1;
	*pos = stream->posRead;
	return 0;
}


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
