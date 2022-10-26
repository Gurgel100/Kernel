/*
 * memory.h
 *
 *  Created on: 31.12.2012
 *      Author: pascal
 */

#ifndef MEMORY_H_
#define MEMORY_H_

//Kernelspace
#define KERNELSPACE_START	0x1000						//Kernelspace Anfang
#define KERNELSPACE_END		(0x8000000000 - 1)			//Kernelspace Ende (512GB)

//Userspace
#define USERSPACE_START		(KERNELSPACE_END + 1)	//Userspace Anfang
#define USERSPACE_END		0xFFFFFF7FFFFFFFFF		//Userspace Ende
#define MAX_ADDRESS			0xFFFFFFFFFFFFFFFF		//Maximale Adresse

#define MM_KERN_STACK_SIZE	(1 * MM_BLOCK_SIZE)		//Stackgrösse für einen Thread im Kernelspace

#define MM_USER_STACK		USERSPACE_END			//Stackaddresse für Prozesse
#define MM_USER_STACK_SIZE	(4 * MM_BLOCK_SIZE)		//Stackgrösse

#define MM_GUARD_PAGES      1                       // Number of guard pages

#define MM_BLOCK_SIZE		4096						//Eine Blockgrösse

#endif /* MEMORY_H_ */
