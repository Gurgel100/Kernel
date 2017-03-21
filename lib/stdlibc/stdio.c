/*
 * stdio.c
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "userlib.h"
#include "ctype.h"
#ifdef BUILD_KERNEL
#include "vfs.h"
#include "console.h"
#else
#include "syscall.h"
#endif
#include "math.h"
#include "assert.h"

#define ASPRINTF_INITIAL_BUFFER_SIZE 64

#define MIN(x,y) ((x < y) ? x : y)
#define MAX(x,y) ((x > y) ? x : y)

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

typedef struct{
	int (*getc)(void *);
	void (*ungetc)(void *, int);
	int (*tell)(void *);
	void *arg;
}jscanf_args;

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
	file->stream_id = vfs_Open(filename, m);
#else
	file->stream_id = syscall_fopen(filename, m);
#endif
	if(file->stream_id == -1ul)
	{
		free(file);
		return NULL;
	}
	setvbuf(file, NULL, _IOFBF, BUFSIZ);

	return file;
}

int fclose(FILE *stream)
{
	if(stream == NULL)
		return EOF;

	//Buffer noch löschen, bevor wir den Stream schliessen
	if(stream->mode.write)
		fflush(stream);

	if(stream->ungetch_count)
		free(stream->ungetch_buffer);

	if(setvbuf(stream, NULL, _IONBF, 0))
		return EOF;

#ifdef BUILD_KERNEL
	vfs_Close(stream->stream_id);
#else
	syscall_fclose(stream->stream_id);
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

	//Überprüfen, ob überhaupt etwas gelesen werden soll
	if(length == 0)
		return 0;

	//Schauen, ob die angeforderten Daten im Cache sind
	if(stream->bufMode != IO_MODE_NO_BUFFER)
	{
		size_t cacheLength = 0;
		uint64_t cacheOff = 0;
		//Schauen wieviel Daten im Cache sind
		if(stream->bufStart <= stream->posRead)
		{
			cacheOff = stream->posRead - stream->bufStart;
			if(stream->bufStart + stream->bufPos >= stream->posRead + length)
				cacheLength = length;
			else if(stream->bufStart + stream->bufPos > stream->posRead)
				cacheLength = stream->bufStart + stream->bufPos - stream->posRead;
		}
		else if(stream->bufStart < stream->posRead + length)
		{
			if(stream->bufStart + stream->bufPos >= stream->posRead + length)
				cacheLength = stream->posRead + length - stream->bufStart;
			else
				cacheLength = stream->bufSize;
		}

		//Daten im Cache kopieren
		memcpy(ptr + stream->bufStart - stream->posRead + cacheOff, stream->buffer + cacheOff, cacheLength);

		//Daten, die nicht im Cache sind laden
		char *tmp;
		size_t tmpLength;
		if(stream->bufStart > stream->posRead)
		{
			tmpLength = MIN(stream->bufStart - stream->posRead, length);
			tmp = malloc(tmpLength + 1);
			size_t size;
#ifdef BUILD_KERNEL
			size = vfs_Read(stream->stream_id, stream->posRead, tmpLength, tmp);
#else
			size = syscall_fread(stream->stream_id, stream->posRead, tmpLength, tmp);
#endif

			stream->eof = size < tmpLength;
			memcpy(ptr + readData, tmp, size);
			readData += size;

			//Wenn diese Daten direkt vor den Daten im Cache sind können wir diese einfach in den Cache laden
			if(stream->bufStart == (size_t)EOF
					|| (stream->bufStart > stream->posRead && stream->bufStart <= stream->posRead + length))
			{
				if(stream->bufMode == IO_MODE_FULL_BUFFER)
				{
					if(stream->bufPos + size <= stream->bufSize)
					{
						memmove(stream->buffer + size, stream->buffer, stream->bufPos);
						memcpy(stream->buffer, tmp, size);
						stream->bufStart = stream->posRead;
						stream->bufPos += size;
					}
				}
				//Mode = IO_MODE_LINE_BUFFER
				else
				{
					tmp[tmpLength] = '\0';
					char *substr = strrchr(tmp, '\n');
					if(substr == NULL)
						substr = tmp;
					if(strlen(substr) > 0)
					{
						memmove(stream->buffer + size, stream->buffer, stream->bufPos);
						memcpy(stream->buffer, substr, strlen(substr));
					}
				}
			}
			//Ansonsten laden wir sie in den Cache, wenn sie Datenmenge grösser ist
			else if(stream->bufPos < size && stream->bufMode == IO_MODE_FULL_BUFFER)
			{
				//Erst müssen wir den Cache leeren
				fflush(stream);
				memcpy(stream->buffer, tmp, MIN(stream->bufSize, size));
				stream->bufStart = stream->posRead;
				stream->bufPos = MIN(stream->bufSize, size);
			}

			free(tmp);
		}
		readData += cacheLength;
		//Daten hinter dem Cache laden
		if(stream->bufStart + stream->bufPos < stream->posRead + length)
		{
			tmpLength = stream->posRead + length - (stream->bufStart + stream->bufPos);
			tmp = malloc(tmpLength + 1);
			size_t size;
#ifdef BUILD_KERNEL
			size = vfs_Read(stream->stream_id, stream->posRead + readData, tmpLength, tmp);
#else
			size = syscall_fread(stream->stream_id, stream->posRead + readData, tmpLength, tmp);
#endif

			stream->eof = size < tmpLength;
			memcpy(ptr + readData, tmp, size);
			readData += size;

			//Wenn diese Daten direkt nach den Daten im Cache sind können wir diese einfach in den Cache laden
			if(stream->bufStart == (size_t)EOF
					|| (stream->bufStart + stream->bufPos >= stream->posRead && stream->bufStart + stream->bufPos < stream->posRead + length))
			{
				if(stream->bufMode == IO_MODE_FULL_BUFFER)
				{
					if(stream->bufPos + size <= stream->bufSize)
					{
						memcpy(stream->buffer + stream->bufPos, tmp, size);
						stream->bufPos += size;
					}
				}
				//Mode = IO_MODE_LINE_BUFFER
				else
				{
					//TODO
				}
			}
			//Ansonsten laden wir sie in den Cache, wenn sie Datenmenge grösser ist
			else if(stream->bufPos < size && stream->bufMode == IO_MODE_FULL_BUFFER)
			{
				//Erst müssen wir den Cache leeren
				fflush(stream);
				memcpy(stream->buffer, tmp, MIN(stream->bufSize, size));
				stream->bufStart = stream->posRead;
				stream->bufPos = MIN(stream->bufSize, size);
			}
			free(tmp);
		}
		stream->posRead += readData;
	}
	else
	{
#ifdef BUILD_KERNEL
		readData = vfs_Read(stream->stream_id, stream->posRead, length, ptr);
#else
		readData = syscall_fread(stream->stream_id, stream->posRead, length, ptr);
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

	//Überprüfen, ob überhaupt etwas geschrieben werden soll
	if(length == 0)
		return 0;

	if(stream->bufMode == IO_MODE_NO_BUFFER)
	{
#ifdef BUILD_KERNEL
		writeData = vfs_Write(stream->stream_id, stream->posWrite, length, ptr);
#else
		writeData = syscall_fwrite(stream->stream_id, stream->posWrite, length, ptr);
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
				size = vfs_Write(stream->stream_id, stream->bufStart, stream->bufPos + 1, stream->buffer);
#else
				size = syscall_fwrite(stream->stream_id, stream->bufStart, stream->bufPos + 1, stream->buffer);
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

	if(stream->bufMode != IO_MODE_NO_BUFFER && stream->mode.write && stream->bufStart != EOF && stream->bufDirty)
	{
#ifdef BUILD_KERNEL
		vfs_Write(stream->stream_id, stream->bufStart, stream->bufPos, stream->buffer);
#else
		syscall_fwrite(stream->stream_id, stream->bufStart, stream->bufPos, stream->buffer);
#endif
	}

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
		stream->bufDirty = false;
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
		stream->bufDirty = false;
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
		stream->bufDirty = false;
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
				size_t filesize = vfs_getFileinfo(stream->stream_id, VFS_INFO_FILESIZE);
#else
				size_t filesize = syscall_StreamInfo(stream->stream_id, VFS_INFO_FILESIZE);
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

int feof(FILE *stream)
{
	return stream->eof;
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
	if(stream == NULL || str == NULL || n <= 0)
		return NULL;

	while((c = fgetc(stream)) != EOF && i < (n - 1))
	{
		str[i++] = c;
		if(c == '\n')
			break;
	}
	str[i] = '\0';
	if(c == EOF)
		return NULL;
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
#ifdef BUILD_KERNEL
	int pos = vkprintf(format, arg);
#else
	int pos = vprintf(format, arg);
#endif
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
static int jprintf_putc(jprintf_args *args, char c)
{
	assert(args->putc != NULL);
	return args->putc(args->arg, c);
}

static int jprintf_putsn(jprintf_args *args, const char *str, int num)
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

		for(i = 0; (num == -1 || i < (unsigned int)num) && str[i] != '\0'; i++)
		{
			bytes_written += jprintf_putc(args, str[i]);
		}

		return bytes_written;
	}
}

static int jprintd(jprintf_args *args, double x, uint64_t prec, bool sign, bool space_sign, bool point, uint64_t width, char lpad)
{
	int n = 0;
	bool minus = signbit(x);

	int fptype = fpclassify(x);

	//Herausfinden, wieviele Stellen vor dem Dezimalpunkt sind
	size_t i = 0;
	switch(fptype)
	{
		case FP_INFINITE:
		case FP_NAN:
			i = 3;
		break;
		default:
			while(x >= 10)
			{
				i++;
				x /= 10.0;
			}
	}

	size_t length = i + ((minus || sign || space_sign) ? 2 : 1) + ((prec) ? prec + 1 : point);
	if(minus) x = -x;

	while(width-- > length)
		jprintf_putc(args, lpad);

	//Minuszeichen
	if(minus)
	{
		jprintf_putc(args, '-');
		n++;
	}
	else
	{
		if(sign)
		{
			jprintf_putc(args, '+');
			n++;
		}
		else if(space_sign)
		{
			jprintf_putc(args, ' ');
			n++;
		}
	}

	//Zahlen ausgeben
	switch(fptype)
	{
		case FP_NAN:
			n += jprintf_putsn(args, "nan", 4);
		break;
		case FP_INFINITE:
			n += jprintf_putsn(args, "inf", 4);
		break;
		default:
			while(i-- + prec + 1)
			{
				jprintf_putc(args, ((uint64_t)x % 10) + '0');
				n++;
				x -= (uint64_t)x;
				x *= 10.0;
				if(i == (uint64_t)-1)
				{
					if(point || prec)
					{
						jprintf_putc(args, '.');
						n++;
					}
				}
			}
	}

	return n;
}

static int jprintld(jprintf_args *args, long double x, uint64_t prec, bool sign, bool space_sign, bool point, uint64_t width, char lpad)
{
	int n = 0;
	bool minus = signbit(x);

	int fptype = fpclassify(x);

	//Herausfinden, wieviele Stellen vor dem Dezimalpunkt sind
	size_t i = 0;
	switch(fptype)
	{
		case FP_INFINITE:
		case FP_NAN:
			i = 3;
		break;
		default:
			while(x >= 10)
			{
				i++;
				x /= 10.0;
			}
	}

	size_t length = i + ((minus || sign || space_sign) ? 2 : 1) + ((prec) ? prec + 1 : point);
	if(minus) x = -x;

	while(width-- > length)
		jprintf_putc(args, lpad);

	//Minuszeichen
	if(minus)
	{
		jprintf_putc(args, '-');
		n++;
	}
	else
	{
		if(sign)
		{
			jprintf_putc(args, '+');
			n++;
		}
		else if(space_sign)
		{
			jprintf_putc(args, ' ');
			n++;
		}
	}

	//Zahlen ausgeben
	switch(fptype)
	{
		case FP_NAN:
			n += jprintf_putsn(args, "nan", 4);
		break;
		case FP_INFINITE:
			n += jprintf_putsn(args, "inf", 4);
		break;
		default:
			while(i-- + prec + 1)
			{
				jprintf_putc(args, ((uint64_t)x % 10) + '0');
				n++;
				x -= (uint64_t)x;
				x *= 10.0;
				if(i == (uint64_t)-1)
				{
					if(point || prec)
					{
						jprintf_putc(args, '.');
						n++;
					}
				}
			}
	}

	return n;
}

static int jvprintf(jprintf_args *args, const char *format, va_list arg)
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
								value = va_arg(arg, int);
							break;
							case -1:
								value = va_arg(arg, int);
							break;
							case 0: default:
								value = va_arg(arg, int);
							break;
							case 1:
								value = va_arg(arg, long);
							break;
							case 2:
								value = va_arg(arg, long long);
							break;
							case 3:
								value = va_arg(arg, int64_t);
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
						if(!precision_spec)
							precision = 6;
						if(length == 0 || length == 1)
						{
							double value = va_arg(arg, double);
							pos += jprintd(args, value, precision, sign, space_sign, alt, width, lpad);
						}
						else if(length == 6)
						{
							long double value = va_arg(arg, long double);
							pos += jprintld(args, value, precision, sign, space_sign, alt, width, lpad);
						}
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
static int vasprintf_putc(void *arg, char c)
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
//Callback-Funktionen
static int vfprintf_putc(void *arg, char c)
{
	return fwrite(&c, 1, sizeof(char), arg);
}

static int vfprintf_putsn(void *arg, const char *str, int n)
{
	size_t len = strlen(str);
	return fwrite(str, MIN(len, (uint64_t)n), sizeof(char), arg);
}

int vfprintf(FILE *stream, const char *format, va_list arg)
{
	jprintf_args handler = {
			.arg = stream,
			.putc = vfprintf_putc,
			.putsn = vfprintf_putsn
	};

	return jvprintf(&handler, format, arg);
}

//Callback-Funktionen
static int vprintf_putc(void *arg, char c)
{
	putchar(c);
	return 1;
}

static int vprintf_putsn(void *arg, const char *str, int n)
{
	int len = strlen(str);

	if(len <= n || n == -1)
	{
		return putsn(strlen(str), str);
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

#ifdef BUILD_KERNEL
//Callback-Funktionen
static int vkprintf_putc(void *arg, char c)
{
	console_ansi_write(&initConsole, c);
	return 1;
}

int vkprintf(const char *format, va_list arg)
{
	jprintf_args handler = {
			.putc = &vkprintf_putc
	};

	return jvprintf(&handler, format, arg);
}
#endif

//Callback-Funktionen
typedef struct{
	char *str;
	size_t size;
}vsprintf_t;
static int vsprintf_putc(void *arg, char c)
{
	vsprintf_t *args = arg;

	args->str[args->size++] = c;

	return 1;
}

int vsprintf(char *str, const char *format, va_list arg)
{
	vsprintf_t args = {
			.str = str
	};

	jprintf_args handler = {
			.putc = &vsprintf_putc,
			.arg = &args
	};

	int retval = jvprintf(&handler, format, arg);

	//String null-terminieren
	args.str[args.size] = '\0';

	return retval;
}

static void jscanf_ungetc(jscanf_args *arg, int c)
{
	arg->ungetc(arg->arg, c);
}

static int jscanf_getc(jscanf_args *arg)
{
	return arg->getc(arg->arg);
}

static int jscanf_tell(jscanf_args *arg)
{
	return arg->tell(arg->arg);
}

static size_t jscanf_readNumber(jscanf_args *args, char *buffer, size_t size, uint8_t base, int *eof)
{
	size_t first_digit = 0;
	size_t i;
	for(i = 0; i < size; i++)
	{
		int c;
		bool valid = false;
		buffer[i] = c = jscanf_getc(args);
		if(c == EOF)
		{
			*eof = 1;
			break;
		}
		switch(buffer[i])
		{
			case '+': case '-':
				if(i == 0)
				{
					valid = true;
					first_digit++;
				}
			break;
			case '0':
				valid = true;
			break;
			case '1' ... '7':
				if(base == 0)
					base = (buffer[first_digit] == '0') ? 8 : 10;
				valid = true;
			break;
			case '8' ... '9':
				if(base == 0)
					base = (buffer[first_digit] == '0') ? 8 : 10;
				valid = (base != 8);
			break;
			case 'a' ... 'f': case 'A' ... 'F':
				valid = (base == 16);
			break;
			case 'x':
				if(base == 0)
					base = 16;
				if(base == 16 && i == first_digit + 1 && buffer[first_digit] == '0')
					valid = true;
			break;
		}

		if(!valid)
		{
			jscanf_ungetc(args, c);
			break;
		}
	}
	buffer[i] = '\0';

	return i;
}

static void jscanf_assignNumber(void *ptr, uint64_t value, int8_t size)
{
	switch(size)
	{
		case -2:
			*((uint8_t*)ptr) = (uint8_t)value;
		break;
		case -1:
			*((uint16_t*)ptr) = (uint16_t)value;
		break;
		case 0:
			*((uint32_t*)ptr) = (uint32_t)value;
		break;
		case 1 ... 5:
			*((uint64_t*)ptr) = value;
		break;
	}
}

static int jvscanf(jscanf_args *args, const char *format, va_list arg)
{
	int8_t length;
	size_t width;
	bool assign;
	int c, base, eof;
	char buffer[25];
	char *end;
	uint64_t value;
	int ret = 0;
	for(; *format; format++)
	{
		switch(*format)
		{
			case ' ':
			case '\n':
			case '\t':
			case '\f':
			case '\v':
				do
				{
					c = jscanf_getc(args);
				} while(isspace(c));

				if(c != EOF)
				{
					jscanf_ungetc(args, c);
				}
			break;
			case '%':	//Formatieren?
				format++;

				//Ein * bedeuted, dass der Wert eingelesen aber nicht zugewiesen wird
				if(*format == '*')
				{
					assign = false;
					format++;
				}
				else
					assign = true;

				if(isdigit(*format))
				{
					width = strtol(format, (char**)&format, 10);
					if(width == 0)
						return ret;
				}
				else
					width = 0;

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

				//Whitespaces überspringen (ausser %[, %c, %n)
				if(*format != '[' && *format != 'c' && *format != 'n')
				{
					while(isspace(c = jscanf_getc(args)));
					if(c != EOF)
						jscanf_ungetc(args, c);
				}

				switch(*format)
				{
					case 'i': case 'u':	//Unsigned int
						base = 0;
						goto convert_number;
					case 'd':
						base = 10;
						goto convert_number;
					case 'o':
						base = 8;
						goto convert_number;
					case 'x': case 'X':
						base = 16;
						convert_number:
						if(width == 0 || width > sizeof(buffer))
							width = sizeof(buffer) - 1;
						width = jscanf_readNumber(args, buffer, width, base, &eof);
						value = strtol(buffer, &end, base);
						if(eof && width == 0 && ret == 0)
							return EOF;
						if(assign)
						{
							jscanf_assignNumber(va_arg(arg, void*), value, length);
							ret++;
						}
					break;
					case 'n':	//Bisher gelesene Zeichen
						if(assign)
							jscanf_assignNumber(va_arg(arg, void*), jscanf_tell(args), length);
					break;
					case 's':	//String
					{
						char *ptr;
						bool matched = false;
						if(width == 0)
							width = -1;
						if(assign)
							ptr = va_arg(arg, char*);

						while(width--)
						{
							c = jscanf_getc(args);
							if(isspace(c))
							{
								jscanf_ungetc(args, c);
								break;
							}
							else if(c == EOF)
								break;

							matched = true;

							if(assign)
								*ptr++ = c;
						}

						if(!matched)
						{
							if(ret == 0)
								return EOF;
							return ret;
						}
						if(assign)
						{
							*ptr = '\0';
							ret++;
						}
					}
					break;
					case 'c':	//Char
					{
						char *ptr;
						if(assign)
							ptr = va_arg(arg, char*);
						if(width == 0)
							width = 1;
						while(width--)
						{
							c = jscanf_getc(args);
							if(c == EOF)
							{
								if(ret == 0)
									return EOF;
								return ret;
							}
							if(assign)
								ptr[width] = c;
							ret += assign;
						}
					}
					break;
					default:	//Ansonsten ungültig
						format--;
						//pos--;
					break;
				}
			break;
			default:	//Ansonsten schreibe das aktuelle Zeichen
				c = jscanf_getc(args);
				if(c != *format)
					return ret;
			break;
		}
	}
	return ret;
}

//scanf-Funktionen
int fscanf(FILE *stream, const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	int pos = vfscanf(stream, format, arg);
	va_end(arg);
	return pos;
}

int scanf(const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	int pos = vscanf(format, arg);
	va_end(arg);
	return pos;
}

int sscanf(const char *str, const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	int pos = vsscanf(str, format, arg);
	va_end(arg);
	return pos;
}

static int vfscanf_getc(void *ptr)
{
	FILE *stream = ptr;
	return fgetc(stream);
}

static void vfscanf_ungetc(void *ptr, int c)
{
	FILE *stream = ptr;
	ungetc(c, stream);
}

static int vfscanf_tell(void *ptr)
{
	FILE *stream = ptr;
	return ftell(stream);
}

int vfscanf(FILE *stream, const char *format, va_list arg)
{
	jscanf_args args = {
			.arg = stream,
			.getc = vfscanf_getc,
			.ungetc = vfscanf_ungetc,
			.tell = vfscanf_tell
	};

	return jvscanf(&args, format, arg);
}

int vscanf(const char *format, va_list arg)
{
	return vfscanf(stdin, format, arg);
}

typedef struct{
	char *str;
	size_t pos;
}vsscanf_args;

static int vsscanf_getc(void *ptr)
{
	vsscanf_args *arg = ptr;
	if(arg->str[arg->pos])
		return arg->str[arg->pos++];
	return EOF;
}

static void vsscanf_ungetc(void *ptr, int c)
{
	vsscanf_args *arg = ptr;
	if(arg->pos > 0)
		arg->pos--;
}

static int vsscanf_tell(void *ptr)
{
	vsscanf_args *arg = ptr;
	return arg->pos;
}

int vsscanf(const char *str, const char *format, va_list arg)
{
	vsscanf_args scanf_args = {
			.pos = 0,
			.str = (char*)str
	};
	jscanf_args args = {
			.arg = &scanf_args,
			.getc = vsscanf_getc,
			.ungetc = vsscanf_ungetc,
			.tell = vsscanf_tell
	};

	return jvscanf(&args, format, arg);
}

//I/O-Funktionen
int ungetc(int c, FILE *stream)
{
	if(stream == NULL)
		return EOF;

	char *new_ptr = realloc(stream->ungetch_buffer, stream->ungetch_count + 1);
	if(new_ptr == NULL)
		return EOF;
	stream->ungetch_buffer = new_ptr;
	stream->ungetch_buffer[stream->ungetch_count++] = (char)c;
	return c;
}

int getc(FILE *stream)
{
	unsigned char c;
	if(fread(&c, sizeof(unsigned char), 1, stream) == 0)
		return EOF;
	return c;
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
	return fwrite(&zeichen, sizeof(unsigned char), 1, stream);
}

int putchar(int zeichen)
{
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
	//Newline ausgeben
	if(putc('\n', stdout) == EOF)
		return EOF;
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

//Gibt n Zeichen aus. Nullbytes werden ignoriert
int putsn(size_t n, const char *str)
{
	size_t i;
	for(i = 0; i < n; i++)
	{
		putc(str[i], stdout);
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
