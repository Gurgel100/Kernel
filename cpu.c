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
	uint32_t volatile Temp;
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

	cpuInfo.maxstdCPUID = cpu_CPUID(0x00000000, EAX);
	Temp = cpu_CPUID(0x00000000, EBX);
	cpuInfo.VendorID[0] = (uint8_t)Temp;
	cpuInfo.VendorID[1] = (uint8_t)(Temp >> 8);
	cpuInfo.VendorID[2] = (uint8_t)(Temp >> 16);
	cpuInfo.VendorID[3] = (uint8_t)(Temp >> 24);
	Temp = cpu_CPUID(0x00000000, EDX);
	cpuInfo.VendorID[4] = (uint8_t)Temp;
	cpuInfo.VendorID[5] = (uint8_t)(Temp >> 8);
	cpuInfo.VendorID[6] = (uint8_t)(Temp >> 16);
	cpuInfo.VendorID[7] = (uint8_t)(Temp >> 24);
	Temp = cpu_CPUID(0x00000000, ECX);
	cpuInfo.VendorID[8] = (uint8_t)Temp;
	cpuInfo.VendorID[9] = (uint8_t)(Temp >> 8);
	cpuInfo.VendorID[10] = (uint8_t)(Temp >> 16);
	cpuInfo.VendorID[11] = (uint8_t)(Temp >> 24);
	cpuInfo.VendorID[12] = '\0';
	//Herausfinden ob Intel oder AMD Prozessor
	if(cpuInfo.VendorID[11] == 'l')
		cpuInfo.Vendor = INTEL;
	else if(cpuInfo.VendorID[11] == 'D')
		cpuInfo.Vendor = AMD;
	else
		cpuInfo.Vendor = UNKN;

	//Prozessor
	Temp = cpu_CPUID(0x00000001, EAX);
	cpuInfo.Stepping = Temp & 0xF;
	cpuInfo.Model = Temp & 0xF0;
	cpuInfo.Family = Temp & 0xF00;
	if(cpuInfo.Vendor == INTEL)
		cpuInfo.Type = Temp & 0x3000;
	cpuInfo.extModel = Temp & 0xF0000;
	cpuInfo.extFamily = Temp & 0xFF00000;

	Temp = cpu_CPUID(0x00000001, EBX);
	cpuInfo.NumCores = Temp & 0xFF0000;	//Ungülitg, wenn HyperThreading nicht unterstützt

	Temp = cpu_CPUID(0x00000001, ECX);	//Featureflags Teil 1
	cpuInfo.sse3 = Temp & 0x1;
	cpuInfo.ssse3 = Temp & 0x200;
	cpuInfo.sse4_1 = Temp & 0x80000;
	if(cpuInfo.Vendor == INTEL)
	{
		cpuInfo.sse4_2 = Temp & 0x100000;
		cpuInfo.aes = Temp & 0x2000000;
		cpuInfo.avx = Temp & 0x10000000;
		cpuInfo.rdrand = Temp & 0x40000000;
	}

	Temp = cpu_CPUID(0x00000001, EDX);	//Featureflags Teil 2
	cpuInfo.msrAvailable = Temp & 0x20;
	cpuInfo.GlobalPage = Temp & 0x2000;
	cpuInfo.mmx = Temp & 0x800000;
	cpuInfo.sse = Temp & 0x2000000;
	cpuInfo.sse2 = Temp & 0x4000000;
	cpuInfo.HyperThreading = Temp & 0x10000000;

	//Erweiterte Funktionen
	cpuInfo.maxextCPUID = cpu_CPUID(0x80000000, EAX);

	Temp = cpu_CPUID(0x80000001, EDX);
	cpuInfo.nx = Temp & (1 << 20);

	//Namen des Prozessors
	if(cpuInfo.maxextCPUID >= 0x80000003 && cpuInfo.Vendor == INTEL)
	{
		Temp = cpu_CPUID(0x80000002, EAX);
		cpuInfo.Name[0] = (uint8_t)Temp;
		cpuInfo.Name[1] = (uint8_t)(Temp >> 8);
		cpuInfo.Name[2] = (uint8_t)(Temp >> 16);
		cpuInfo.Name[3] = (uint8_t)(Temp >> 24);
		Temp = cpu_CPUID(0x80000002, EBX);
		cpuInfo.Name[4] = (uint8_t)Temp;
		cpuInfo.Name[5] = (uint8_t)(Temp >> 8);
		cpuInfo.Name[6] = (uint8_t)(Temp >> 16);
		cpuInfo.Name[7] = (uint8_t)(Temp >> 24);
		Temp = cpu_CPUID(0x80000002, ECX);
		cpuInfo.Name[8] = (uint8_t)Temp;
		cpuInfo.Name[9] = (uint8_t)(Temp >> 8);
		cpuInfo.Name[10] = (uint8_t)(Temp >> 16);
		cpuInfo.Name[11] = (uint8_t)(Temp >> 24);
		Temp = cpu_CPUID(0x80000002, EDX);
		cpuInfo.Name[12] = (uint8_t)Temp;
		cpuInfo.Name[13] = (uint8_t)(Temp >> 8);
		cpuInfo.Name[14] = (uint8_t)(Temp >> 16);
		cpuInfo.Name[15] = (uint8_t)(Temp >> 24);
		Temp = cpu_CPUID(0x80000003, EAX);
		cpuInfo.Name[16] = (uint8_t)Temp;
		cpuInfo.Name[17] = (uint8_t)(Temp >> 8);
		cpuInfo.Name[18] = (uint8_t)(Temp >> 16);
		cpuInfo.Name[19] = (uint8_t)(Temp >> 24);
		Temp = cpu_CPUID(0x80000003, EBX);
		cpuInfo.Name[20] = (uint8_t)Temp;
		cpuInfo.Name[21] = (uint8_t)(Temp >> 8);
		cpuInfo.Name[22] = (uint8_t)(Temp >> 16);
		cpuInfo.Name[23] = (uint8_t)(Temp >> 24);
		Temp = cpu_CPUID(0x80000003, ECX);
		cpuInfo.Name[24] = (uint8_t)Temp;
		cpuInfo.Name[25] = (uint8_t)(Temp >> 8);
		cpuInfo.Name[26] = (uint8_t)(Temp >> 16);
		cpuInfo.Name[27] = (uint8_t)(Temp >> 24);
		Temp = cpu_CPUID(0x80000003, EDX);
		cpuInfo.Name[28] = (uint8_t)Temp;
		cpuInfo.Name[29] = (uint8_t)(Temp >> 8);
		cpuInfo.Name[30] = (uint8_t)(Temp >> 16);
		cpuInfo.Name[31] = (uint8_t)(Temp >> 24);
		Temp = cpu_CPUID(0x80000004, EAX);
		cpuInfo.Name[32] = (uint8_t)Temp;
		cpuInfo.Name[33] = (uint8_t)(Temp >> 8);
		cpuInfo.Name[34] = (uint8_t)(Temp >> 16);
		cpuInfo.Name[35] = (uint8_t)(Temp >> 24);
		Temp = cpu_CPUID(0x80000004, EBX);
		cpuInfo.Name[36] = (uint8_t)Temp;
		cpuInfo.Name[37] = (uint8_t)(Temp >> 8);
		cpuInfo.Name[38] = (uint8_t)(Temp >> 16);
		cpuInfo.Name[39] = (uint8_t)(Temp >> 24);
		Temp = cpu_CPUID(0x80000004, ECX);
		cpuInfo.Name[40] = (uint8_t)Temp;
		cpuInfo.Name[41] = (uint8_t)(Temp >> 8);
		cpuInfo.Name[42] = (uint8_t)(Temp >> 16);
		cpuInfo.Name[43] = (uint8_t)(Temp >> 24);
		Temp = cpu_CPUID(0x80000004, EDX);
		cpuInfo.Name[44] = (uint8_t)Temp;
		cpuInfo.Name[45] = (uint8_t)(Temp >> 8);
		cpuInfo.Name[46] = (uint8_t)(Temp >> 16);
		cpuInfo.Name[47] = (uint8_t)(Temp >> 24);
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
			"btc $30,%%rax;"	//Cache disable bit deaktivieren
			"btc $29,%%rax;"	//Write through auch deaktivieren sonst gibt es eine #GP-Exception
			"mov %%rax,%%cr0;"
			: : :"rax");

	SysLog("CPU", "Initialisierung abgeschlossen");
}

/*
 * Führt CPUID mit der angegebenen Funktion aus.
 * Rückgabewert: Das angebene Register
 */
uint32_t cpu_CPUID(uint32_t Funktion, CPU_REGISTER Register)
{
	register uint32_t Temp;
	if(!cpuInfo.cpuidAvailable) return 0;

	switch(Register)
	{
		case CR_EAX:
			asm volatile(
					"cpuid;"
					:"=a"(Temp) :"a"(Funktion)
			);
			return Temp;
		case CR_EBX:
			asm volatile(
					"cpuid;"
					:"=b"(Temp) :"a"(Funktion)
			);
			return Temp;
		case CR_ECX:
			asm volatile(
					"cpuid;"
					:"=c"(Temp) :"a"(Funktion)
			);
			return Temp;
		case CR_EDX:
			asm volatile(
					"cpuid;"
					:"=d"(Temp) :"a"(Funktion)
			);
			return Temp;
		default:
			return 0;
	}
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
