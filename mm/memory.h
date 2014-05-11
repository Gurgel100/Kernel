/*
 * memory.h
 *
 *  Created on: 31.12.2012
 *      Author: pascal
 */

#ifndef MEMORY_H_
#define MEMORY_H_

//Kernelspace
#define KERNELSPACE_START	0x0							//Kernelspace Anfang
#define KERNELSPACE_END		(0x100000000 - 1)			//Kernelspace Ende (4GB)

//Userspace
#define USERSPACE_START		KERNELSPACE_END + 1			//Userspace Anfang
#define USERSPACE_END		0xFFFFFEFFFFFFFFFF		//Userspace Ende
#define MAX_ADDRESS			0xFFFFFFFFFFFFFFFF		//Maximale Adresse

#define MM_BLOCK_SIZE		4096						//Eine Blockgrösse

#endif /* MEMORY_H_ */
