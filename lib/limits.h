/*
 * limits.h
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#ifndef LIMITS_H_
#define LIMITS_H_

#ifdef __cplusplus
extern "C" {
#endif

//Grösse der Datentypen
#define CHAR_BIT	8						//Anzahl Bits in einem Byte
#define SCHAR_MIN	(-128)					//Kleinster Wert eines signed char
#define SCHAR_MAX	127						//Grösster Wert eines signed char
#define UCHAR_MAX	255						//Grösster Wert eines unsigned char
#define CHAR_MIN	SCHAR_MIN				//Kleinster Wert eines char
#define CHAR_MAX	SCHAR_MAX				//Grösster Wert eines char

//TODO
#define MB_LEN_MAX	1						//?

#define SHRT_MIN	(-32768)				//Kleinster Wert eines short int
#define SHRT_MAX	32767					//Grösster Wert eines short int
#define USHRT_MAX	65535					//Grösster Wert eines unsigned short int

#define	INT_MIN		(-2147483648)			//Kleinster Wert eines int
#define	INT_MAX		2147483647				//Grösster Wert eines int
#define	UINT_MAX	4294967295U				//Grösster Wert eines unsigned int

#define	LONG_MIN	(-9223372036854775808)	//Kleinster Wert eines long int
#define	LONG_MAX	9223372036854775807		//Grösster Wert eines long int
#define	ULONG_MAX	18446744073709551615	//Grösster Wert eines unsigned long int

#ifdef __cplusplus
}
#endif

#endif /* LIMITS_H_ */
