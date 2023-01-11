/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _R5900_OPCODETABLES_H
#define _R5900_OPCODETABLES_H

#include "Pcsx2Defs.h"

enum Syscall : u8
{
	SetGsCrt = 2,
	SetVTLBRefillHandler = 13,
	GetOSParamConfig = 75,
	GetOSParamConfig2 = 111,
	sysPrintOut = 117,
	sceSifSetDma = 119,
	Deci2Call = 124
};

// reserve the lower 8 bits for opcode specific types
// which of these are actually used depends on the opcode
// flags further below
#define MEMTYPE_MASK         (0x07 << 0)
#define MEMTYPE_BYTE         (0x01 << 0)
#define MEMTYPE_HALF         (0x02 << 0)
#define MEMTYPE_WORD         (0x03 << 0)
#define MEMTYPE_DWORD        (0x04 << 0)
#define MEMTYPE_QWORD        (0x05 << 0)

#define CONDTYPE_MASK        (0x07 << 0)
#define CONDTYPE_EQ          (0x01 << 0)
#define CONDTYPE_NE          (0x02 << 0)
#define CONDTYPE_LEZ         (0x03 << 0)
#define CONDTYPE_GTZ         (0x04 << 0)
#define CONDTYPE_LTZ         (0x05 << 0)
#define CONDTYPE_GEZ         (0x06 << 0)

#define BRANCHTYPE_MASK      (0x07 << 3)
#define BRANCHTYPE_JUMP      (0x01 << 3)
#define BRANCHTYPE_BRANCH    (0x02 << 3)
#define BRANCHTYPE_SYSCALL   (0x03 << 3)
#define BRANCHTYPE_ERET      (0x04 << 3)
#define BRANCHTYPE_REGISTER  (0x05 << 3)
#define BRANCHTYPE_BC1       (0x06 << 3)

#define ALUTYPE_MASK         (0x07 << 3)
#define ALUTYPE_ADD          (0x01 << 3)
#define ALUTYPE_ADDI         (0x02 << 3)
#define ALUTYPE_SUB          (0x03 << 3)
#define ALUTYPE_CONDMOVE     (0x04 << 3)

#define IS_LOAD         0x00000100
#define IS_STORE        0x00000200
#define IS_BRANCH       0x00000400
#define IS_LINKED       0x00001000
#define IS_LIKELY       0x00002000
#define IS_MEMORY       0x00004000
#define IS_CONDMOVE     0x00010000
#define IS_ALU          0x00020000
#define IS_64BIT        0x00040000
#define IS_LEFT         0x00080000
#define IS_RIGHT        0x00100000

namespace R5900
{
	namespace Dynarec {
	namespace OpcodeImpl
	{
		void recNULL();
		void recUnknown();
		void recMMI_Unknown();
		void recCOP0_Unknown();
		void recCOP1_Unknown();

		void recCOP2();

		void recCACHE();
		void recPREF();
		void recSYSCALL();
		void recBREAK();
		void recSYNC();

		void recMFSA();
		void recMTSA();
		void recMTSAB();
		void recMTSAH();

		void recTGE();
		void recTGEU();
		void recTLT();
		void recTLTU();
		void recTEQ();
		void recTNE();
		void recTGEI();
		void recTGEIU();
		void recTLTI();
		void recTLTIU();
		void recTEQI();
		void recTNEI();

	} }

	///////////////////////////////////////////////////////////////////////////
	// Encapsulates information about every opcode on the Emotion Engine and
	// it's many co-processors.
	struct OPCODE
	{
		// Textual name of the instruction.
		const char Name[16];

		// Number of cycles this instruction normally uses.
		u8 cycles;

		// Information about the opcode
		u32 flags;

		const OPCODE& (*getsubclass)(u32 op);

		// Process the instruction using the interpreter.
		// The action is performed immediately on the EE's cpu state.
		void (*interpret)();

		// Generate recompiled code for this instruction, injected into
		// the current EErec block state.
		void (*recompile)();
	};

	// Returns the current real instruction, as per the current cpuRegs settings.
	const OPCODE& GetCurrentInstruction();
	const OPCODE& GetInstruction(u32 op);
	namespace OpcodeTables
	{
		using ::R5900::OPCODE;

		extern const OPCODE tbl_Standard[64];
	}

	namespace Opcodes
	{
		using ::R5900::OPCODE;

		const OPCODE& Class_SPECIAL(u32 op);
		const OPCODE& Class_REGIMM(u32 op);
		const OPCODE& Class_MMI(u32 op);
		const OPCODE& Class_MMI0(u32 op);
		const OPCODE& Class_MMI1(u32 op);
		const OPCODE& Class_MMI2(u32 op);
		const OPCODE& Class_MMI3(u32 op);

		const OPCODE& Class_COP0(u32 op);
		const OPCODE& Class_COP0_BC0(u32 op);
		const OPCODE& Class_COP0_C0(u32 op);

		const OPCODE& Class_COP1(u32 op);
		const OPCODE& Class_COP1_BC1(u32 op);
		const OPCODE& Class_COP1_S(u32 op);
		const OPCODE& Class_COP1_W(u32 op);
	}

	namespace Interpreter {
	namespace OpcodeImpl
	{
		using namespace ::R5900;

		void COP2();

		void Unknown();
		void MMI_Unknown();
		void COP0_Unknown();
		void COP1_Unknown();

// **********************Standard Opcodes**************************
		void J();
		void JAL();
		void BEQ();
		void BNE();
		void BLEZ();
		void BGTZ();
		void ADDI();
		void ADDIU();
		void SLTI();
		void SLTIU();
		void ANDI();
		void ORI();
		void XORI();
		void LUI();
		void BEQL();
		void BNEL();
		void BLEZL();
		void BGTZL();
		void DADDI();
		void DADDIU();
		void LDL();
		void LDR();
		void LB();
		void LH();
		void LWL();
		void LW();
		void LBU();
		void LHU();
		void LWR();
		void LWU();
		void SB();
		void SH();
		void SWL();
		void SW();
		void SDL();
		void SDR();
		void SWR();
		void CACHE();
		void LWC1();
		void PREF();
		void LQC2();
		void LD();
		void SQC2();
		void SD();
		void LQ();
		void SQ();
		void SWC1();
//*****************end of standard opcodes**********************
//********************SPECIAL OPCODES***************************
		void SLL();
		void SRL();
		void SRA();
		void SLLV();
		void SRLV();
		void SRAV();
		void JR();
		void JALR();
		void SYSCALL();
		void BREAK();
		void SYNC();
		void MFHI();
		void MTHI();
		void MFLO();
		void MTLO();
		void DSLLV();
		void DSRLV();
		void DSRAV();
		void MULT();
		void MULTU();
		void DIV();
		void DIVU();
		void ADD();
		void ADDU();
		void SUB();
		void SUBU();
		void AND();
		void OR();
		void XOR();
		void NOR();
		void SLT();
		void SLTU();
		void DADD();
		void DADDU();
		void DSUB();
		void DSUBU();
		void TGE();
		void TGEU();
		void TLT();
		void TLTU();
		void TEQ();
		void TNE();
		void DSLL();
		void DSRL();
		void DSRA();
		void DSLL32();
		void DSRL32();
		void DSRA32();
		void MOVZ();
		void MOVN();
		void MFSA();
		void MTSA();
//*******************END OF SPECIAL OPCODES************************
//***********************REGIMM OPCODES****************************
		void BLTZ();
		void BGEZ();
		void BLTZL();
		void BGEZL();
		void TGEI();
		void TGEIU();
		void TLTI();
		void TLTIU();
		void TEQI();
		void TNEI();
		void BLTZAL();
		void BGEZAL();
		void BLTZALL();
		void BGEZALL();
		void MTSAB();
		void MTSAH();
//*******************END OF REGIMM OPCODES***********************
//***********************MMI OPCODES*****************************
		void MADD();
		void MADDU();
		void MADD1();
		void MADDU1();
		void MFHI1();
		void MTHI1();
		void MFLO1();
		void MTLO1();
		void MULT1();
		void MULTU1();
		void DIV1();
		void DIVU1();

		namespace MMI {
		void PLZCW();
		void PMFHL();
		void PMTHL();
		void PSLLH();
		void PSRLH();
		void PSRAH();
		void PSLLW();
		void PSRLW();
		void PSRAW();
//********************END OF MMI OPCODES***********************
//***********************MMI0 OPCODES**************************
		void PADDW();
		void PSUBW();
		void PCGTW();
		void PMAXW();
		void PADDH();
		void PSUBH();
		void PCGTH();
		void PMAXH();
		void PADDB();
		void PSUBB();
		void PCGTB();
		void PADDSW();
		void PSUBSW();
		void PEXTLW();
		void PPACW();
		void PADDSH();
		void PSUBSH();
		void PEXTLH();
		void PPACH();
		void PADDSB();
		void PSUBSB();
		void PEXTLB();
		void PPACB();
		void PEXT5();
		void PPAC5();
//******************END OF MMI0 OPCODES***********************
//*********************MMI1 OPCODES***************************
		void PABSW();
		void PCEQW();
		void PMINW();
		void PADSBH();
		void PABSH();
		void PCEQH();
		void PMINH();
		void PCEQB();
		void PADDUW();
		void PSUBUW();
		void PEXTUW();
		void PADDUH();
		void PSUBUH();
		void PEXTUH();
		void PADDUB();
		void PSUBUB();
		void PEXTUB();
		void QFSRV();
//*****************END OF MMI1 OPCODES***********************
//*********************MMI2 OPCODES**************************
		void PMADDW();
		void PSLLVW();
		void PSRLVW();
		void PMSUBW();
		void PMFHI();
		void PMFLO();
		void PINTH();
		void PMULTW();
		void PDIVW();
		void PCPYLD();
		void PMADDH();
		void PHMADH();
		void PAND();
		void PXOR();
		void PMSUBH();
		void PHMSBH();
		void PEXEH();
		void PREVH();
		void PMULTH();
		void PDIVBW();
		void PEXEW();
		void PROT3W();
//********************END OF MMI2 OPCODES********************
//***********************MMI3 OPCODES************************
		void PMADDUW();
		void PSRAVW();
		void PMTHI();
		void PMTLO();
		void PINTEH();
		void PMULTUW();
		void PDIVUW();
		void PCPYUD();
		void POR();
		void PNOR();
		void PEXCH();
		void PCPYH();
		void PEXCW();
		}
//**********************END OF MMI3 OPCODES********************
//*************************COP0 OPCODES************************
		namespace COP0 {
		void MFC0();
		void MTC0();
		void BC0F();
		void BC0T();
		void BC0FL();
		void BC0TL();
		void TLBR();
		void TLBWI();
		void TLBWR();
		void TLBP();
		void ERET();
		void DI();
		void EI();
		}
//********************END OF COP0 OPCODES************************
//************COP1 OPCODES - Floating Point Unit*****************
		namespace COP1 {
		void MFC1();
		void CFC1();
		void MTC1();
		void CTC1();
		void BC1F();
		void BC1T();
		void BC1FL();
		void BC1TL();
		void ADD_S();
		void SUB_S();
		void MUL_S();
		void DIV_S();
		void SQRT_S();
		void ABS_S();
		void MOV_S();
		void NEG_S();
		void RSQRT_S();
		void ADDA_S();
		void SUBA_S();
		void MULA_S();
		void MADD_S();
		void MSUB_S();
		void MADDA_S();
		void MSUBA_S();
		void CVT_W();
		void MAX_S();
		void MIN_S();
		void C_F();
		void C_EQ();
		void C_LT();
		void C_LE();
		void CVT_S();
		}
	} }
}	// End namespace R5900

//****************************************************************************
//** COP2 - (VU0)                                                           **
//****************************************************************************
void QMFC2();
void CFC2();
void QMTC2();
void CTC2();
void BC2F();
void BC2T();
void BC2FL();
void BC2TL();
//*****************SPECIAL 1 VUO TABLE*******************************
void VADDx();
void VADDy();
void VADDz();
void VADDw();
void VSUBx();
void VSUBy();
void VSUBz();
void VSUBw();
void VMADDx();
void VMADDy();
void VMADDz();
void VMADDw();
void VMSUBx();
void VMSUBy();
void VMSUBz();
void VMSUBw();
void VMAXx();
void VMAXy();
void VMAXz();
void VMAXw();
void VMINIx();
void VMINIy();
void VMINIz();
void VMINIw();
void VMULx();
void VMULy();
void VMULz();
void VMULw();
void VMULq();
void VMAXi();
void VMULi();
void VMINIi();
void VADDq();
void VMADDq();
void VADDi();
void VMADDi();
void VSUBq();
void VMSUBq();
void VSUBi();
void VMSUBi();
void VADD();
void VMADD();
void VMUL();
void VMAX();
void VSUB();
void VMSUB();
void VOPMSUB();
void VMINI();
void VIADD();
void VISUB();
void VIADDI();
void VIAND();
void VIOR();
void VCALLMS();
void VCALLMSR();
//***********************************END OF SPECIAL1 VU0 TABLE*****************************
//******************************SPECIAL2 VUO TABLE*****************************************
void VADDAx();
void VADDAy();
void VADDAz();
void VADDAw();
void VSUBAx();
void VSUBAy();
void VSUBAz();
void VSUBAw();
void VMADDAx();
void VMADDAy();
void VMADDAz();
void VMADDAw();
void VMSUBAx();
void VMSUBAy();
void VMSUBAz();
void VMSUBAw();
void VITOF0();
void VITOF4();
void VITOF12();
void VITOF15();
void VFTOI0();
void VFTOI4();
void VFTOI12();
void VFTOI15();
void VMULAx();
void VMULAy();
void VMULAz();
void VMULAw();
void VMULAq();
void VABS();
void VMULAi();
void VCLIPw();
void VADDAq();
void VMADDAq();
void VADDAi();
void VMADDAi();
void VSUBAq();
void VMSUBAq();
void VSUBAi();
void VMSUBAi();
void VADDA();
void VMADDA();
void VMULA();
void VSUBA();
void VMSUBA();
void VOPMULA();
void VNOP();
void VMOVE();
void VMR32();
void VLQI();
void VSQI();
void VLQD();
void VSQD();
void VDIV();
void VSQRT();
void VRSQRT();
void VWAITQ();
void VMTIR();
void VMFIR();
void VILWR();
void VISWR();
void VRNEXT();
void VRGET();
void VRINIT();
void VRXOR();
//*******************END OF SPECIAL2 *********************

#endif
