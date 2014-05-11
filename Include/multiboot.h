/*
 * multiboot.h
 *
 *  Created on: 02.07.2012
 *      Author: pascal
 */

#ifndef MULTIBOOT_H_
#define MULTIBOOT_H_

#include "stdint.h"

typedef struct
{
		uint32_t	mod_start;	//Startaddresse des Moduls
		uint32_t	mod_end;	//Endaddresse des Moduls
		uint32_t	name;		//Name des Moduls (0 terminierter ASCII-String)
		uint32_t	reserved;	//Reserviert (0)
}mods;

typedef struct{
		uint32_t size;		//Gibt die Grösse des Eintrag - 4 an
		uint64_t base_addr;	//Basisadresse des Speicherbereichs
		uint64_t length;	//Länge des Speicherbereichs
		uint32_t type;		//Typ des Speicherbereichs (1 = frei, ansonsten belegt)
}__attribute__((packed)) mmap;

typedef struct
{												//Gültig wenn mbs_flags[x] gesetzt
		uint32_t	mbs_flags;					//immer
		uint32_t	mbs_mem_lower;				//0
		uint32_t	mbs_mem_upper;				//0
		uint32_t	mbs_bootdevice;				//1
		uint32_t	mbs_cmdline;				//2
		uint32_t	mbs_mods_count;				//3
		uint32_t	mbs_mods_addr;				//3
		uint32_t	mbs_syms[4];				//4 oder 5
		uint32_t	mbs_mmap_length;			//6
		uint32_t	mbs_mmap_addr;				//6
		uint32_t	mbs_drives_length;			//7
		uint32_t	mbs_drives_addr;			//7
		uint32_t	mbs_config_table;			//8
		uint32_t	mbs_boot_loader_name;		//9
		uint32_t	mbs_apm_table;				//10
		uint32_t	mbs_vbe_control_info;		//11
		uint32_t	mbs_vbe_mode_info;			//11
		uint16_t	mbs_vbe_mode;				//11
		uint16_t	mbs_vbe_interface_seg;		//11
		uint16_t	mbs_vbe_interface_off;		//11
		uint16_t	mbs_vbe_interface_len;		//11
}multiboot_structure;

multiboot_structure *MBS;

#endif /* MULTIBOOT_H_ */
