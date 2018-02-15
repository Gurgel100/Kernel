/*
 * apic.c
 *
 *  Created on: 18.02.2015
 *      Author: pascal
 */

#include "apic.h"
#include "cpu.h"
#include "memory.h"
#include "vmm.h"
#include "pmm.h"

#define APIC_BASE_MSR	0x1B

#define APIC_REG_SPIV 0xF0
#define APIC_REG_ICR_LOW 0x300
#define APIC_REG_ICR_HIGH 0x310
#define APIC_REG_DIV_CONFIG 0x3E0
#define APIC_REG_LVT_TIMER 0x320
#define APIC_REG_LVT_THERM 0x330
#define APIC_REG_LVT_PERFMON 0x340
#define APIC_REG_LVT_LINT0 0x350
#define APIC_REG_LVT_LINT1 0x360
#define APIC_REG_LVT_ERROR 0x370

static paddr_t apic_base_phys;
void *apic_base_virt;

extern void *getFreePages(void *start, void *end, size_t pages);

bool apic_available()
{
	return (cpu_cpuid(0x00000001).edx >> 9) & 1;
}

void apic_Init()
{
	//Basisaddresse auslesen
	apic_base_phys = cpu_MSRread(APIC_BASE_MSR);

	//Speicherbereich mappen
	apic_info.virtBase = vmm_Map(NULL, apic_info.physBase, 1, VMM_FLAGS_GLOBAL | VMM_FLAGS_NX | VMM_FLAGS_WRITE | VMM_FLAGS_NO_CACHE);

	//APIC aktivieren
	apic_Write(APIC_REG_SPIV, 1 << 8);
}

/*
 * Ein APIC-Register auslesen
 *
 * Parameter:	offset = Offset der Speicherstelle
 *
 * Rückgabe:	Wert der Speicherstelle
 */
uint32_t apic_Read(uintptr_t offset)
{
	uint32_t *ptr = apic_base_virt + offset;
	return *ptr;
}

/*
 * In ein APIC-Register schreiben
 *
 * Parameter:	offset = Offset der Speicherstelle
 * 				value = Wert der geschrieben werden soll
 */
void apic_Write(uintptr_t offset, uint32_t value)
{
	uint32_t *ptr = apic_base_virt + offset;
	*ptr = value;
}
