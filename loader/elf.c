/*
 * elf.c
 *
 *  Created on: 25.06.2012
 *      Author: pascal
 */
#include "elf.h"
#include "stdint.h"
#include "mm.h"
#include "memory.h"
#include "string.h"
#include "vmm.h"
#include "stdlib.h"
#include "assert.h"

typedef uint64_t	elf64_addr;
typedef uint16_t 	elf64_half;
typedef uint64_t	elf64_off;
typedef int32_t		elf64_sword;
typedef uint32_t	elf64_word;
typedef uint64_t	elf64_xword;
typedef int64_t		elf64_sxword;
typedef uint8_t		elf64_uchar;


#define ELF_MAG0	0x7F	//Datei-Identifikation
#define ELF_MAG1	'E'
#define ELF_MAG2	'L'
#define ELF_MAG3	'F'
#define	ELF_MAGIC	(ELF_MAG0 | ((ELF_MAG1 & 0xFFLL) << 8) | ((ELF_MAG2 & 0xFFLL) << 16) | ((ELF_MAG3 & 0xFFLL) << 24))

#define	ELF_CLASS_NONE	0x00	//Datei-Klasse
#define	ELF_CLASS_32	0x01	//32-Bit Datei
#define	ELF_CLASS_64	0x02	//64-Bit Datei

#define	ELF_DATA_NONE	0x00	//Prozessorspezifische Datenkodierung
#define	ELF_DATA_2LSB	0x01	//Little Endian
#define	ELF_DATA_2MSB	0x02	//Big Endian

typedef struct
{
		uint32_t	ei_magic;		// ELF-Magic Number
		uint8_t		ei_class;		// 32 oder 64 bit?
		uint8_t		ei_data;		// Little oder Big Endian?
		uint8_t		ei_version;		// dasselbe wie ELF_HEADER.e_version
		uint8_t		ei_osabi;		//OS/ABI Identifikation
		uint8_t		ei_abiversion;	//ABI Version
		uint8_t		ei_pad;			// reserved (zero)
		uint16_t	ei_pad2;		// reserved (zero)
		uint32_t	ei_pad3;		//reserviert (NULL)
		//uint8_t		ei_nident;		// ?
}elf_ident_header;


#define	ELF_ET_NONE		0x0000		// kein Typ
#define	ELF_ET_REL		0x0001		// relocatable
#define	ELF_ET_EXEC		0x0002		// ausführbar
#define	ELF_ET_DYN		0x0003		// Shared-Object-File
#define	ELF_ET_CORE		0x0004		// Corefile
#define	ELF_ET_LOPROC	0xFF00		// Processor-specific
#define	ELF_ET_HIPROC	0x00FF		// Processor-specific

#define	ELF_EM_NONE		0x0000		// kein Typ
#define	ELF_EM_M32		0x0001		// AT&T WE 32100
#define	ELF_EM_SPARC	0x0002		// SPARC
#define	ELF_EM_386		0x0003		// Intel 80386
#define	ELF_EM_68K		0x0004		// Motorola 68000
#define	ELF_EM_88K		0x0005		// Motorola 88000
#define	ELF_EM_860		0x0007		// Intel 80860
#define	ELF_EM_MIPS		0x0008		// MIPS RS3000
#define	ELF_EM_X86_64	0x003e		// AMD X86-64

#define	ELF_EV_NONE		0x00000000	// ungültige Version
#define	ELF_EV_CURRENT	0x00000001	// aktuelle Version

typedef struct
{
		elf_ident_header	e_ident;	// IDENT-HEADER (siehe oben)
		elf64_half			e_type; 	// Typ der ELF-Datei
		elf64_half			e_machine;	// Prozessortyp
		elf64_word			e_version;	// ELF-Version
		elf64_addr			e_entry;	// Virtuelle addresse des Einstiegspunkt des Codes
		elf64_off			e_phoff;	// Offset des Programm-Headers (ist 0 wenn nicht vorhanden)
		elf64_off			e_shoff;	// Offset des Section-Headers (ist 0 wenn nicht vorhanden)
		elf64_word			e_flags;	// processor-specific flags
		elf64_half			e_ehsize;	// Grösse des ELF-header
		elf64_half			e_phentsize;// Grösse eines Programm-Header Eintrags
		elf64_half			e_phnum;	// Anzahl Einträge im Programm-Header (ist 0 wenn nicht vorhanden)
		elf64_half			e_shentsize;// size of one section-header entry
		elf64_half			e_shnum;	// Anzahl Einträge im Section-Header (ist 0 wenn nicht vorhanden)
		elf64_half			e_shstrndex;// tells us which entry of the section-header is linked to the String-Table
}elf_header;


#define	ELF_PT_NULL		0x00000000	// ungültiges Segment
#define	ELF_PT_LOAD		0x00000001	// ladbares Segment
#define	ELF_PT_DYNAMIC	0x00000002	// dynamisch linkbares Segment
#define	ELF_PT_INTERP	0x00000003	// position of a zero-terminated string, which tells the interpreter
#define	ELF_PT_NOTE		0x00000004	// Universelles Segment
#define	ELF_PT_SHLIB	0x00000005	// shared lib (reserviert)
#define	ELF_PT_PHDIR	0x00000006	// gibt Position und Grösse des Programm-Headers an
#define	ELF_PT_LOPROC	0x70000000	// reserved
#define	ELF_PT_HIPROC	0x7FFFFFFF	// reserved

#define	ELF_PF_X		0x00000001	// ausführbares Segment
#define	ELF_PF_W		0x00000002	// schreibbares Segment
#define	ELF_PF_R		0x00000004	// lesbares Segment

typedef struct
{
		elf64_word	p_type;			// Typ des Segments (siehe oben)
		elf64_word	p_flags;		// Flags
		elf64_off	p_offset;		// Dateioffset des Segments
		elf64_addr	p_vaddr;		// Virtuelle Addresse, an die das Segment kopiert werden soll
		elf64_addr	p_paddr;		// Physikalische Addresse (meist irrelevant)
		elf64_xword	p_filesz;		// Grösse des Segments in der Datei
		elf64_xword	p_memsz;		// Grösse des Segments, die es im Speicher haben soll
		elf64_xword	p_align;		// Alignment. if zero or one, then no alignment is needed, otherwise the alignment has to be a power of two
}elf_program_header_entry;

#define ELF_SHT_NULL		0	//Nicht benutzt
#define ELF_SHT_PROGBITS	1	//Enthält Programmspezifische Informationen
#define ELF_SHT_SYMTAB		2	//Enthält Linkersymboltabelle
#define ELF_SHT_STRTAB		3	//Enthält Stringtabelle
#define ELF_SHT_RELA		4	//Enthält "Rela" Typ relocation entries
#define ELF_SHT_HASH		5	//Enthält symbol Hashtabelle
#define ELF_SHT_DYNAMIC		6	//Enthält dynamic linking tables
#define ELF_SHT_NOTE		7	//Enthält note Informationen
#define ELF_SHT_NOBITS		8	//Enthält uninitialisierten Platz. Belegt keinen Speicher in der Datei
#define ELF_SHT_REL			9	//Enthält "Rel" Typ relocation entries
#define ELF_SHT_SHLIB		10	//Reserviert
#define ELF_SHT_DYNSYM		11	//Enthält eine dynamic loader symbol table
#define ELF_SHT_LOOS		0x60000000	//Environment Spezifisch
#define ELF_SHT_HIOS		0x6FFFFFFF
#define ELF_SHT_LOPROC		0x70000000	//Prozessor spezifisch
#define ELF_SHT_HIPROC		0x7FFFFFFF

#define ELF_SHF_WRITE		0x1		//Sektion enthält schreibbare Daten
#define ELF_SHF_ALLOC		0x2		//Sektion ist im Speicherimage des Programms
#define ELF_SHF_EXECINSTR	0x4		//Sektion enthält ausführbare Daten
#define ELF_SHF_MASKOS		0x0F000000	//Envirement-spezifisch
#define ELF_SHF_MASKPROC	0xF0000000	//Prozessor-spezifisch

typedef struct{
	elf64_word	sh_name;		// Offset zum Sektionsnamen relativ zur Sektionsnamentabelle
	elf64_word	sh_type;		// Sektionstyp
	elf64_xword	sh_flags;		// Sektionsattribute
	elf64_addr	sh_addr;		// Virtuelle Addresse im Speicher
	elf64_off	sh_offset;		// Offset in der Datei
	elf64_xword	sh_size;		// Sektionsgrösse in der Datei
	elf64_word	sh_link;		// Link zu einer anderen Sektion
	elf64_word	sh_info;		// Verschiedene Informationen
	elf64_xword	sh_addralign;	// Address Alignment
	elf64_xword	sh_entsize;		// Grösse der Einträge, wenn Sektion eine Tabelle hat
}elf_section_header_entry;

extern context_t kernel_context;

static char elfCheck(elf_header *ELFHeader)
{
	//zuerst überprüfen wir auf den Magic-String
	if(ELFHeader->e_ident.ei_magic != ELF_MAGIC) return 2;
	//jetzt überprüfen wir auf den Dateityp
	if(ELFHeader->e_type != ELF_ET_EXEC) return 3;
	//jetzt überprüfen wir die Architektur (sollte x86 sein)
	if(ELFHeader->e_machine != ELF_EM_X86_64) return 4;
	//jetzt überprüfen wir die Klasse (wir suchen eine 64-Bit-Datei)
	if(ELFHeader->e_ident.ei_class != ELF_CLASS_64) return 5;
	//Daten
	if(ELFHeader->e_ident.ei_data != ELF_DATA_2LSB) return 6;
	//und jetzt noch die Version
	if(ELFHeader->e_version != ELF_EV_CURRENT) return 7;
	//alles OK
	return -1;
}

pid_t elfLoad(FILE *fp, const char *cmd, const char **env, const char *stdin, const char *stdout, const char *stderr)
{
	elf_header Header;

	//Zurücksetzen des Dateizeigers
	fseek(fp, 0, SEEK_SET);

	if(fread(&Header, 1, sizeof(elf_header), fp) < sizeof(elf_header))
		return -1;

	//Header überprüfen
	if(elfCheck(&Header) != -1)
		return -1;

	if(Header.e_entry < USERSPACE_START)
	{
		return -1;
	}

	elf_program_header_entry *ProgramHeader = malloc(Header.e_phnum * sizeof(elf_program_header_entry));
	if(ProgramHeader == NULL)
	{
		return -1;
	}
	fseek(fp, Header.e_phoff, SEEK_SET);
	if(fread(ProgramHeader, sizeof(elf_program_header_entry), Header.e_phnum, fp) < Header.e_phnum)
	{
		free(ProgramHeader);
		return -1;
	}

	//Jetzt einen neuen Prozess anlegen
	process_t *task = pm_InitTask(currentProcess, (void*)Header.e_entry, (char*)cmd, env, stdin, stdout, stderr);
	if(task == NULL)
	{
		free(ProgramHeader);
		return -1;
	}

	int i;
	for(i = 0; i < Header.e_phnum; i++)
	{
		//Wenn kein ladbares Segment, dann Springe zum nächsten Segment
		if(ProgramHeader[i].p_type != ELF_PT_LOAD) continue;

		size_t pages = ProgramHeader[i].p_memsz / 4096;
		pages += (ProgramHeader[i].p_memsz % 4096) ? 1 : 0;
		void *dest = mm_SysAlloc(pages);
		assert(dest != NULL);
		uintptr_t dest_off = ProgramHeader[i].p_vaddr % 0x1000;
		fseek(fp, ProgramHeader[i].p_offset, SEEK_SET);						//Position der Daten in der Datei
		fread(dest + dest_off, 1, ProgramHeader[i].p_filesz, fp);

		//Eventuell mit nullen auffüllen
		memset(dest + dest_off + ProgramHeader[i].p_filesz, 0, ProgramHeader[i].p_memsz - ProgramHeader[i].p_filesz);

		uint16_t flags = VMM_FLAGS_USER;
		if(ProgramHeader[i].p_flags & ELF_PF_W)
			flags |= VMM_FLAGS_WRITE;
		if(!(ProgramHeader[i].p_flags & ELF_PF_X))
			flags |= VMM_FLAGS_NX;

		//Speicherbereich an die richtige Addresse mappen
		vmm_ReMap(&kernel_context, dest, task->Context, (void*)ProgramHeader[i].p_vaddr, pages, flags, 0);
	}

	//Temporäre Daten wieder freigeben
	free(ProgramHeader);

	//Prozess aktivieren
	pm_ActivateTask(task);
	return task->PID;
}
