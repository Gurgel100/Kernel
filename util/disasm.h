/*
 * disasm.h
 *
 *  Created on: 17.08.2013
 *      Author: pascal
 */

#ifndef DISASM_H_
#define DISASM_H_

#include "config.h"

#ifdef DEBUGMODE

#include "stdint.h"
#include "stdbool.h"


#define M_8			0x1
#define M_16		0x2
#define M_32		0x4
#define M_64		0x8

#define M_R			0x10
#define M_M			0x20
#define M_RM		0x30
#define M_I			0x40
#define M_REL		0x50

#define F_VALID		0x1
#define F_MONIC		0x2
#define	F_STRUCT	0x4
#define F_FUNC		0x6
#define F_REGOPCODE	0x8
#define F_REGADD	0x16
#define F_MODREG	0x32

typedef struct{
	union{
		const char *mnemonic;	//Special string to convert
		/*
		 * Flags:	/s:			Source Operand 			equal /1
		 * 			/d:			Destination Operand		equal /0
		 * 			/Number:	Index of operand
		 * 			/r:			Range (b: byte; w: word; d: double word; q: quad word)
		 * 			/m:			Mnemonic when ModRM.REG is used to identify
		 */
		void *ext;
		uint8_t base_opcode;
	}data;
	uint8_t flags;	//Bits:		0: valid; 1-3: 001 = mnemonic, 010 = structure, 011 = function, 100 = Opcode - Base_Opcode = Register;
					//			4: Register is added to the Opcode; 5: ModRM.REG is used to identify
	bool ModRM;		//Is the ModR/M-Byte present?
	uint8_t op1;	//Bit 0-3:	1 = 8bit; 2 = 16bit; 4 = 32bit; 8 = 64bit
	uint8_t op2;	//Bit 4-7:	1 = r; 2 = m; 3 = r/m; 4 = imm; 5 = rel
	uint8_t op3;
}OpcodeDB_t;

typedef struct{
	char *r8;
	char *r16;
	char *r32;
	char *r64;
}Registers_t;

//Table of Registers
static const Registers_t ValueToReg[] =
{
	{"%al", "%ax", "%eax", "%rax"},	//0
	{"%cl", "%cx", "%ecx", "%rcx"},	//1
	{"%dl", "%dx", "%edx", "%rdx"},	//2
	{"%bl", "%bx", "%ebx", "%rbx"},	//3
	{"%ah", "%sp", "%esp", "%rsp"},	//4
	{"%ch", "%bp", "%ebp", "%rbp"},	//5
	{"%dh", "%si", "%esi", "%rsi"},	//6
	{"%bh", "%di", "%edi", "%rdi"}	//7
};

//One Byte Opcodes
static const OpcodeDB_t wOpcodeToMnemonic[];
static const OpcodeDB_t REG1OpcodeToMnemonic[];
static const OpcodeDB_t OpcodeToMnemonic[] =
{
/*0x00*/	{"%s add /s,/d", F_VALID & F_MONIC, 1, M_RM & M_8, M_R & M_8},
/*0x01*/	{"%s add /s,/d", F_VALID & F_MONIC, 1, M_RM & M_16 & M_32 & M_64, M_R & M_16 & M_32 & M_64},
/*0x02*/	{"%s add /s,/d", F_VALID & F_MONIC, 1, M_R & M_8, M_RM & M_8},
/*0x03*/	{"%s add /s,/d", F_VALID & F_MONIC, 1, M_R & M_16 & M_32 & M_64, M_RM & M_16 & M_32 & M_64},
/*0x04*/	{"%s add /s,%%al", F_VALID & F_MONIC, 1, M_8, M_I & M_8},
/*0x05*/	{"%s add /s,/d", F_VALID & F_MONIC, 1, M_16 & M_32, M_I & M_16 & M_32},
/*0x06*/	{},
/*0x07*/	{},
/*0x08*/	{"%s or /s,/d", F_VALID & F_MONIC, 1, M_RM & M_8, M_R & M_8},
/*0x09*/	{"%s or /s,/d", F_VALID & F_MONIC, 1, M_RM & M_16 & M_32 & M_64, M_R & M_16 & M_32 & M_64},
/*0x0a*/	{"%s or /s,/d", F_VALID & F_MONIC, 1, M_R & M_8, M_RM & M_8},
/*0x0b*/	{"%s or /s,/d", F_VALID & F_MONIC, 1, M_R & M_16 & M_32 & M_64, M_RM & M_16 & M_32 & M_64},
/*0x0c*/	{"%s or /s,%%al", F_VALID & F_MONIC, 1, M_8, M_I & M_8},
/*0x0d*/	{"%s or /s,/d", F_VALID & F_MONIC, 1, M_16 & M_32, M_I & M_16 & M_32},
/*0x0e*/	{},
/*0x0f*/	{wOpcodeToMnemonic, F_VALID & F_STRUCT},	//--> wOpcodeToMnemonic[]
/*0x10*/	{"%s adc /s,/d", F_VALID & F_MONIC, 1, M_RM & M_8, M_R & M_8},
/*0x11*/	{"%s adc /s,/d", F_VALID & F_MONIC, 1, M_RM & M_16 & M_32 & M_64, M_R & M_16 & M_32 & M_64},
/*0x12*/	{"%s adc /s,/d", F_VALID & F_MONIC, 1, M_R & M_8, M_RM & M_8},
/*0x13*/	{"%s adc /s,/d", F_VALID & F_MONIC, 1, M_R & M_16 & M_32 & M_64, M_RM & M_16 & M_32 & M_64},
/*0x14*/	{"%s adc /s,%%al", F_VALID & F_MONIC, 1, M_8, M_I & M_8},
/*0x15*/	{"%s adc /s,/d", F_VALID & F_MONIC, 1, M_16 & M_32, M_I & M_16 & M_32},
/*0x16*/	{},
/*0x17*/	{},
/*0x18*/	{"%s sbb /s,/d", F_VALID & F_MONIC, 1, M_RM & M_8, M_R & M_8},
/*0x19*/	{"%s sbb /s,/d", F_VALID & F_MONIC, 1, M_RM & M_16 & M_32 & M_64, M_R & M_16 & M_32 & M_64},
/*0x1a*/	{"%s sbb /s,/d", F_VALID & F_MONIC, 1, M_R & M_8, M_RM & M_8},
/*0x1b*/	{"%s sbb /s,/d", F_VALID & F_MONIC, 1, M_R & M_16 & M_32 & M_64, M_RM & M_16 & M_32 & M_64},
/*0x1c*/	{"%s sbb /s,%%al", F_VALID & F_MONIC, 1, M_8, M_I & M_8},
/*0x1d*/	{"%s sbb /s,/d", F_VALID & F_MONIC, 1, M_16 & M_32, M_I & M_16 & M_32},
/*0x1e*/	{},
/*0x1f*/	{},
/*0x20*/	{"%s and /s,/d", F_VALID & F_MONIC, 1, M_RM & M_8, M_R & M_8},
/*0x21*/	{"%s and /s,/d", F_VALID & F_MONIC, 1, M_RM & M_16 & M_32 & M_64, M_R & M_16 & M_32 & M_64},
/*0x22*/	{"%s and /s,/d", F_VALID & F_MONIC, 1, M_R & M_8, M_RM & M_8},
/*0x23*/	{"%s and /s,/d", F_VALID & F_MONIC, 1, M_R & M_16 & M_32 & M_64, M_RM & M_16 & M_32 & M_64},
/*0x24*/	{"%s and /s,%%al", F_VALID & F_MONIC, 1, M_8, M_I & M_8},
/*0x25*/	{"%s and /s,/d", F_VALID & F_MONIC, 1, M_16 & M_32, M_I & M_16 & M_32},
/*0x26*/	{},
/*0x27*/	{},
/*0x28*/	{"%s sub /s,/d", F_VALID & F_MONIC, 1, M_RM & M_8, M_R & M_8},
/*0x29*/	{"%s sub /s,/d", F_VALID & F_MONIC, 1, M_RM & M_16 & M_32 & M_64, M_R & M_16 & M_32 & M_64},
/*0x2a*/	{"%s sub /s,/d", F_VALID & F_MONIC, 1, M_R & M_8, M_RM & M_8},
/*0x2b*/	{"%s sub /s,/d", F_VALID & F_MONIC, 1, M_R & M_16 & M_32 & M_64, M_RM & M_16 & M_32 & M_64},
/*0x2c*/	{"%s sub /s,%%al", F_VALID & F_MONIC, 1, M_8, M_I & M_8},
/*0x2d*/	{"%s sub /s,/d", F_VALID & F_MONIC, 1, M_16 & M_32, M_I & M_16 & M_32},
/*0x2e*/	{},
/*0x2f*/	{},
/*0x30*/	{"%s xor /s,/d", F_VALID & F_MONIC, 1, M_RM & M_8, M_R & M_8},
/*0x31*/	{"%s xor /s,/d", F_VALID & F_MONIC, 1, M_RM & M_16 & M_32 & M_64, M_R & M_16 & M_32 & M_64},
/*0x32*/	{"%s xor /s,/d", F_VALID & F_MONIC, 1, M_R & M_8, M_RM & M_8},
/*0x33*/	{"%s xor /s,/d", F_VALID & F_MONIC, 1, M_R & M_16 & M_32 & M_64, M_RM & M_16 & M_32 & M_64},
/*0x34*/	{"%s xor /s,%%al", F_VALID & F_MONIC, 1, M_8, M_I & M_8},
/*0x35*/	{"%s xor /s,/d", F_VALID & F_MONIC, 1, M_16 & M_32, M_I & M_16 & M_32},
/*0x36*/	{},
/*0x37*/	{},
/*0x38*/	{"%s cmp /s,/d", F_VALID & F_MONIC, 1, M_RM & M_8, M_R & M_8},
/*0x39*/	{"%s cmp /s,/d", F_VALID & F_MONIC, 1, M_RM & M_16 & M_32 & M_64, M_R & M_16 & M_32 & M_64},
/*0x3a*/	{"%s cmp /s,/d", F_VALID & F_MONIC, 1, M_R & M_8, M_RM & M_8},
/*0x3b*/	{"%s cmp /s,/d", F_VALID & F_MONIC, 1, M_R & M_16 & M_32 & M_64, M_RM & M_16 & M_32 & M_64},
/*0x3c*/	{"%s cmp /s,%%al", F_VALID & F_MONIC, 1, M_8, M_I & M_8},
/*0x3d*/	{"%s cmp /s,/d", F_VALID & F_MONIC, 1, M_16 & M_32, M_I & M_16 & M_32},
/*0x3e*/	{},
/*0x3f*/	{},
/*0x40*/	{},	//REX
/*0x41*/	{},
/*0x42*/	{},
/*0x43*/	{},
/*0x44*/	{},
/*0x45*/	{},
/*0x46*/	{},
/*0x47*/	{},
/*0x48*/	{},
/*0x49*/	{},
/*0x4a*/	{},
/*0x4b*/	{},
/*0x4c*/	{},
/*0x4d*/	{},
/*0x4e*/	{},
/*0x4f*/	{},	//REX end
/*0x50*/	{"%s push /0", F_VALID & F_MONIC & F_REGADD, 1, M_R & M_16 & M_64},
/*0x51*/	{0x50, F_VALID & F_REGOPCODE & F_REGADD},
/*0x52*/	{0x50, F_VALID & F_REGOPCODE & F_REGADD},
/*0x53*/	{0x50, F_VALID & F_REGOPCODE & F_REGADD},
/*0x54*/	{0x50, F_VALID & F_REGOPCODE & F_REGADD},
/*0x55*/	{0x50, F_VALID & F_REGOPCODE & F_REGADD},
/*0x56*/	{0x50, F_VALID & F_REGOPCODE & F_REGADD},
/*0x57*/	{0x50, F_VALID & F_REGOPCODE & F_REGADD},
/*0x58*/	{"%s pop /0", F_VALID & F_MONIC & F_REGADD, 1, M_R & M_16 & M_64},
/*0x59*/	{0x58, F_VALID & F_REGOPCODE & F_REGADD},
/*0x5a*/	{0x58, F_VALID & F_REGOPCODE & F_REGADD},
/*0x5b*/	{0x58, F_VALID & F_REGOPCODE & F_REGADD},
/*0x5c*/	{0x58, F_VALID & F_REGOPCODE & F_REGADD},
/*0x5d*/	{0x58, F_VALID & F_REGOPCODE & F_REGADD},
/*0x5e*/	{0x58, F_VALID & F_REGOPCODE & F_REGADD},
/*0x5f*/	{0x58, F_VALID & F_REGOPCODE & F_REGADD},
/*0x60*/	{},
/*0x61*/	{},
/*0x62*/	{},
/*0x63*/	{"%s movsxd /s,/d", F_VALID & F_MONIC, 1, M_R & M_32 & M_64, M_RM & M_32},
/*0x64*/	{},
/*0x65*/	{},
/*0x66*/	{},
/*0x67*/	{},
/*0x68*/	{"%s push /0", F_VALID & F_MONIC, 1, M_I & M_16 & M_32},
/*0x69*/	{"%s imul /2,/s,/d", F_VALID & F_MONIC, 1, M_R & M_16 & M_32 & M_64, M_RM & M_16 & M_32 & M_64, M_I & M_16 & M_32},
/*0x6a*/	{"%s push /0", F_VALID & F_MONIC, 1, M_I & M_16 & M_32},
/*0x6b*/	{"%s imul /2,/s,/d", F_VALID & F_MONIC, 1, M_R & M_16 & M_32 & M_64, M_RM & M_16 & M_32 & M_64, M_I & M_8},
/*0x6c*/	{"%s insb (%%dx),/d", F_VALID & F_MONIC, 1, M_M & M_8},
/*0x6d*/	{"%s ins/r (%%dx),/d", F_VALID & F_MONIC, 1, M_M & M_16 & M_32},
/*0x6e*/	{"%s outsb /s,(%%dx)", F_VALID & F_MONIC, 1, M_M & M_8},
/*0x6f*/	{"%s outs/r /s,(%%dx)", F_VALID & F_MONIC, 1, M_M & M_16 & M_32},
/*0x70*/	{"%s jo", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x71*/	{"%s jno", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x72*/	{"%s jb", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x73*/	{"%s jnb", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x74*/	{"%s je", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x75*/	{"%s jne", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x76*/	{"%s jna", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x77*/	{"%s ja", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x78*/	{"%s js", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x79*/	{"%s jns", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x7a*/	{"%s jp", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x7b*/	{"%s jnp", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x7c*/	{"%s jl", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x7d*/	{"%s jnl", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x7e*/	{"%s jng", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x7f*/	{"%s jg", F_VALID & F_MONIC, 1, M_REL & M_8},
/*0x80*/	{REG1OpcodeToMnemonic, F_VALID & F_STRUCT & F_MODREG, 1, M_RM & M_8, M_I & M_8},
/*0x81*/	{REG1OpcodeToMnemonic, F_VALID & F_STRUCT & F_MODREG, 1, M_RM & M_16 & M_32 & M_64, M_I & M_16 & M_32},
/*0x82*/	{},
/*0x83*/	{REG1OpcodeToMnemonic, F_VALID & F_STRUCT & F_MODREG, 1, M_RM & M_16 & M_32 & M_64, M_I & M_8},
/*0x84*/	{"%s test /s,/d", F_VALID & F_MONIC, 1, M_RM & M_8, M_R & M_8},
/*0x85*/	{"%s test /s,/d", F_VALID & F_MONIC, 1, M_RM & M_16 & M_32 & M_64, M_R & M_16 & M_32 & M_64},
/*0x86*/	{"%s xchg /s,/d", F_VALID & F_MONIC, 1, M_RM & M_8, M_R & M_8},
/*0x87*/	{"%s xchg /s,/d", F_VALID & F_MONIC, 1, M_RM & M_16 & M_32 & M_64, M_R & M_16 & M_32 & M_64},
};

static const OpcodeDB_t wOpcodeToMnemonic[] =
{
};

static const OpcodeDB_t REG1OpcodeToMnemonic[] =
{
/*0*/	{"%s add /s,/d", F_VALID & F_MONIC & F_MODREG},
/*1*/	{"%s or /s,/d", F_VALID & F_MONIC & F_MODREG},
/*2*/	{"%s adc /s,/d", F_VALID & F_MONIC & F_MODREG},
/*3*/	{"%s sbb /s,/d", F_VALID & F_MONIC & F_MODREG},
/*4*/	{"%s and /s,/d", F_VALID & F_MONIC & F_MODREG},
/*5*/	{"%s sub /s,/d", F_VALID & F_MONIC & F_MODREG},
/*6*/	{"%s xor /s,/d", F_VALID & F_MONIC & F_MODREG},
/*7*/	{"%s cmp /s,/d", F_VALID & F_MONIC & F_MODREG}
};

void disasm(void *cmd);

#endif	//DEBUGMODE

#endif /* DISASM_H_ */
