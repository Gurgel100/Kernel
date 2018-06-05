/*
 * cpu.h
 *
 *  Created on: 13.07.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#ifndef CPU_H_
#define CPU_H_

#include "stdint.h"
#include "stdbool.h"

#define INTEL	200
#define AMD		201
#define UNKN	202
typedef uint8_t CPU_VENDOR;

typedef struct{
	uint32_t eax, ebx, ecx, edx;
}cpu_cpuid_result_t;

struct{
		char Name[49];
		char VendorID[13];		//Vendor-ID-String
		CPU_VENDOR Vendor;
		uint8_t Stepping;
		uint8_t Model;
		uint8_t Family;
		uint8_t Type;
		uint8_t extModel;
		uint8_t extFamily;
		uint32_t maxstdCPUID;	//Maximale standard CPUID-Funktion
		uint32_t maxextCPUID;	//Maximale erweiterte CPUID-Funktion
		uint8_t NumCores;
		bool GlobalPage;		//1: Globale Pages werden unterstützt
		bool cpuidAvailable;
		bool HyperThreading;
		bool sse3;
		bool ssse3;
		bool sse4_1;
		bool sse4_2;
		bool avx;
		bool aes;
		bool rdrand;
		bool nx;
		bool syscall;
		bool xsave;				//XGETBV, XRSTOR, XSAVE, XSETBV
}cpuInfo;

typedef enum{
	CPU_CR0, CPU_CR2, CPU_CR3, CPU_CR4, CPU_CR8, CPU_XCR0
}cpu_control_register_t;

void cpu_init(bool isBSP);
cpu_cpuid_result_t cpu_cpuid(uint32_t function);
cpu_cpuid_result_t cpu_cpuidSubleaf(uint32_t function, uint32_t subleaf);
uint64_t cpu_MSRread(uint32_t msr);
void cpu_MSRwrite(uint32_t msr, uint64_t Value);

inline uint64_t cpu_readControlRegister(cpu_control_register_t reg)
{
	uint64_t val;

	switch(reg)
	{
		case CPU_CR0:
			asm volatile("mov %%cr0, %0": "=r"(val));
			break;
		case CPU_CR2:
			asm volatile("mov %%cr2, %0": "=r"(val));
			break;
		case CPU_CR3:
			asm volatile("mov %%cr3, %0": "=r"(val));
			break;
		case CPU_CR4:
			asm volatile("mov %%cr4, %0": "=r"(val));
			break;
		case CPU_CR8:
			asm volatile("mov %%cr8, %0": "=r"(val));
			break;
		case CPU_XCR0:
		{
			uint32_t lo, hi;
			asm volatile("xgetbv": "=a"(lo), "=d"(hi): "c"(0));
			val = lo | ((uint64_t)hi << 32);
			break;
		}
		default:
			asm volatile("ud2");
	}

	return val;
}

inline void cpu_writeControlRegister(cpu_control_register_t reg, uint64_t val)
{
	switch(reg)
	{
		case CPU_CR0:
			asm volatile("mov %0, %%cr0":: "r"(val));
			break;
		case CPU_CR2:
			asm volatile("mov %0, %%cr2":: "r"(val));
			break;
		case CPU_CR3:
			asm volatile("mov %0, %%cr3":: "r"(val));
			break;
		case CPU_CR4:
			asm volatile("mov %0, %%cr4":: "r"(val));
			break;
		case CPU_CR8:
			asm volatile("mov %0, %%cr8":: "r"(val));
			break;
		case CPU_XCR0:
			asm volatile("xsetbv":: "a"((uint32_t)val), "d"(val >> 32), "c"(0));
			break;
		default:
			asm volatile("ud2");
	}
}

inline void cpu_saveXState(void *ptr)
{
	if(cpuInfo.xsave)
		asm volatile("xsave %0": "=m"(*(char*)ptr): "a"(-1), "d"(0));
	else
		asm volatile("fxsave %0": "=m"(*(char*)ptr));
}

inline void cpu_restoreXState(void *ptr)
{
	if(cpuInfo.xsave)
		asm volatile("xrstor %0":: "m"(*(char*)ptr), "a"(-1), "d"(0));
	else
		asm volatile("fxrstor %0":: "m"(*(char*)ptr));
}

#endif /* CPU_H_ */

#endif
