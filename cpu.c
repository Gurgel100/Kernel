/*
 * cpu.c
 *
 *  Created on: 13.07.2012
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#include "cpu.h"
#include "config.h"
#include "display.h"

#define NULL (void*)0

/*
 * Initialisiert die CPU
 */
//TODO
void cpu_Init()
{
	uint64_t Result;
	//Überprüfe, ob CPU CPUID unterstützt
	asm volatile(
			"pushfq;"
			"pop %%rcx;"
			"mov %%rcx,%%rax;"
			"xor $0x200000,%%rax;"
			"push %%rax;"
			"popfq;"
			"pushfq;"
			"pop %%rax;"
			"sub %%rcx,%%rax;"
			"mov %%rax,%0;"
			:"=rm" (Result) : :"%rcx", "%rax"
	);
	cpuInfo.cpuidAvailable = (Result == 0) ? false : true;

	cpu_cpuid_result_t cpuid_0 = cpu_cpuid(0x00000000);
	cpuInfo.maxstdCPUID = cpuid_0.eax;
	cpuInfo.VendorID[0] = (uint8_t)cpuid_0.ebx;
	cpuInfo.VendorID[1] = (uint8_t)(cpuid_0.ebx >> 8);
	cpuInfo.VendorID[2] = (uint8_t)(cpuid_0.ebx >> 16);
	cpuInfo.VendorID[3] = (uint8_t)(cpuid_0.ebx >> 24);
	cpuInfo.VendorID[4] = (uint8_t)cpuid_0.edx;
	cpuInfo.VendorID[5] = (uint8_t)(cpuid_0.edx >> 8);
	cpuInfo.VendorID[6] = (uint8_t)(cpuid_0.edx >> 16);
	cpuInfo.VendorID[7] = (uint8_t)(cpuid_0.edx >> 24);
	cpuInfo.VendorID[8] = (uint8_t)cpuid_0.ecx;
	cpuInfo.VendorID[9] = (uint8_t)(cpuid_0.ecx >> 8);
	cpuInfo.VendorID[10] = (uint8_t)(cpuid_0.ecx >> 16);
	cpuInfo.VendorID[11] = (uint8_t)(cpuid_0.ecx >> 24);
	cpuInfo.VendorID[12] = '\0';
	//Herausfinden ob Intel oder AMD Prozessor
	if(cpuInfo.VendorID[11] == 'l')
		cpuInfo.Vendor = INTEL;
	else if(cpuInfo.VendorID[11] == 'D')
		cpuInfo.Vendor = AMD;
	else
		cpuInfo.Vendor = UNKN;

	//Prozessor
	cpu_cpuid_result_t cpuid_1 = cpu_cpuid(0x00000001);

	cpuInfo.Stepping = cpuid_1.eax & 0xF;
	cpuInfo.Model = cpuid_1.eax & 0xF0;
	cpuInfo.Family = cpuid_1.eax & 0xF00;
	if(cpuInfo.Vendor == INTEL)
		cpuInfo.Type = cpuid_1.eax & 0x3000;
	cpuInfo.extModel = cpuid_1.eax & 0xF0000;
	cpuInfo.extFamily = cpuid_1.eax & 0xFF00000;

	cpuInfo.NumCores = cpuid_1.ebx & 0xFF0000;	//Ungülitg, wenn HyperThreading nicht unterstützt

	//Featureflags Teil 1
	cpuInfo.sse3 = cpuid_1.ecx & 0x1;
	cpuInfo.ssse3 = cpuid_1.ecx & 0x200;
	cpuInfo.sse4_1 = cpuid_1.ecx & 0x80000;
	if(cpuInfo.Vendor == INTEL)
	{
		cpuInfo.sse4_2 = cpuid_1.ecx & 0x100000;
		cpuInfo.aes = cpuid_1.ecx & 0x2000000;
		cpuInfo.avx = cpuid_1.ecx & 0x10000000;
		cpuInfo.rdrand = cpuid_1.ecx & 0x40000000;
	}

	//Featureflags Teil 2
	cpuInfo.msrAvailable = cpuid_1.edx & 0x20;
	cpuInfo.GlobalPage = cpuid_1.edx & 0x2000;
	cpuInfo.mmx = cpuid_1.edx & 0x800000;
	cpuInfo.sse = cpuid_1.edx & 0x2000000;
	cpuInfo.sse2 = cpuid_1.edx & 0x4000000;
	cpuInfo.HyperThreading = cpuid_1.edx & 0x10000000;
	cpuInfo.fxsr = cpuid_1.edx & (1 << 24);

	//Erweiterte Funktionen
	cpuInfo.maxextCPUID = cpu_cpuid(0x80000000).eax;

	cpu_cpuid_result_t cpuid_80000001 = cpu_cpuid(0x80000001);
	cpuInfo.nx = cpuid_80000001.edx & (1 << 20);
	cpuInfo.syscall = cpuid_80000001.edx & (1 << 11);

	//Namen des Prozessors
	if(cpuInfo.maxextCPUID >= 0x80000003 && cpuInfo.Vendor == INTEL)
	{
		cpu_cpuid_result_t cpuid_80000002 = cpu_cpuid(0x80000002);
		cpuInfo.Name[0] = (uint8_t)cpuid_80000002.eax;
		cpuInfo.Name[1] = (uint8_t)(cpuid_80000002.eax >> 8);
		cpuInfo.Name[2] = (uint8_t)(cpuid_80000002.eax >> 16);
		cpuInfo.Name[3] = (uint8_t)(cpuid_80000002.eax >> 24);
		cpuInfo.Name[4] = (uint8_t)cpuid_80000002.ebx;
		cpuInfo.Name[5] = (uint8_t)(cpuid_80000002.ebx >> 8);
		cpuInfo.Name[6] = (uint8_t)(cpuid_80000002.ebx >> 16);
		cpuInfo.Name[7] = (uint8_t)(cpuid_80000002.ebx >> 24);
		cpuInfo.Name[8] = (uint8_t)cpuid_80000002.ecx;
		cpuInfo.Name[9] = (uint8_t)(cpuid_80000002.ecx >> 8);
		cpuInfo.Name[10] = (uint8_t)(cpuid_80000002.ecx >> 16);
		cpuInfo.Name[11] = (uint8_t)(cpuid_80000002.ecx >> 24);
		cpuInfo.Name[12] = (uint8_t)cpuid_80000002.edx;
		cpuInfo.Name[13] = (uint8_t)(cpuid_80000002.edx >> 8);
		cpuInfo.Name[14] = (uint8_t)(cpuid_80000002.edx >> 16);
		cpuInfo.Name[15] = (uint8_t)(cpuid_80000002.edx >> 24);

		cpu_cpuid_result_t cpuid_80000003 = cpu_cpuid(0x80000003);
		cpuInfo.Name[16] = (uint8_t)cpuid_80000003.eax;
		cpuInfo.Name[17] = (uint8_t)(cpuid_80000003.eax >> 8);
		cpuInfo.Name[18] = (uint8_t)(cpuid_80000003.eax >> 16);
		cpuInfo.Name[19] = (uint8_t)(cpuid_80000003.eax >> 24);
		cpuInfo.Name[20] = (uint8_t)cpuid_80000003.ebx;
		cpuInfo.Name[21] = (uint8_t)(cpuid_80000003.ebx >> 8);
		cpuInfo.Name[22] = (uint8_t)(cpuid_80000003.ebx >> 16);
		cpuInfo.Name[23] = (uint8_t)(cpuid_80000003.ebx >> 24);
		cpuInfo.Name[24] = (uint8_t)cpuid_80000003.ecx;
		cpuInfo.Name[25] = (uint8_t)(cpuid_80000003.ecx >> 8);
		cpuInfo.Name[26] = (uint8_t)(cpuid_80000003.ecx >> 16);
		cpuInfo.Name[27] = (uint8_t)(cpuid_80000003.ecx >> 24);
		cpuInfo.Name[28] = (uint8_t)cpuid_80000003.edx;
		cpuInfo.Name[29] = (uint8_t)(cpuid_80000003.edx >> 8);
		cpuInfo.Name[30] = (uint8_t)(cpuid_80000003.edx >> 16);
		cpuInfo.Name[31] = (uint8_t)(cpuid_80000003.edx >> 24);

		cpu_cpuid_result_t cpuid_80000004 = cpu_cpuid(0x80000004);
		cpuInfo.Name[32] = (uint8_t)cpuid_80000004.eax;
		cpuInfo.Name[33] = (uint8_t)(cpuid_80000004.eax >> 8);
		cpuInfo.Name[34] = (uint8_t)(cpuid_80000004.eax >> 16);
		cpuInfo.Name[35] = (uint8_t)(cpuid_80000004.eax >> 24);
		cpuInfo.Name[36] = (uint8_t)cpuid_80000004.ebx;
		cpuInfo.Name[37] = (uint8_t)(cpuid_80000004.ebx >> 8);
		cpuInfo.Name[38] = (uint8_t)(cpuid_80000004.ebx >> 16);
		cpuInfo.Name[39] = (uint8_t)(cpuid_80000004.ebx >> 24);
		cpuInfo.Name[40] = (uint8_t)cpuid_80000004.ecx;
		cpuInfo.Name[41] = (uint8_t)(cpuid_80000004.ecx >> 8);
		cpuInfo.Name[42] = (uint8_t)(cpuid_80000004.ecx >> 16);
		cpuInfo.Name[43] = (uint8_t)(cpuid_80000004.ecx >> 24);
		cpuInfo.Name[44] = (uint8_t)cpuid_80000004.edx;
		cpuInfo.Name[45] = (uint8_t)(cpuid_80000004.edx >> 8);
		cpuInfo.Name[46] = (uint8_t)(cpuid_80000004.edx >> 16);
		cpuInfo.Name[47] = (uint8_t)(cpuid_80000004.edx >> 24);
		cpuInfo.Name[48] = '\0';
		printf("%s\n", cpuInfo.Name);
	}

	//Wenn AVX verfügbar ist, dann aktivieren wir es jetzt
	if(cpuInfo.avx)
	{
		asm volatile(
				"mov %%cr4,%%rax;"
				"or $0x40000,%%rax;"
				"mov %%rax,%%cr4;"
				: : :"rax"
				);
		asm volatile(
				"xor %%ecx,%%ecx;"
				"xor %%edx,%%edx;"
				"mov $0x7,%%eax;"
				"xsetbv;"
				: : :"rax", "rcx", "rdx"
				);
	}

	//Setze NX-Bit (Bit 11 im EFER), wenn verfügbar
	if(cpuInfo.nx)
		cpu_MSRwrite(0xC0000080, cpu_MSRread(0xC0000080) | 0x800);

	//Wenn verfügbar Global Pages aktivieren
	if(cpuInfo.GlobalPage)
		asm volatile(
				"mov %%cr4,%%rax;"
				"or $1<<7,%%rax;"
				"mov %%rax,%%cr4;"
				: : :"rax");

	//Caching aktivieren
	asm volatile(
			"mov %%cr0,%%rax;"
			"btr $30,%%rax;"	//Cache disable bit deaktivieren
			"btr $29,%%rax;"	//Write through auch deaktivieren sonst gibt es eine #GP-Exception
			"mov %%rax,%%cr0;"
			: : :"rax");

	SysLog("CPU", "Initialisierung abgeschlossen");
}

cpu_cpuid_result_t cpu_cpuidSubleaf(uint32_t function, uint32_t subleaf)
{
	cpu_cpuid_result_t result;
	asm volatile("cpuid": "=a"(result.eax), "=b"(result.ebx), "=c"(result.ecx), "=d"(result.edx): "a"(function), "c"(subleaf));
	return result;
}

cpu_cpuid_result_t cpu_cpuid(uint32_t function)
{
	return cpu_cpuidSubleaf(function, 0);
}

/*
 * Liest den Wert aus dem angegeben MSR (Model spezific register) aus
 * Parameter:	msr = Nummer des MSR
 */
uint64_t cpu_MSRread(uint32_t msr)
{
	uint32_t low, high;
	if(!cpuInfo.msrAvailable) return 0;

	asm volatile("rdmsr" : "=a" (low), "=d" (high) : "c" (msr));
	return ((uint64_t)high << 32) | low;
}

/*
 * Schreibt den Wert in das angegebe MSR
 * Parameter:	msr = Nummer des MSR
 * 				Value = Wert, der in das MSR geschrieben werden soll
 */
void cpu_MSRwrite(uint32_t msr, uint64_t Value)
{
	uint32_t low = Value &0xFFFFFFFF;
	uint32_t high = Value >> 32;
	if(!cpuInfo.msrAvailable) return;

	asm volatile("wrmsr" : : "a" (low), "c" (msr), "d" (high));
}

#endif
