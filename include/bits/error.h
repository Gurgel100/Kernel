/*
 * error.h
 *
 *  Created on: 20.03.2017
 *      Author: pascal
 */

#ifndef ERROR_H_
#define ERROR_H_

#include "stdint.h"

#define ERROR_STRUCT(type) \
	struct{ \
		type value; \
		error_t error; \
	}

#define ERROR_TYPE(type)							error_##type##_t
#define ERROR_TYPE_POINTER(type)					error_##type##p##_t

/**
 * Defines a new type which can hold an error code
 */
#define ERROR_TYPEDEF(type)							typedef ERROR_STRUCT(type) ERROR_TYPE(type)
#define ERROR_TYPEDEF_POINTER(type)					typedef ERROR_STRUCT(type*) ERROR_TYPE_POINTER(type)

#define ERROR_RETURN(type, value, error)			((ERROR_TYPE(type)){value, error})
#define ERROR_RETURN_VALUE(type, value)				ERROR_RETURN(type, value, E_NONE)
#define ERROR_RETURN_ERROR(type, error)				ERROR_RETURN(type, (type)0, error)

#define ERROR_RETURN_POINTER(type, value, error)	((ERROR_TYPE_POINTER(type)){value, error})
#define ERROR_RETURN_POINTER_VALUE(type, value)		ERROR_RETURN_POINTER(type, value, E_NONE)
#define ERROR_RETURN_POINTER_ERROR(type, error)		ERROR_RETURN_POINTER(type, (type*)0, error)

#define ERROR_RETURN_VOID()							(E_NONE)
#define ERROR_RETURN_VOID_ERROR(error)				(error)

#define ERROR_DETECT(ret)							(__builtin_expect(ret.error != E_NONE, 0))
#define ERROR_DETECT_VOID(ret)						(__builtin_expect(ret != E_NONE, 0))
#define ERROR_GET_ERROR(ret)						(ret.error)
#define ERROR_GET_VALUE(ret)						(ret.value)

#define ERROR_MESSAGE(error)						(error_messages[error])

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

	_E_NUM
} error_t;

typedef error_t ERROR_TYPE(void);
ERROR_TYPEDEF_POINTER(void);

ERROR_TYPEDEF(char);
ERROR_TYPEDEF_POINTER(char);

ERROR_TYPEDEF(int8_t);
ERROR_TYPEDEF_POINTER(int8_t);
ERROR_TYPEDEF(int16_t);
ERROR_TYPEDEF(int32_t);
ERROR_TYPEDEF(int64_t);

ERROR_TYPEDEF(uint8_t);
ERROR_TYPEDEF_POINTER(uint8_t);
ERROR_TYPEDEF(uint16_t);
ERROR_TYPEDEF(uint32_t);
ERROR_TYPEDEF(uint64_t);

ERROR_TYPEDEF(size_t);

static const char *const error_messages[] = {
/*E_NONE*/					"No error",
/*E_INVALID_ARGUMENT*/		"Invalid arguments",
/*E_ACCESS*/				"Permission denied",
/*E_AGAIN*/					"Resource unavailable, try again",
/*E_BAD_FILE_DESCRIPTOR*/	"Bad file descriptor",
/*E_BUSY*/					"Resource busy",
/*E_CANCELED*/				"Operation canceled",
/*E_CHILD*/					"No child process",
/*E_EXISTS*/				"File exists",
/*E_FAULT*/					"Bad address",
/*E_FILE_TO_BIG*/			"File too large",
/*E_IO*/					"I/O error",
/*E_IS_DIR*/				"Is a directory",
/*E_NO_EXECUTABLE*/			"Executable file format error",
/*E_NO_MEMORY*/				"Not enough memory",
/*E_OVERFLOW*/				"Value too large to be stored in data type",
/*E_NOT_DIR*/				"Not a directory or a symbolic link to a directory",
/*E_NOT_SUPPORTED*/			"Operation not supported",
/*E_NO_PERMISSION*/			"Operation not permitted",
/*E_READ_ONLY_FILESYSTEM*/	"Read-only file system",
};

#endif /* ERROR_H_ */
