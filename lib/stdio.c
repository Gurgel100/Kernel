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
#include "stdarg.h"
#include "string.h"
#include "userlib.h"
#include "ctype.h"
#ifdef BUILD_KERNEL
#include "vfs.h"
#else
#include "syscall.h"
#endif

#define ASPRINTF_INITIAL_BUFFER_SIZE 64

#define MIN(x,y) ((x < y) ? x : y)
#define MAX(x,y) ((x > y) ? x : y)

struct asprintf_args{
		char *buffer;
		size_t buflen;
		size_t bytes_written;
};

FILE* stderr = NULL;
FILE* stdin = NULL;
FILE* stdout = NULL;

/*
 * Rückgabewerte von putc und putsn:
 * 	>0: Erfolg. Wert wird zu bytes_written addiert, das von jvprintf am Ende zurückgegeben wird.
 * 	=0 (putc) oder !=num (putsn): jvprintf terminiert und gibt bytes_written + diesen Wert zurück.
 * 	<0: jvprintf terminiert und gibt -1 zurück.
 */
typedef struct{
	int (*putc)(void *, char);
	int (*putsn)(void *, const char *, int);
	void *arg;
}jprintf_args;

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
#ifdef BUILD_KERNEL
	file->stream = vfs_Open(filename, m);
#else
	file->stream = syscall_fopen(filename, m);
#endif
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

#ifdef BUILD_KERNEL
	vfs_Close(stream->stream);
#else
	syscall_fclose(stream->stream);
#endif
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
#ifdef BUILD_KERNEL
					size_t size = vfs_Read(stream->stream, stream->posRead++, 1, &tmp);
#else
					size_t size = syscall_fread(stream->stream, stream->posRead++, 1, &tmp);
#endif
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
				for(i = 0; i < length; i += stream->bufSize)
				{
#ifdef BUILD_KERNEL
					tmp = vfs_Read(stream->stream, stream->posRead, MIN(length, stream->bufSize), stream->buffer);
#else
					tmp = syscall_fread(stream->stream, stream->posRead, MIN(length, stream->bufSize), stream->buffer);
#endif
					stream->bufPos = tmp;
					stream->bufStart = stream->posRead;
					stream->posRead += tmp;
					memcpy(ptr + i, stream->buffer, tmp);
					size += tmp;
					if(tmp == 0)
						break;
				}
				if(size < length)
					stream->eof = true;
				readData = size;
			}
		}
	}
	else
	{
#ifdef BUILD_KERNEL
		readData = vfs_Read(stream->stream, stream->posRead, length, ptr);
#else
		readData = syscall_fread(stream->stream, stream->posRead, length, ptr);
#endif
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
#ifdef BUILD_KERNEL
		writeData = vfs_Write(stream->stream, stream->posWrite, length, ptr);
#else
		writeData = syscall_fwrite(stream->stream, stream->posWrite, length, ptr);
#endif
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
#ifdef BUILD_KERNEL
				size = vfs_Write(stream->stream, stream->bufStart, stream->bufPos + 1, stream->buffer);
#else
				size = syscall_fwrite(stream->stream, stream->bufStart, stream->bufPos + 1, stream->buffer);
#endif
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
#ifdef BUILD_KERNEL
		vfs_Write(stream->stream, stream->bufStart, stream->bufPos, stream->buffer);
#else
		syscall_fwrite(stream->stream, stream->bufStart, stream->bufPos, stream->buffer);
#endif

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
#ifdef BUILD_KERNEL
				size_t filesize = vfs_getFileinfo(stream->stream, VFS_INFO_FILESIZE);
#else
				size_t filesize = syscall_StreamInfo(stream->stream, VFS_INFO_FILESIZE);
#endif
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

int fgetc(FILE *stream)
{
	char buffer;
	if(stream == NULL)
		return EOF;
	if(fread(&buffer, 1, 1, stream) == 0)
	{
		if(stream->eof)
			return EOF;
		return EOF;
	}
	return (int)buffer;
}

char *fgets(char *str, int n, FILE *stream)
{
	char c;
	int i = 0;
	if(stream == NULL || str == NULL)
		return NULL;

	while((c = fgetc(stream)) != '\n' && i < (n - 1) && !stream->eof)
	{
		str[i++] = c;
	}
	if(i != 0)
		str[i] = '\0';
	return str;
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

//Generelle vprintf-Funtkion mit Callbacks
int jprintf_putc(jprintf_args *args, char c)
{
	if(args->putc != NULL)
	{
		return args->putc(args->arg, c);
	}

	return 1;
}

int jprintf_putsn(jprintf_args *args, const char *str, int num)
{
	if(args->putsn != NULL)
	{
		return args->putsn(args->arg, str, num);
	}
	else
	{
		//Wenn keine putsn-Funktion definiert wurde, verwenden wir einfach putc
		size_t i;
		int bytes_written = 0;

		for(i = 0; (i < num || num == -1) && str[i] != '\0'; i++)
		{
			bytes_written += jprintf_putc(args, str[i]);
		}

		return bytes_written;
	}
}

int jvprintf(jprintf_args *args, const char *format, va_list arg)
{
	uint64_t pos = 0;
	char lpad;
	uint64_t width, precision;
	bool precision_spec;
	int8_t length;
	char buffer[64];
	bool left, sign, space_sign, alt;
	for(; *format; format++)
	{
		switch(*format)
		{
			case '%':	//Formatieren?
				format++;
				//% überspringen
				if(*format == '%')
				{
					jprintf_putc(args, '%');
					pos++;
					format++;
				}

				//Flags
				left = false;
				sign = false;
				space_sign = false;
				alt = false;
				lpad = ' ';
				switch(*format)
				{
					case '-':
						left = true;
						format++;
					break;
					case '+':
						sign = true;
						format++;
					break;
					case ' ':
						space_sign = true;
						format++;
					break;
					case '#':
						alt = true;
						format++;
					break;
					case '0':
						lpad = '0';
						format++;
					break;
				}

				//Width
				width = 0;
				if(*format >= '0' && *format <= '9')
					width = strtol(format, (char**)&format, 10);
				else if(*format == '*')
				{
					format++;
					int w = va_arg(arg, int);
					if(w < 0)
					{
						width = -w;
						left = true;
					}
					else
						width = w;
				}

				//Precision
				precision_spec = false;
				if(*format == '.')
				{
					format++;
					if(*format >= '0' && *format <= '9')
					{
						precision = strtol(format, (char**)&format, 10);
						precision_spec = true;
					}
					else if(*format == '*')
					{
						int prec = va_arg(arg, int);
						if(prec < 0)
							precision = 0;
						else
							precision = prec;
						precision_spec = true;
					}
				}

				//Length
				length = 0;
				switch(*format)
				{
					case 'h':
						format++;
						if(*format == 'h')
						{
							format++;
							length = -2;
						}
						else
							length = -1;
					break;
					case 'l':
						format++;
						if(*format == 'l')
						{
							format++;
							length = 2;
						}
						else
							length = 1;
					break;
					case 'j':
						format++;
						length = 3;
					break;
					case 'z':
						format++;
						length = 4;
					break;
					case 't':
						format++;
						length = 5;
					break;
					case 'L':
						format++;
						length = 6;
					break;
				}

				switch(*format)
				{
					case 'u':	//Unsigned int
					{
						uint64_t value;
						switch(length)
						{
							case -2:
								value = va_arg(arg, unsigned int);
							break;
							case -1:
								value = va_arg(arg, unsigned int);
							break;
							case 0: default:
								value = va_arg(arg, unsigned int);
							break;
							case 1:
								value = va_arg(arg, unsigned long);
							break;
							case 2:
								value = va_arg(arg, unsigned long long);
							break;
							case 3:
								value = va_arg(arg, uint64_t);
							break;
							case 4:
								value = va_arg(arg, size_t);
							break;
							case 5:
								value = va_arg(arg, uintptr_t);
							break;
						}
						if(!precision_spec)
							precision = 1;
						utoa(value, buffer);
						size_t len = strlen(buffer);
						//Padding
						for(; width > MAX(len, precision); width--)
						{
							pos += jprintf_putc(args, lpad);
						}
						if(!((precision_spec && precision == 0) && value == 0))
						{
							for(; precision > len; precision--)
							{
								jprintf_putc(args, '0');
							}
							pos += jprintf_putsn(args, buffer, -1);
						}
					}
					break;
					case 'i':	//Signed int
					case 'd':
					{
						int64_t value;
						switch(length)
						{
							case -2:
								value = va_arg(arg, unsigned int);
							break;
							case -1:
								value = va_arg(arg, unsigned int);
							break;
							case 0: default:
								value = va_arg(arg, unsigned int);
							break;
							case 1:
								value = va_arg(arg, unsigned long);
							break;
							case 2:
								value = va_arg(arg, unsigned long long);
							break;
							case 3:
								value = va_arg(arg, uint64_t);
							break;
							case 4:
								value = va_arg(arg, size_t);
							break;
							case 5:
								value = va_arg(arg, uintptr_t);
							break;
						}
						if(!precision_spec)
							precision = 1;
						itoa(value, buffer);
						size_t len = len = strlen(buffer);
						if(value < 0)
						{
							if(sign)
							{
								memmove(buffer + 1, buffer, len + 1);
								buffer[0] = '+';
							}
							else if(space_sign)
							{
								memmove(buffer + 1, buffer, len + 1);
								buffer[0] = ' ';
							}
						}
						//Padding
						for(; width > MAX(len, precision); width--)
						{
							pos += jprintf_putc(args, lpad);
						}
						if(!((precision_spec && precision == 0) && value == 0))
						{
							for(; precision > len; precision--)
							{
								jprintf_putc(args, '0');
							}
							pos += jprintf_putsn(args, buffer, -1);
						}
					}
					break;
					case 'f': case 'F':	//Float
						//sputs(str, ftoa(va_arg(arg, double), buffer));
						//pos += strlen(buffer);
					break;
					case 'x': case 'X':	//Hex
					{
						uint64_t value;
						switch(length)
						{
							case -2:
								value = va_arg(arg, unsigned int);
								i2hex(value, buffer, 2);
							break;
							case -1:
								value = va_arg(arg, unsigned int);
								i2hex(value, buffer, 4);
							break;
							case 0: default:
								value = va_arg(arg, unsigned int);
								i2hex(value, buffer, 8);
							break;
							case 1:
								value = va_arg(arg, unsigned long);
								i2hex(value, buffer, 16);
							break;
							case 2:
								value = va_arg(arg, unsigned long long);
								i2hex(value, buffer, 16);
							break;
							case 3:
								value = va_arg(arg, uint64_t);
								i2hex(value, buffer, 16);
							break;
							case 4:
								value = va_arg(arg, size_t);
								i2hex(value, buffer, 16);
							break;
							case 5:
								value = va_arg(arg, uintptr_t);
								i2hex(value, buffer, 16);
							break;
						}
						if(!precision_spec)
							precision = 1;
						size_t len = strlen(buffer);
						//Padding
						for(; width > MAX(len, precision); width--)
						{
							pos += jprintf_putc(args, lpad);
						}
						if(!((precision_spec && precision == 0) && value == 0))
						{
							for(; precision > strlen(buffer); precision--)
							{
								jprintf_putc(args, '0');
							}

							if(*format == 'x')
							{
								size_t i;
								for(i = 0; i < strlen(buffer); i++)
								{
									buffer[i] = (char)tolower(buffer[i]);
								}
							}

							//Alternative Form
							if(alt)
							{
								pos += jprintf_putc(args, '0');
								pos += jprintf_putc(args, *format);
							}
							pos += jprintf_putsn(args, buffer, -1);
						}
					}
					break;
					case 's':	//String
					{
						if(!precision_spec)
							precision = -1;

						const char *str = va_arg(arg, char*);

						for(; width > strlen(str); width--)
						{
							pos += jprintf_putc(args, lpad);
						}

						pos += jprintf_putsn(args, str, precision);
					}
					break;
					case 'c':	//Char
						//Padding
						for(; width > 1; width--)
						{
							pos += jprintf_putc(args, lpad);
						}
						pos += jprintf_putc(args, (char)va_arg(arg, int));
					break;
					default:	//Ansonsten ungültig
						format--;
						//pos--;
					break;
				}
			break;
			default:	//Ansonsten schreibe das aktuelle Zeichen
				jprintf_putc(args, *format);
				pos++;
			break;
		}
	}
	return pos;
}

//Callback-Funktionen
typedef struct{
	char *buffer;
	size_t size, bytes_written;
}vasprintf_t;
int vasprintf_putc(void *arg, char c)
{
	vasprintf_t *args = arg;
	if(args->size == args->bytes_written)
	{
		args->size *= 2;
		args->buffer = (char*)realloc(args->buffer, args->size);
		if(args->buffer == NULL)
			return -1;
	}

	args->buffer[args->bytes_written++] = c;

	return 1;
}

int vasprintf(char **str, const char *format, va_list arg)
{
	if(str == NULL)
		return -1;

	int retval;

	vasprintf_t asarg = {
			.buffer = malloc(ASPRINTF_INITIAL_BUFFER_SIZE),
			.size = ASPRINTF_INITIAL_BUFFER_SIZE
	};

	jprintf_args args = {
			.putc = &vasprintf_putc,
			.arg = &asarg
	};

	if(asarg.buffer == NULL)
		return -1;

	retval = jvprintf(&args, format, arg);

	//Den String nullterminieren
	if(vasprintf_putc(&asarg, '\0') == -1)
		return -1;

	//Den Buffer wieder verkleinern, wenn wir können
	if(asarg.bytes_written < asarg.size)
	{
		asarg.buffer = realloc(asarg.buffer, asarg.bytes_written);
	}

	*str = asarg.buffer;

	return retval;
}

int vfprintf(FILE *stream, const char *format, va_list arg)
{
	return EOF;
}

//Callback-Funktionen
int vprintf_putc(void *arg, char c)
{
	putchar(c);
	return 1;
}

int vprintf_putsn(void *arg, const char *str, int n)
{
	int len = strlen(str);

	if(len <= n || n == -1)
	{
		return puts(str);
	}
	else
	{
		int i;
		for(i = 0; i < n && str[i] != '\0'; i++)
		{
			putchar(str[i]);
		}
		return i;
	}

	return n;
}

int vprintf(const char *format, va_list arg)
{
	jprintf_args handler = {
			.putc = &vprintf_putc,
			.putsn = &vprintf_putsn
	};

	return jvprintf(&handler, format, arg);
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
#ifdef BUILD_KERNEL
		return (int)getch();
#else
		return (int)syscall_getch();
#endif
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
#ifdef BUILD_KERNEL
		putch(zeichen);
#else
		syscall_putch(zeichen);
#endif
		return zeichen;
	}
	else
		return EOF;
}

int putchar(int zeichen)
{
	if(stdout == NULL)
	{
#ifdef BUILD_KERNEL
		putch(zeichen);
#else
		syscall_putch(zeichen);
#endif
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
