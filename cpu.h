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

#define EAX 100
#define EBX 101
#define ECX 102
#define EDX 103
typedef enum{
	CR_EAX = 100,
	CR_EBX = 101,
	CR_ECX = 102,
	CR_EDX = 103
}CPU_REGISTER;

#define INTEL	200
#define AMD		201
#define UNKN	202
typedef uint8_t CPU_VENDOR;

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
		bool msrAvailable;
		bool mmx;
		bool sse;
		bool sse2;
		bool sse3;
		bool ssse3;
		bool sse4_1;
		bool sse4_2;
		bool avx;
		bool aes;
		bool rdrand;
		bool nx;
}cpuInfo;

void cpu_Init(void);
uint32_t cpu_CPUID(uint32_t Funktion, CPU_REGISTER Register);
uint64_t cpu_MSRread(uint32_t msr);
void cpu_MSRwrite(uint32_t msr, uint64_t Value);

#endif /* CPU_H_ */

#endif
