/*
 * cpu.c
 *
 *  Created on: 13.07.2012
 *      Author: pascal
 */

#include "cpu.h"
#include "config.h"
#include "display.h"
#include "bit.h"
#include "assert.h"

#define NULL (void*)0

/*
 * Gets information of a cpu
 */
static void cpu_initInfo()
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
	cpuInfo.sse3 = BIT_EXTRACT(cpuid_1.ecx, 0);
	cpuInfo.ssse3 = BIT_EXTRACT(cpuid_1.ecx, 9);
	cpuInfo.sse4_1 = BIT_EXTRACT(cpuid_1.ecx, 19);
	cpuInfo.sse4_2 = BIT_EXTRACT(cpuid_1.ecx, 20);
	cpuInfo.avx = BIT_EXTRACT(cpuid_1.ecx, 28);
	cpuInfo.aes = BIT_EXTRACT(cpuid_1.ecx, 25);
	cpuInfo.rdrand = BIT_EXTRACT(cpuid_1.ecx, 30);
	cpuInfo.xsave = BIT_EXTRACT(cpuid_1.ecx, 26);

	//Featureflags Teil 2
	//RDMSR and WRMSR are supported because else we wouldn't be in long mode
	//SSE and SSE2 are supported in long mode
	//FXRSTOR and FXSAVE are supported in long mode
	assert(BIT_EXTRACT(cpuid_1.edx, 5) && "RDMSR/WRMSR not supported");
	assert(BIT_EXTRACT(cpuid_1.edx, 24) && "FXRSTOR/FXSAVE not supported");
	assert(BIT_EXTRACT(cpuid_1.edx, 25) && "SSE not supported");
	assert(BIT_EXTRACT(cpuid_1.edx, 26) && "SSE2 not supported");
	cpuInfo.GlobalPage = BIT_EXTRACT(cpuid_1.edx, 13);
	cpuInfo.HyperThreading = BIT_EXTRACT(cpuid_1.edx, 28);

	//Erweiterte Funktionen
	cpuInfo.maxextCPUID = cpu_cpuid(0x80000000).eax;

	cpu_cpuid_result_t cpuid_80000001 = cpu_cpuid(0x80000001);
	cpuInfo.nx = cpuid_80000001.edx & (1 << 20);
	cpuInfo.syscall = cpuid_80000001.edx & (1 << 11);
	cpuInfo.page_size_1gb = cpuid_80000001.edx & (1 << 26);

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
	}
}

void cpu_init(bool isBSP)
{
	if(isBSP)
		cpu_initInfo();

	uint64_t cr0 = cpu_readControlRegister(CPU_CR0);
	uint64_t cr4 = cpu_readControlRegister(CPU_CR4);

	//Activate caching
	cr0 = BIT_CLEAR(cr0, 29);	//clear CD (cache disable)
	cr0 = BIT_CLEAR(cr0, 30);	//clear NW (not write-through)

	cr0 = BIT_SET(cr0, 1);		//set CR0.MP
	cr0 = BIT_CLEAR(cr0, 2);	//delete CR0.EM

	cr4 = BIT_SET(cr4, 9);		//set CR4.OSFXSR
	//TODO: support OSXMMEXCPT
	//cr4 = BIT_SET(cr4, 10);	//set CR4.OSXMMEXCPT

	//Activate XGETBV, XRSTOR, XSAVE, XSETBV support
	if(cpuInfo.xsave)
		cr4 = BIT_SET(cr4, 18);

	//Activate global pages
	if(cpuInfo.GlobalPage)
		cr4 = BIT_SET(cr4, 7);

	cpu_writeControlRegister(CPU_CR0, cr0);
	cpu_writeControlRegister(CPU_CR4, cr4);

	if(cpuInfo.xsave)
	{
		//Activate all supported features
		uint64_t mask = 0b11 | (cpuInfo.avx << 2);
		cpu_writeControlRegister(CPU_XCR0, mask);
	}

	//Setze NX-Bit (Bit 11 im EFER), wenn verfügbar
	if(cpuInfo.nx)
		cpu_MSRwrite(0xC0000080, cpu_MSRread(0xC0000080) | 0x800);
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
	asm volatile("wrmsr" : : "a" (low), "c" (msr), "d" (high));
}
