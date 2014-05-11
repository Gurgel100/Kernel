/*
 * disasm.c
 *
 *  Created on: 26.04.2014
 *      Author: pascal
 */

#include "disasm.h"

#ifdef DEBUGMODE

#include "stdlib.h"
#include "stddef.h"

//Definitionen für Disassembler

#define PR_LOCK		0
#define PR_REPNE	1
#define PR_REP		2
#define PR_REPNZ	PR_REPNZ
#define PR_REPE		PR_REP
#define PR_REPZ		PR_REPE

typedef enum{
	LOCK, REP, REPNE,
	CS, SS , DS, ES, FS, GS,
	BNT, BT,
	OP_SIZE_OV,
	ADDR_SIZE_OV,
	__LAST_PREFIX
}prefix_t;

typedef struct{
	bool W, R, X, B;
	bool Valid;
}REX_t;

typedef struct{
	uint8_t R_M:3;
	uint8_t REG:3;
	uint8_t Mod:2;
}__attribute__((packed))ModR_M_t;

char *parse_mnemonic(OpcodeDB_t *Opcode, ModR_M_t *ModRM);

//Disassembler
void disasm(void *cmd)
{
	uint8_t *Byte;
	bool Prefix[__LAST_PREFIX];
	bool Prefixs = false;
	REX_t REX;
	REX.Valid = false;
	Byte = cmd;

	//Ausgabevariablen
	char *tmp;

	bool wgo = true;
	while(wgo)
	{
		switch(*Byte)
		{
			//Nach Prefixe suchen
			//Gruppe 1
			case 0xF0:
				Prefix[LOCK] = true;
				Prefixs = true;
			break;
			case 0xF2:
				Prefix[REPNE] = true;
				Prefixs = true;
			break;
			case 0xF3:
				Prefix[REP] = true;
				Prefixs = true;
			break;

			//Gruppe 2
			case 0x2E:
				Prefix[CS] = true;
				Prefixs = true;
			break;
			case 0x36:
				Prefix[SS] = true;
				Prefixs = true;
			break;
			case 0x3E:
				Prefix[DS] = true;
				Prefixs = true;
			break;
			case 0x26:
				Prefix[ES] = true;
				Prefixs = true;
			break;
			case 0x64:
				Prefix[FS] = true;
				Prefixs = true;
			break;
			case 0x65:
				Prefix[GS] = true;
				Prefixs = true;
			break;
			/*case 0x2E:
				Prefix[BNT] = true;
				Prefixs = true;
			break;
			case 0x3E:
				Prefix[BT] = true;
				Prefixs = true;
			break;*/

			//Gruppe 3
			case 0x66:
				Prefix[OP_SIZE_OV] = true;
				Prefixs = true;
			break;

			//Gruppe 4
			case 0x67:
				Prefix[ADDR_SIZE_OV] = true;
				Prefixs = true;
			break;
			default:
				wgo = false;
				Byte--;		//Fix, weil Byte++ immer ausgeführt wird
			break;
		}
		Byte++;
	}

	//REX erkennen (0x4X, für X = eine Zahl)
	if((*Byte >> 4) & 0xF == 4)
	{
		REX.W = (*Byte >> 3) & 1;
		REX.R = (*Byte >> 2) & 1;
		REX.X = (*Byte >> 1) & 1;
		REX.B = *Byte & 1;
		REX.Valid = true;
		Byte++;
	}

	char *Prfx;
	Prfx = "";
	if(Prefixs)
	{
		if(Prefix[LOCK])
			Prfx = "lock";
		else if(Prefix[REP])
			Prfx = "repe";
		else if(Prefix[REPNE])
			Prfx = "repne";
	}

	ModR_M_t *ModRM = 0;
	OpcodeDB_t Opcode = OpcodeToMnemonic[*Byte++];
	char *out;
	if(Opcode.flags & F_VALID == 0)
	{
		printf("(bad)");
		return;
	}
	if(Opcode.flags & 0b1110 == F_STRUCT)
	{
		if(Opcode.flags & (1 << 5))	//Mod.RM is used to identify
		{
			OpcodeDB_t *rOpcode = Opcode.data.ext;
			ModRM = Byte++;
			OpcodeDB_t sOpcode = rOpcode[ModRM->REG >> 3];
			if(sOpcode.flags & F_VALID == 0)
			{
				printf("(bad)");
				return;
			}
			if(sOpcode.flags & (1 << 5))
			{
				if(sOpcode.flags & 0b1110 == F_MONIC)
					out = parse_mnemonic(sOpcode, ModRM);
			}
		}
	}
	else if(Opcode.flags & 0b1110 == F_FUNC)
	{
	}
	else if(Opcode.flags & 0b1110 == F_REGOPCODE)
	{
	}
	else
		out = parse_mnemonic(Opcode, ModRM);

	printf(out, Prfx);

	free(out);
}

char *parse_mnemonic(OpcodeDB_t *Opcode, ModR_M_t *ModRM)
{
	char *monic = Opcode->data.mnemonic;
	char *out = 0;
	size_t i;
	for(i = 0; *monic != '\0'; i++, monic++)
	{
		if(*monic++ == '/')
		{
			switch(*monic)
			{
				case 's':	//Source Operand
				case '0':
					if((Opcode->op1 & 0xF0) >> 4 == 1)
					{
					}
				break;
				case 'd':	//Destination Operand
				case '1':
				break;
				default:
					out = realloc(out, i + 2);
					out[i++] = *(monic - 1);
					out[i] = *monic;
				break;
			}
		}
		else
		{
			out = realloc(out, i + 1);
			out[i] = *monic;
		}
	}

	return out;
}
#endif
