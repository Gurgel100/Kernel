/*
 * error.h
 *
 *  Created on: 20.03.2017
 *      Author: pascal
 */

#ifndef ERROR_H_
#define ERROR_H_

#include "stdint.h"

/**
 * Defined a new type which can hold an error code
 */
#define ERROR_TYPEDEF(type) \
	typedef struct{ \
		type value; \
		error_t error; \
	} ERROR_TYPE(type)

#define ERROR_TYPE(type)					error_##type##_t

#define ERROR_RETURN(type, value, error)	((ERROR_TYPE(type)){value, error})
#define ERROR_RETURN_VOID(error)			(error)

#define ERROR_DETECT(ret)					(__builtin_expect(ret.error != E_NONE, 0))
#define ERROR_DETECT_VOID(ret)				(__builtin_expect(ret != E_NONE, 0))
#define ERROR_ERROR_VALUE(ret)				(ret.error)
#define ERROR_RETURN_VALUE(ret)				(ret.value)

typedef void*		void_p;
typedef char*		char_p;
typedef int8_t*		int8_t_p;
typedef uint8_t*	uint8_t_p;

//Errorcodes
typedef enum{
	///No error
	E_NONE = 0,

	///Invalid arguments
	E_INVALID_ARGUMENT,

	///Permission denied
	E_ACCESS,

	///Resource unavailable, try again
	E_AGAIN,

	///Bad file descriptor
	E_BAD_FILE_DESCRIPTOR,

	///Resource busy
	E_BUSY,

	///Operation canceled
	E_CANCELED,

	///No child process
	E_CHILD,

	///File exists
	E_EXISTS,

	///Bad address
	E_FAULT,

	///File too large
	E_FILE_TO_BIG,

	///I/O error
	E_IO,

	///Is a directory
	E_IS_DIR,

	///Executable file format error
	E_NO_EXECUTABLE,

	///Not enough memory
	E_NO_MEMORY,

	///Value too large to be stored in data type
	E_OVERFLOW,

	///Not a directory or a symbolic link to a directory
	E_NOT_DIR,

	///Operation not supported
	E_NOT_SUPPORTED,

	//Operation not permitted
	E_NO_PERMISSION,

	///Read-only file system
	E_READ_ONLY_FILESYSTEM,
} error_t;

typedef error_t ERROR_TYPE(void);
ERROR_TYPEDEF(void_p);

ERROR_TYPEDEF(char);
ERROR_TYPEDEF(char_p);

ERROR_TYPEDEF(int8_t);
ERROR_TYPEDEF(int8_t_p);
ERROR_TYPEDEF(int16_t);
ERROR_TYPEDEF(int32_t);
ERROR_TYPEDEF(int64_t);

ERROR_TYPEDEF(uint8_t);
ERROR_TYPEDEF(uint8_t_p);
ERROR_TYPEDEF(uint16_t);
ERROR_TYPEDEF(uint32_t);
ERROR_TYPEDEF(uint64_t);

ERROR_TYPEDEF(size_t);

#endif /* ERROR_H_ */
