/*
 * syscall.h
 *
 *  Created on: 28.10.2014
 *      Author: pascal
 */

#ifndef BUILD_KERNEL

#ifndef SYSCALL_H_
#define SYSCALL_H_


inline void *AllocPage(size_t Pages);
inline void FreePage(void *Address, size_t Pages);

inline char syscall_getch();
inline void syscall_putch(unsigned char c);

#endif /* SYSCALL_H_ */

#endif
