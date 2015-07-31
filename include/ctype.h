/*
 * ctype.h
 *
 *  Created on: 28.07.2012
 *      Author: pascal
 */

#ifndef CTYPE_H_
#define CTYPE_H_

extern int isalnum(int c);
extern int isalpha(int c);
extern int isdigit(int c);
extern int isspace(int c);
extern int isxdigit(int c);

extern int iscntrl(int c);
extern int isgraph(int c);
extern int isprint(int c);
extern int ispunct(int c);

extern int islower(int c);
extern int isupper(int c);

extern int tolower(int c);
extern int toupper(int c);

#endif /* CTYPE_H_ */
