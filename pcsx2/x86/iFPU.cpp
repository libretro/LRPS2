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

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"
#include "iFPU.h"

using namespace x86Emitter;

const __aligned16 u32 g_minvals[4]	= {0xff7fffff, 0xff7fffff, 0xff7fffff, 0xff7fffff};
const __aligned16 u32 g_maxvals[4]	= {0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff};

//------------------------------------------------------------------
namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
namespace COP1 {

namespace DOUBLE {

void recABS_S_xmm(int info);
void recADD_S_xmm(int info);
void recADDA_S_xmm(int info);
void recC_EQ_xmm(int info);
void recC_LE_xmm(int info);
void recC_LT_xmm(int info);
void recCVT_S_xmm(int info);
void recCVT_W();
void recDIV_S_xmm(int info);
void recMADD_S_xmm(int info);
void recMADDA_S_xmm(int info);
void recMAX_S_xmm(int info);
void recMIN_S_xmm(int info);
void recMOV_S_xmm(int info);
void recMSUB_S_xmm(int info);
void recMSUBA_S_xmm(int info);
void recMUL_S_xmm(int info);
void recMULA_S_xmm(int info);
void recNEG_S_xmm(int info);
void recSUB_S_xmm(int info);
void recSUBA_S_xmm(int info);
void recSQRT_S_xmm(int info);
void recRSQRT_S_xmm(int info);

};

//------------------------------------------------------------------
// Helper Macros
//------------------------------------------------------------------
#define _Ft_ _Rt_
#define _Fs_ _Rd_
#define _Fd_ _Sa_

// FCR31 Flags
#define FPUflagC	0X00800000
#define FPUflagI	0X00020000
#define FPUflagD	0X00010000
#define FPUflagO	0X00008000
#define FPUflagU	0X00004000
#define FPUflagSI	0X00000040
#define FPUflagSD	0X00000020
#define FPUflagSO	0X00000010
#define FPUflagSU	0X00000008

static const __aligned16 u32 s_neg[4] = { 0x80000000, 0xffffffff, 0xffffffff, 0xffffffff };
static const __aligned16 u32 s_pos[4] = { 0x7fffffff, 0xffffffff, 0xffffffff, 0xffffffff };

#define REC_FPUBRANCH(f) \
	void f(); \
	void rec##f() { \
	iFlushCall(FLUSH_INTERPRETER); \
	xFastCall((void*)(uptr)R5900::Interpreter::OpcodeImpl::COP1::f); \
	g_branch = 2; \
}

#define REC_FPUFUNC(f) \
	void f(); \
	void rec##f() { \
	iFlushCall(FLUSH_INTERPRETER); \
	xFastCall((void*)(uptr)R5900::Interpreter::OpcodeImpl::COP1::f); \
}
//------------------------------------------------------------------

//------------------------------------------------------------------
// *FPU Opcodes!*
//------------------------------------------------------------------

// Those opcode are marked as special ! But I don't understand why we can't run them in the interpreter
#ifndef FPU_RECOMPILE

REC_FPUFUNC(CFC1);
REC_FPUFUNC(CTC1);
REC_FPUFUNC(MFC1);
REC_FPUFUNC(MTC1);

#else

//------------------------------------------------------------------
// CFC1 / CTC1
//------------------------------------------------------------------
void recCFC1(void)
{
	if ( !_Rt_  ) return;

	_eeOnWriteReg(_Rt_, 1);

	if (_Fs_ >= 16)
		xMOV(eax, ptr[&fpuRegs.fprc[31] ]);
	else
		xMOV(eax, ptr[&fpuRegs.fprc[0] ]);
	_deleteEEreg(_Rt_, 0);

	if (_Fs_ >= 16)
	{
		xAND(eax, 0x0083c078); //remove always-zero bits
		xOR(eax, 0x01000001); //set always-one bits
	}

	xCDQ( );
	xMOV(ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ]], eax);
	xMOV(ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ]], edx);
}

void recCTC1()
{
	if ( _Fs_ != 31 ) return;

	if ( GPR_IS_CONST1(_Rt_) )
	{
		xMOV(ptr32[&fpuRegs.fprc[ _Fs_ ]], g_cpuConstRegs[_Rt_].UL[0]);
	}
	else
	{
		int mmreg = _checkXMMreg(XMMTYPE_GPRREG, _Rt_, MODE_READ);

		if( mmreg >= 0 )
		{
			xMOVSS(ptr[&fpuRegs.fprc[ _Fs_ ]], xRegisterSSE(mmreg));
		}

		else
		{
			_deleteGPRtoXMMreg(_Rt_, 1);

			xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
			xMOV(ptr[&fpuRegs.fprc[ _Fs_ ]], eax);
		}
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// MFC1
//------------------------------------------------------------------

void recMFC1()
{
	int regt, regs;
	if ( ! _Rt_ ) return;

	_eeOnWriteReg(_Rt_, 1);

	regs = _checkXMMreg(XMMTYPE_FPREG, _Fs_, MODE_READ);

	if( regs >= 0 )
	{
		_deleteGPRtoXMMreg(_Rt_, 2);
		_signExtendXMMtoM((uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], regs, 0);
	}
	else
	{
		regt = _checkXMMreg(XMMTYPE_GPRREG, _Rt_, MODE_READ);

		if( regt >= 0 )
		{
			if( xmmregs[regt].mode & MODE_WRITE )
			{
				xMOVH.PS(ptr[&cpuRegs.GPR.r[_Rt_].UL[2]], xRegisterSSE(regt));
			}
			xmmregs[regt].inuse = 0;
		}

		_deleteEEreg(_Rt_, 0);
		xMOV(eax, ptr[&fpuRegs.fpr[ _Fs_ ].UL ]);

		xCDQ( );
		xMOV(ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ]], eax);
		xMOV(ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ]], edx);
	}
}

//------------------------------------------------------------------


//------------------------------------------------------------------
// MTC1
//------------------------------------------------------------------
void recMTC1()
{
	if( GPR_IS_CONST1(_Rt_) )
	{
		_deleteFPtoXMMreg(_Fs_, 0);
		xMOV(ptr32[&fpuRegs.fpr[ _Fs_ ].UL], g_cpuConstRegs[_Rt_].UL[0]);
	}
	else
	{
		int mmreg = _checkXMMreg(XMMTYPE_GPRREG, _Rt_, MODE_READ);

		if( mmreg >= 0 )
		{
			if( g_pCurInstInfo->regs[_Rt_] & EEINST_LASTUSE )
			{
				// transfer the reg directly
				_deleteGPRtoXMMreg(_Rt_, 2);
				_deleteFPtoXMMreg(_Fs_, 2);
				_allocFPtoXMMreg(mmreg, _Fs_, MODE_WRITE);
			}
			else
			{
				int mmreg2 = _allocCheckFPUtoXMM(g_pCurInstInfo, _Fs_, MODE_WRITE);

				if( mmreg2 >= 0 )
					xMOVSS(xRegisterSSE(mmreg2), xRegisterSSE(mmreg));
				else
					xMOVSS(ptr[&fpuRegs.fpr[ _Fs_ ].UL], xRegisterSSE(mmreg));
			}
		}
		else
		{
			int mmreg2 = _allocCheckFPUtoXMM(g_pCurInstInfo, _Fs_, MODE_WRITE);

			if( mmreg2 >= 0 )
			{
				xMOVSSZX(xRegisterSSE(mmreg2), ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ]]);
			}
			else
			{
				xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ]]);
				xMOV(ptr[&fpuRegs.fpr[ _Fs_ ].UL], eax);
			}
		}
	}
}
#endif
//------------------------------------------------------------------


#ifndef FPU_RECOMPILE // If FPU_RECOMPILE is not defined, then use the interpreter opcodes. (CFC1, CTC1, MFC1, and MTC1 are special because they work specifically with the EE rec so they're defined above)

REC_FPUFUNC(ABS_S);
REC_FPUFUNC(ADD_S);
REC_FPUFUNC(ADDA_S);
REC_FPUBRANCH(BC1F);
REC_FPUBRANCH(BC1T);
REC_FPUBRANCH(BC1FL);
REC_FPUBRANCH(BC1TL);
REC_FPUFUNC(C_EQ);
REC_FPUFUNC(C_F);
REC_FPUFUNC(C_LE);
REC_FPUFUNC(C_LT);
REC_FPUFUNC(CVT_S);
REC_FPUFUNC(CVT_W);
REC_FPUFUNC(DIV_S);
REC_FPUFUNC(MAX_S);
REC_FPUFUNC(MIN_S);
REC_FPUFUNC(MADD_S);
REC_FPUFUNC(MADDA_S);
REC_FPUFUNC(MOV_S);
REC_FPUFUNC(MSUB_S);
REC_FPUFUNC(MSUBA_S);
REC_FPUFUNC(MUL_S);
REC_FPUFUNC(MULA_S);
REC_FPUFUNC(NEG_S);
REC_FPUFUNC(SUB_S);
REC_FPUFUNC(SUBA_S);
REC_FPUFUNC(SQRT_S);
REC_FPUFUNC(RSQRT_S);

#else // FPU_RECOMPILE

//------------------------------------------------------------------
// Clamp Functions (Converts NaN's and Infinities to Normal Numbers)
//------------------------------------------------------------------

static __aligned16 u64 FPU_FLOAT_TEMP[2];
static __fi void fpuFloat4(int regd) { // +NaN -> +fMax, -NaN -> -fMax, +Inf -> +fMax, -Inf -> -fMax
	int t1reg = _allocTempXMMreg(XMMT_FPS, -1);
	if (t1reg >= 0) {
		xMOVSS(xRegisterSSE(t1reg), xRegisterSSE(regd));
		xAND.PS(xRegisterSSE(t1reg), ptr[&s_neg[0]]);
		xMIN.SS(xRegisterSSE(regd), ptr[&g_maxvals[0]]);
		xMAX.SS(xRegisterSSE(regd), ptr[&g_minvals[0]]);
		xOR.PS(xRegisterSSE(regd), xRegisterSSE(t1reg));
		_freeXMMreg(t1reg);
	}
	else {
		t1reg = (regd == 0) ? 1 : 0; // get a temp reg thats not regd
		xMOVAPS(ptr[&FPU_FLOAT_TEMP[0]], xRegisterSSE(t1reg )); // backup data in t1reg to a temp address
		xMOVSS(xRegisterSSE(t1reg), xRegisterSSE(regd));
		xAND.PS(xRegisterSSE(t1reg), ptr[&s_neg[0]]);
		xMIN.SS(xRegisterSSE(regd), ptr[&g_maxvals[0]]);
		xMAX.SS(xRegisterSSE(regd), ptr[&g_minvals[0]]);
		xOR.PS(xRegisterSSE(regd), xRegisterSSE(t1reg));
		xMOVAPS(xRegisterSSE(t1reg), ptr[&FPU_FLOAT_TEMP[0] ]); // restore t1reg data
	}
}

static __fi void fpuFloat(int regd) {  // +/-NaN -> +fMax, +Inf -> +fMax, -Inf -> -fMax
	if (CHECK_FPU_OVERFLOW) {
		xMIN.SS(xRegisterSSE(regd), ptr[&g_maxvals[0]]); // MIN() must be before MAX()! So that NaN's become +Maximum
		xMAX.SS(xRegisterSSE(regd), ptr[&g_minvals[0]]);
	}
}

static __fi void fpuFloat2(int regd) { // +NaN -> +fMax, -NaN -> -fMax, +Inf -> +fMax, -Inf -> -fMax
	if (CHECK_FPU_OVERFLOW) {
		fpuFloat4(regd);
	}
}

static __fi void fpuFloat3(int regd)
{
	// This clamp function is used in the recC_xx opcodes
	// Rule of Rose needs clamping or else it crashes (minss or maxss both fix the crash)
	// Tekken 5 has disappearing characters unless preserving NaN sign (fpuFloat4() preserves NaN sign).
	// Digimon Rumble Arena 2 needs MAXSS clamping (if you only use minss, it spins on the intro-menus;
	// it also doesn't like preserving NaN sign with fpuFloat4, so the only way to make Digimon work
	// is by calling MAXSS first)
	if (CHECK_FPUCOMPAREHACK) {
		//xMIN.SS(xRegisterSSE(regd), ptr[&g_maxvals[0]]);
		xMAX.SS(xRegisterSSE(regd), ptr[&g_minvals[0]]);
	}
	else fpuFloat4(regd);
}

static void ClampValues(int regd)
{
	fpuFloat(regd);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ABS XMM
//------------------------------------------------------------------
void recABS_S_xmm(int info)
{
	if( info & PROCESS_EE_S ) xMOVSS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else xMOVSSZX(xRegisterSSE(EEREC_D), ptr[&fpuRegs.fpr[_Fs_]]);

	xAND.PS(xRegisterSSE(EEREC_D), ptr[&s_pos[0]]);
	//xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagO|FPUflagU)); // Clear O and U flags

	if (CHECK_FPU_OVERFLOW) // Only need to do positive clamp, since EEREC_D is positive
		xMIN.SS(xRegisterSSE(EEREC_D), ptr[&g_maxvals[0]]);
}

FPURECOMPILE_CONSTCODE(ABS_S, XMMINFO_WRITED|XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// FPU_ADD_SUB (Used to mimic PS2's FPU add/sub behavior)
//------------------------------------------------------------------
// Compliant IEEE FPU uses, in computations, uses additional "guard" bits to the right of the mantissa
// but EE-FPU doesn't. Substraction (and addition of positive and negative) may shift the mantissa left,
// causing those bits to appear in the result; this function masks out the bits of the mantissa that will
// get shifted right to the guard bits to ensure that the guard bits are empty.
// The difference of the exponents = the amount that the smaller operand will be shifted right by.
// Modification - the PS2 uses a single guard bit? (Coded by Nneeve)
//------------------------------------------------------------------
static void FPU_ADD_SUB(int regd, int regt, int issub)
{
	int tempecx = _allocX86reg(ecx, X86TYPE_TEMP, 0, 0); //receives regd
	int temp2   = _allocX86reg(xEmptyReg, X86TYPE_TEMP, 0, 0); //receives regt
	int xmmtemp = _allocTempXMMreg(XMMT_FPS, -1); //temporary for anding with regd/regt

	xMOVD(xRegister32(tempecx), xRegisterSSE(regd));
	xMOVD(xRegister32(temp2), xRegisterSSE(regt));

	//mask the exponents
	xSHR(xRegister32(tempecx), 23);
	xSHR(xRegister32(temp2), 23);
	xAND(xRegister32(tempecx), 0xff);
	xAND(xRegister32(temp2), 0xff);

	xSUB(xRegister32(tempecx), xRegister32(temp2)); //tempecx = exponent difference
	xCMP(xRegister32(tempecx), 25);
	j8Ptr[0] = JGE8(0);
	xCMP(xRegister32(tempecx), 0);
	j8Ptr[1] = JG8(0);
	j8Ptr[2] = JE8(0);
	xCMP(xRegister32(tempecx), -25);
	j8Ptr[3] = JLE8(0);

	//diff = -24 .. -1 , expd < expt
	xNEG(xRegister32(tempecx));
	xDEC(xRegister32(tempecx));
	xMOV(xRegister32(temp2), 0xffffffff);
	xSHL(xRegister32(temp2), cl); //temp2 = 0xffffffff << tempecx
	xMOVDZX(xRegisterSSE(xmmtemp), xRegister32(temp2));
	xAND.PS(xRegisterSSE(regd), xRegisterSSE(xmmtemp));
	if (issub)
		xSUB.SS(xRegisterSSE(regd), xRegisterSSE(regt));
	else
		xADD.SS(xRegisterSSE(regd), xRegisterSSE(regt));
	j8Ptr[4] = JMP8(0);

	x86SetJ8(j8Ptr[0]);
	//diff = 25 .. 255 , expt < expd
	xMOVAPS(xRegisterSSE(xmmtemp), xRegisterSSE(regt));
	xAND.PS(xRegisterSSE(xmmtemp), ptr[s_neg]);
	if (issub)
		xSUB.SS(xRegisterSSE(regd), xRegisterSSE(xmmtemp));
	else
		xADD.SS(xRegisterSSE(regd), xRegisterSSE(xmmtemp));
	j8Ptr[5] = JMP8(0);

	x86SetJ8(j8Ptr[1]);
	//diff = 1 .. 24, expt < expd
	xDEC(xRegister32(tempecx));
	xMOV(xRegister32(temp2), 0xffffffff);
	xSHL(xRegister32(temp2), cl); //temp2 = 0xffffffff << tempecx
	xMOVDZX(xRegisterSSE(xmmtemp), xRegister32(temp2));
	xAND.PS(xRegisterSSE(xmmtemp), xRegisterSSE(regt));
	if (issub)
		xSUB.SS(xRegisterSSE(regd), xRegisterSSE(xmmtemp));
	else
		xADD.SS(xRegisterSSE(regd), xRegisterSSE(xmmtemp));
	j8Ptr[6] = JMP8(0);

	x86SetJ8(j8Ptr[3]);
	//diff = -255 .. -25, expd < expt
	xAND.PS(xRegisterSSE(regd), ptr[s_neg]);
	if (issub)
		xSUB.SS(xRegisterSSE(regd), xRegisterSSE(regt));
	else
		xADD.SS(xRegisterSSE(regd), xRegisterSSE(regt));
	j8Ptr[7] = JMP8(0);

	x86SetJ8(j8Ptr[2]);
	//diff == 0
	if (issub)
		xSUB.SS(xRegisterSSE(regd), xRegisterSSE(regt));
	else
		xADD.SS(xRegisterSSE(regd), xRegisterSSE(regt));

	x86SetJ8(j8Ptr[4]);
	x86SetJ8(j8Ptr[5]);
	x86SetJ8(j8Ptr[6]);
	x86SetJ8(j8Ptr[7]);

	_freeXMMreg(xmmtemp);
	_freeX86reg(temp2);
	_freeX86reg(tempecx);
}

static void FPU_ADD(int regd, int regt)
{
       FPU_ADD_SUB(regd, regt, 0);
}

//------------------------------------------------------------------
// Note: PS2's multiplication uses some variant of booth multiplication with wallace trees:
// It cuts off some bits, resulting in inaccurate and non-commutative results.
// The PS2's result mantissa is either equal to x86's rounding to zero result mantissa
// or SMALLER (by 0x1). (this means that x86's other rounding modes are only less similar to PS2's mul)
//------------------------------------------------------------------

u32 FPU_MUL_HACK(u32 s, u32 t)
{
	if ((s == 0x3e800000) && (t == 0x40490fdb))
		return 0x3f490fda; // needed for Tales of Destiny Remake (only in a very specific room late-game)
	return 0;
}

void FPU_MUL(int regd, int regt, bool reverseOperands)
{
	u8 *noHack, *endMul = nullptr;

	if (CHECK_FPUMULHACK)
	{
		xMOVD(ecx, xRegisterSSE(reverseOperands ? regt : regd));
		xMOVD(edx, xRegisterSSE(reverseOperands ? regd : regt));
		xFastCall((void*)(uptr)&FPU_MUL_HACK, ecx, edx); //returns the hacked result or 0
		xTEST(eax, eax);
		noHack = JZ8(0);
			xMOVDZX(xRegisterSSE(regd), eax);
			endMul = JMP8(0);
		x86SetJ8(noHack);
	}

	xMUL.SS(xRegisterSSE(regd), xRegisterSSE(regt));

	if (CHECK_FPUMULHACK)
		x86SetJ8(endMul);
}

void FPU_MUL(int regd, int regt) { FPU_MUL(regd, regt, false); }
void FPU_MUL_REV(int regd, int regt) { FPU_MUL(regd, regt, true); } //reversed operands

//------------------------------------------------------------------
// CommutativeOp XMM (used for ADD, MUL, MAX, and MIN opcodes)
//------------------------------------------------------------------
static void (*recComOpXMM_to_XMM[] )(x86SSERegType, x86SSERegType) = {
	FPU_ADD, FPU_MUL, SSE_MAXSS_XMM_to_XMM, SSE_MINSS_XMM_to_XMM };

static void (*recComOpXMM_to_XMM_REV[] )(x86SSERegType, x86SSERegType) = { //reversed operands
	FPU_ADD, FPU_MUL_REV, SSE_MAXSS_XMM_to_XMM, SSE_MINSS_XMM_to_XMM };

int recCommutativeOp(int info, int regd, int op)
{
	int t0reg = _allocTempXMMreg(XMMT_FPS, -1);

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			if (regd == EEREC_S) {
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW  /*&& !CHECK_FPUCLAMPHACK */ || (op >= 2)) { fpuFloat2(regd); fpuFloat2(t0reg); }
				recComOpXMM_to_XMM[op](regd, t0reg);
			}
			else {
				xMOVSSZX(xRegisterSSE(regd), ptr[&fpuRegs.fpr[_Ft_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW || (op >= 2)) { fpuFloat2(regd); fpuFloat2(EEREC_S); }
				recComOpXMM_to_XMM_REV[op](regd, EEREC_S);
			}
			break;
		case PROCESS_EE_T:
			if (regd == EEREC_T) {
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Fs_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW || (op >= 2)) { fpuFloat2(regd); fpuFloat2(t0reg); }
				recComOpXMM_to_XMM_REV[op](regd, t0reg);
			}
			else {
				xMOVSSZX(xRegisterSSE(regd), ptr[&fpuRegs.fpr[_Fs_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW || (op >= 2)) { fpuFloat2(regd); fpuFloat2(EEREC_T); }
				recComOpXMM_to_XMM[op](regd, EEREC_T);
			}
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			if (regd == EEREC_T) {
				if (CHECK_FPU_EXTRA_OVERFLOW || (op >= 2)) { fpuFloat2(regd); fpuFloat2(EEREC_S); }
				recComOpXMM_to_XMM_REV[op](regd, EEREC_S);
			}
			else {
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(EEREC_S));
				if (CHECK_FPU_EXTRA_OVERFLOW || (op >= 2)) { fpuFloat2(regd); fpuFloat2(EEREC_T); }
				recComOpXMM_to_XMM[op](regd, EEREC_T);
			}
			break;
		default:
			xMOVSSZX(xRegisterSSE(regd), ptr[&fpuRegs.fpr[_Fs_]]);
			xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
			if (CHECK_FPU_EXTRA_OVERFLOW || (op >= 2)) { fpuFloat2(regd); fpuFloat2(t0reg); }
			recComOpXMM_to_XMM[op](regd, t0reg);
			break;
	}

	_freeXMMreg(t0reg);
	return regd;
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ADD XMM
//------------------------------------------------------------------
void recADD_S_xmm(int info)
{
    ClampValues(recCommutativeOp(info, EEREC_D, 0));
}

FPURECOMPILE_CONSTCODE(ADD_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

void recADDA_S_xmm(int info)
{
    ClampValues(recCommutativeOp(info, EEREC_ACC, 0));
}

FPURECOMPILE_CONSTCODE(ADDA_S, XMMINFO_WRITEACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------

//------------------------------------------------------------------
// BC1x XMM
//------------------------------------------------------------------

static void _setupBranchTest(void)
{
	_eeFlushAllUnused();

	// COP1 branch conditionals are based on the following equation:
	// (fpuRegs.fprc[31] & 0x00800000)
	// BC2F checks if the statement is false, BC2T checks if the statement is true.

	xMOV(eax, ptr[&fpuRegs.fprc[31]]);
	xTEST(eax, FPUflagC);
}

void recBC1F(void)
{
	_setupBranchTest();
	recDoBranchImm(JNZ32(0));
}

void recBC1T(void)
{
	_setupBranchTest();
	recDoBranchImm(JZ32(0));
}

void recBC1FL(void)
{
	_setupBranchTest();
	recDoBranchImm_Likely(JNZ32(0));
}

void recBC1TL(void)
{
	_setupBranchTest();
	recDoBranchImm_Likely(JZ32(0));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// C.x.S XMM
//------------------------------------------------------------------
void recC_EQ_xmm(int info)
{
	int tempReg;
	int t0reg;

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			fpuFloat3(EEREC_S);
			t0reg = _allocTempXMMreg(XMMT_FPS, -1);
			if (t0reg >= 0) {
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
				fpuFloat3(t0reg);
				xUCOMI.SS(xRegisterSSE(EEREC_S), xRegisterSSE(t0reg));
				_freeXMMreg(t0reg);
			}
			else xUCOMI.SS(xRegisterSSE(EEREC_S), ptr[&fpuRegs.fpr[_Ft_]]);
			break;
		case PROCESS_EE_T:
			fpuFloat3(EEREC_T);
			t0reg = _allocTempXMMreg(XMMT_FPS, -1);
			if (t0reg >= 0) {
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Fs_]]);
				fpuFloat3(t0reg);
				xUCOMI.SS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
				_freeXMMreg(t0reg);
			}
			else xUCOMI.SS(xRegisterSSE(EEREC_T), ptr[&fpuRegs.fpr[_Fs_]]);
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			fpuFloat3(EEREC_S);
			fpuFloat3(EEREC_T);
			xUCOMI.SS(xRegisterSSE(EEREC_S), xRegisterSSE(EEREC_T));
			break;
		default:
			tempReg = _allocX86reg(xEmptyReg, X86TYPE_TEMP, 0, 0);
			xMOV(xRegister32(tempReg), ptr[&fpuRegs.fpr[_Fs_]]);
			xCMP(xRegister32(tempReg), ptr[&fpuRegs.fpr[_Ft_]]);

			j8Ptr[0] = JZ8(0);
				xAND(ptr32[&fpuRegs.fprc[31]], ~FPUflagC );
				j8Ptr[1] = JMP8(0);
			x86SetJ8(j8Ptr[0]);
				xOR(ptr32[&fpuRegs.fprc[31]], FPUflagC);
			x86SetJ8(j8Ptr[1]);

			if (tempReg >= 0) _freeX86reg(tempReg);
			return;
	}

	j8Ptr[0] = JZ8(0);
		xAND(ptr32[&fpuRegs.fprc[31]], ~FPUflagC );
		j8Ptr[1] = JMP8(0);
	x86SetJ8(j8Ptr[0]);
		xOR(ptr32[&fpuRegs.fprc[31]], FPUflagC);
	x86SetJ8(j8Ptr[1]);
}

FPURECOMPILE_CONSTCODE(C_EQ, XMMINFO_READS|XMMINFO_READT);
//REC_FPUFUNC(C_EQ);

void recC_F(void)
{
	xAND(ptr32[&fpuRegs.fprc[31]], ~FPUflagC );
}
//REC_FPUFUNC(C_F);

void recC_LE_xmm(int info )
{
	int tempReg; //tempX86reg
	int t0reg; //tempXMMreg

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			fpuFloat3(EEREC_S);
			t0reg = _allocTempXMMreg(XMMT_FPS, -1);
			if (t0reg >= 0) {
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
				fpuFloat3(t0reg);
				xUCOMI.SS(xRegisterSSE(EEREC_S), xRegisterSSE(t0reg));
				_freeXMMreg(t0reg);
			}
			else xUCOMI.SS(xRegisterSSE(EEREC_S), ptr[&fpuRegs.fpr[_Ft_]]);
			break;
		case PROCESS_EE_T:
			fpuFloat3(EEREC_T);
			t0reg = _allocTempXMMreg(XMMT_FPS, -1);
			if (t0reg >= 0) {
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Fs_]]);
				fpuFloat3(t0reg);
				xUCOMI.SS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
				_freeXMMreg(t0reg);
			}
			else {
				xUCOMI.SS(xRegisterSSE(EEREC_T), ptr[&fpuRegs.fpr[_Fs_]]);

				j8Ptr[0] = JAE8(0);
					xAND(ptr32[&fpuRegs.fprc[31]], ~FPUflagC );
					j8Ptr[1] = JMP8(0);
				x86SetJ8(j8Ptr[0]);
					xOR(ptr32[&fpuRegs.fprc[31]], FPUflagC);
				x86SetJ8(j8Ptr[1]);
				return;
			}
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			fpuFloat3(EEREC_S);
			fpuFloat3(EEREC_T);
			xUCOMI.SS(xRegisterSSE(EEREC_S), xRegisterSSE(EEREC_T));
			break;
		default: // Untested and incorrect, but this case is never reached AFAIK (cottonvibes)
			tempReg = _allocX86reg(xEmptyReg, X86TYPE_TEMP, 0, 0);
			xMOV(xRegister32(tempReg), ptr[&fpuRegs.fpr[_Fs_]]);
			xCMP(xRegister32(tempReg), ptr[&fpuRegs.fpr[_Ft_]]);

			j8Ptr[0] = JLE8(0);
				xAND(ptr32[&fpuRegs.fprc[31]], ~FPUflagC );
				j8Ptr[1] = JMP8(0);
			x86SetJ8(j8Ptr[0]);
				xOR(ptr32[&fpuRegs.fprc[31]], FPUflagC);
			x86SetJ8(j8Ptr[1]);

			if (tempReg >= 0) _freeX86reg(tempReg);
			return;
	}

	j8Ptr[0] = JBE8(0);
		xAND(ptr32[&fpuRegs.fprc[31]], ~FPUflagC );
		j8Ptr[1] = JMP8(0);
	x86SetJ8(j8Ptr[0]);
		xOR(ptr32[&fpuRegs.fprc[31]], FPUflagC);
	x86SetJ8(j8Ptr[1]);
}

FPURECOMPILE_CONSTCODE(C_LE, XMMINFO_READS|XMMINFO_READT);
//REC_FPUFUNC(C_LE);

void recC_LT_xmm(int info)
{
	int tempReg;
	int t0reg;

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			fpuFloat3(EEREC_S);
			t0reg = _allocTempXMMreg(XMMT_FPS, -1);
			if (t0reg >= 0) {
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
				fpuFloat3(t0reg);
				xUCOMI.SS(xRegisterSSE(EEREC_S), xRegisterSSE(t0reg));
				_freeXMMreg(t0reg);
			}
			else xUCOMI.SS(xRegisterSSE(EEREC_S), ptr[&fpuRegs.fpr[_Ft_]]);
			break;
		case PROCESS_EE_T:
			fpuFloat3(EEREC_T);
			t0reg = _allocTempXMMreg(XMMT_FPS, -1);
			if (t0reg >= 0) {
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Fs_]]);
				fpuFloat3(t0reg);
				xUCOMI.SS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
				_freeXMMreg(t0reg);
			}
			else {
				xUCOMI.SS(xRegisterSSE(EEREC_T), ptr[&fpuRegs.fpr[_Fs_]]);

				j8Ptr[0] = JA8(0);
					xAND(ptr32[&fpuRegs.fprc[31]], ~FPUflagC );
					j8Ptr[1] = JMP8(0);
				x86SetJ8(j8Ptr[0]);
					xOR(ptr32[&fpuRegs.fprc[31]], FPUflagC);
				x86SetJ8(j8Ptr[1]);
				return;
			}
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			// Clamp NaNs
			// Note: This fixes a crash in Rule of Rose.
			fpuFloat3(EEREC_S);
			fpuFloat3(EEREC_T);
			xUCOMI.SS(xRegisterSSE(EEREC_S), xRegisterSSE(EEREC_T));
			break;
		default:
			tempReg = _allocX86reg(xEmptyReg, X86TYPE_TEMP, 0, 0);
			xMOV(xRegister32(tempReg), ptr[&fpuRegs.fpr[_Fs_]]);
			xCMP(xRegister32(tempReg), ptr[&fpuRegs.fpr[_Ft_]]);

			j8Ptr[0] = JL8(0);
				xAND(ptr32[&fpuRegs.fprc[31]], ~FPUflagC );
				j8Ptr[1] = JMP8(0);
			x86SetJ8(j8Ptr[0]);
				xOR(ptr32[&fpuRegs.fprc[31]], FPUflagC);
			x86SetJ8(j8Ptr[1]);

			if (tempReg >= 0) _freeX86reg(tempReg);
			return;
	}

	j8Ptr[0] = JB8(0);
		xAND(ptr32[&fpuRegs.fprc[31]], ~FPUflagC );
		j8Ptr[1] = JMP8(0);
	x86SetJ8(j8Ptr[0]);
		xOR(ptr32[&fpuRegs.fprc[31]], FPUflagC);
	x86SetJ8(j8Ptr[1]);
}

FPURECOMPILE_CONSTCODE(C_LT, XMMINFO_READS|XMMINFO_READT);
//REC_FPUFUNC(C_LT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// CVT.x XMM
//------------------------------------------------------------------
void recCVT_S_xmm(int info)
{
	if( !(info&PROCESS_EE_S) || (EEREC_D != EEREC_S && !(info&PROCESS_EE_MODEWRITES)) ) {
		xCVTSI2SS(xRegisterSSE(EEREC_D), ptr32[&fpuRegs.fpr[_Fs_]]);
	}
	else {
		xCVTDQ2PS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	}
}

FPURECOMPILE_CONSTCODE(CVT_S, XMMINFO_WRITED|XMMINFO_READS);

void recCVT_W(void)
{
	if (CHECK_FPU_FULL)
	{
		DOUBLE::recCVT_W();
		return;
	}
	// If we have the following EmitOP() on the top then it'll get calculated twice when CHECK_FPU_FULL is true
	// as we also have an EmitOP() at recCVT_W() on iFPUd.cpp.  hence we have it below the possible return.

	int regs = _checkXMMreg(XMMTYPE_FPREG, _Fs_, MODE_READ);

	if( regs >= 0 )
	{
		if (CHECK_FPU_EXTRA_OVERFLOW) fpuFloat2(regs);
		xCVTTSS2SI(eax, xRegisterSSE(regs));
		xMOVMSKPS(edx, xRegisterSSE(regs));	//extract the signs
		xAND(edx, 1);				//keep only LSB
	}
	else
	{
		xCVTTSS2SI(eax, ptr32[&fpuRegs.fpr[ _Fs_ ]]);
		xMOV(edx, ptr[&fpuRegs.fpr[ _Fs_ ]]);
		xSHR(edx, 31);	//mov sign to lsb
	}

	//kill register allocation for dst because we write directly to fpuRegs.fpr[_Fd_]
	_deleteFPtoXMMreg(_Fd_, 2);

	xADD(edx, 0x7FFFFFFF);	//0x7FFFFFFF if positive, 0x8000 0000 if negative

	xCMP(eax, 0x80000000);	//If the result is indefinitive
	xCMOVE(eax, edx);		//Saturate it

	//Write the result
	xMOV(ptr[&fpuRegs.fpr[_Fd_]], eax);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// DIV XMM
//------------------------------------------------------------------
static void recDIVhelper1(int regd, int regt) // Sets flags
{
	u8 *pjmp1, *pjmp2;
	u32 *ajmp32, *bjmp32;
	int t1reg = _allocTempXMMreg(XMMT_FPS, -1);
	int tempReg = _allocX86reg(xEmptyReg, X86TYPE_TEMP, 0, 0);

	xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagI|FPUflagD)); // Clear I and D flags

	/*--- Check for divide by zero ---*/
	xXOR.PS(xRegisterSSE(t1reg), xRegisterSSE(t1reg));
	xCMPEQ.SS(xRegisterSSE(t1reg), xRegisterSSE(regt));
	xMOVMSKPS(xRegister32(tempReg), xRegisterSSE(t1reg));
	xAND(xRegister32(tempReg), 1);  //Check sign (if regt == zero, sign will be set)
	ajmp32 = JZ32(0); //Skip if not set

		/*--- Check for 0/0 ---*/
		xXOR.PS(xRegisterSSE(t1reg), xRegisterSSE(t1reg));
		xCMPEQ.SS(xRegisterSSE(t1reg), xRegisterSSE(regd));
		xMOVMSKPS(xRegister32(tempReg), xRegisterSSE(t1reg));
		xAND(xRegister32(tempReg), 1);  //Check sign (if regd == zero, sign will be set)
		pjmp1 = JZ8(0); //Skip if not set
			xOR(ptr32[&fpuRegs.fprc[31]], FPUflagI|FPUflagSI); // Set I and SI flags ( 0/0 )
			pjmp2 = JMP8(0);
		x86SetJ8(pjmp1); //x/0 but not 0/0
			xOR(ptr32[&fpuRegs.fprc[31]], FPUflagD|FPUflagSD); // Set D and SD flags ( x/0 )
		x86SetJ8(pjmp2);

		/*--- Make regd +/- Maximum ---*/
		xXOR.PS(xRegisterSSE(regd), xRegisterSSE(regt)); // Make regd Positive or Negative
		xAND.PS(xRegisterSSE(regd), ptr[&s_neg[0]]); // Get the sign bit
		xOR.PS(xRegisterSSE(regd), ptr[&g_maxvals[0]]); // regd = +/- Maximum
		//xMOVSSZX(xRegisterSSE(regd), ptr[&g_maxvals[0]]);
		bjmp32 = JMP32(0);

	x86SetJ32(ajmp32);

	/*--- Normal Divide ---*/
	if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(regt); }
	xDIV.SS(xRegisterSSE(regd), xRegisterSSE(regt));

	ClampValues(regd);
	x86SetJ32(bjmp32);

	_freeXMMreg(t1reg);
	_freeX86reg(tempReg);
}

static __aligned16 SSE_MXCSR roundmode_nearest, roundmode_neg;

void recDIV_S_xmm(int info)
{
	bool roundmodeFlag = false;
	int t0reg = _allocTempXMMreg(XMMT_FPS, -1);

	if( CHECK_FPUNEGDIVHACK )
	{
		if (g_sseMXCSR.GetRoundMode() != SSEround_NegInf)
		{
			// Set roundmode to nearest since it isn't already

			roundmode_neg = g_sseMXCSR;
			roundmode_neg.SetRoundMode( SSEround_NegInf );
			xLDMXCSR( roundmode_neg );
			roundmodeFlag = true;
		}
	}
	else
	{
		if (g_sseMXCSR.GetRoundMode() != SSEround_Nearest)
		{
			// Set roundmode to nearest since it isn't already

			roundmode_nearest = g_sseMXCSR;
			roundmode_nearest.SetRoundMode( SSEround_Nearest );
			xLDMXCSR( roundmode_nearest );
			roundmodeFlag = true;
		}
	}

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			xMOVSS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
			recDIVhelper1(EEREC_D, t0reg);
			break;
		case PROCESS_EE_T:
			if (EEREC_D == EEREC_T) {
				xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
				xMOVSSZX(xRegisterSSE(EEREC_D), ptr[&fpuRegs.fpr[_Fs_]]);
				recDIVhelper1(EEREC_D, t0reg);
			}
			else {
				xMOVSSZX(xRegisterSSE(EEREC_D), ptr[&fpuRegs.fpr[_Fs_]]);
				recDIVhelper1(EEREC_D, EEREC_T);
			}
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			if (EEREC_D == EEREC_T) {
				xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
				xMOVSS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
				recDIVhelper1(EEREC_D, t0reg);
			}
			else {
				xMOVSS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
				recDIVhelper1(EEREC_D, EEREC_T);
			}
			break;
		default:
			xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
			xMOVSSZX(xRegisterSSE(EEREC_D), ptr[&fpuRegs.fpr[_Fs_]]);
			recDIVhelper1(EEREC_D, t0reg);
			break;
	}
	if (roundmodeFlag) xLDMXCSR (g_sseMXCSR);
	_freeXMMreg(t0reg);
}

FPURECOMPILE_CONSTCODE(DIV_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------



//------------------------------------------------------------------
// MADD XMM
//------------------------------------------------------------------
void recMADDtemp(int info, int regd)
{
	int t1reg;
	int t0reg = _allocTempXMMreg(XMMT_FPS, -1);

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			if(regd == EEREC_S) {
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(t0reg); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(t0reg));
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(regd); }
					FPU_ADD_SUB(regd, EEREC_ACC, 0);
				}
				else {
					xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(t0reg); }
					FPU_ADD_SUB(regd, t0reg, 0);
				}
			}
			else if (regd == EEREC_ACC){
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(EEREC_S); fpuFloat2(t0reg); }
				xMUL.SS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(regd, t0reg, 0);
			}
			else {
				xMOVSSZX(xRegisterSSE(regd), ptr[&fpuRegs.fpr[_Ft_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(EEREC_S); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(EEREC_S));
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(regd); }
					FPU_ADD_SUB(regd, EEREC_ACC, 0);
				}
				else {
					xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(t0reg); }
					FPU_ADD_SUB(regd, t0reg, 0);
				}
			}
			break;
		case PROCESS_EE_T:
			if(regd == EEREC_T) {
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Fs_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(t0reg); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(t0reg));
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(regd); }
					FPU_ADD_SUB(regd, EEREC_ACC, 0);
				}
				else {
					xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(t0reg); }
					FPU_ADD_SUB(regd, t0reg, 0);
				}
			}
			else if (regd == EEREC_ACC){
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Fs_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(EEREC_T); fpuFloat2(t0reg); }
				xMUL.SS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(regd, t0reg, 0);
			}
			else {
				xMOVSSZX(xRegisterSSE(regd), ptr[&fpuRegs.fpr[_Fs_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(EEREC_T); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(EEREC_T));
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(regd); }
					FPU_ADD_SUB(regd, EEREC_ACC, 0);
				}
				else {
					xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(t0reg); }
					FPU_ADD_SUB(regd, t0reg, 0);
				}
			}
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			if(regd == EEREC_S) {
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(EEREC_T); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(EEREC_T));
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_ACC); }
					FPU_ADD_SUB(regd, EEREC_ACC, 0);
				}
				else {
					xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
					FPU_ADD_SUB(regd, t0reg, 0);
				}
			}
			else if(regd == EEREC_T) {
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(EEREC_S); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(EEREC_S));
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_ACC); }
					FPU_ADD_SUB(regd, EEREC_ACC, 0);
				}
				else {
					xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
					FPU_ADD_SUB(regd, t0reg, 0);
				}
			}
			else if(regd == EEREC_ACC) {
				xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(t0reg); fpuFloat2(EEREC_T); }
				xMUL.SS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(regd, t0reg, 0);
			}
			else {
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(EEREC_S));
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(EEREC_T); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(EEREC_T));
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_ACC); }
					FPU_ADD_SUB(regd, EEREC_ACC, 0);
				}
				else {
					xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
					FPU_ADD_SUB(regd, t0reg, 0);
				}
			}
			break;
		default:
			if(regd == EEREC_ACC){
				t1reg = _allocTempXMMreg(XMMT_FPS, -1);
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Fs_]]);
				xMOVSSZX(xRegisterSSE(t1reg), ptr[&fpuRegs.fpr[_Ft_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(t0reg); fpuFloat2(t1reg); }
				xMUL.SS(xRegisterSSE(t0reg), xRegisterSSE(t1reg));
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(regd, t0reg, 0);
				_freeXMMreg(t1reg);
			}
			else
			{
				xMOVSSZX(xRegisterSSE(regd), ptr[&fpuRegs.fpr[_Fs_]]);
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(t0reg); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(t0reg));
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_ACC); }
					FPU_ADD_SUB(regd, EEREC_ACC, 0);
				}
				else {
					xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
					FPU_ADD_SUB(regd, t0reg, 0);
				}
			}
			break;
	}

     ClampValues(regd);
	 _freeXMMreg(t0reg);
}

void recMADD_S_xmm(int info)
{
	//xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagO|FPUflagU)); // Clear O and U flags
	recMADDtemp(info, EEREC_D);
}

FPURECOMPILE_CONSTCODE(MADD_S, XMMINFO_WRITED|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);

void recMADDA_S_xmm(int info)
{
	//xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagO|FPUflagU)); // Clear O and U flags
	recMADDtemp(info, EEREC_ACC);
}

FPURECOMPILE_CONSTCODE(MADDA_S, XMMINFO_WRITEACC|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MAX / MIN XMM
//------------------------------------------------------------------
void recMAX_S_xmm(int info)
{
	//xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagO|FPUflagU)); // Clear O and U flags
    recCommutativeOp(info, EEREC_D, 2);
}

FPURECOMPILE_CONSTCODE(MAX_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

void recMIN_S_xmm(int info)
{
	//xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagO|FPUflagU)); // Clear O and U flags
    recCommutativeOp(info, EEREC_D, 3);
}

FPURECOMPILE_CONSTCODE(MIN_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MOV XMM
//------------------------------------------------------------------
void recMOV_S_xmm(int info)
{
	if( info & PROCESS_EE_S ) xMOVSS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else xMOVSSZX(xRegisterSSE(EEREC_D), ptr[&fpuRegs.fpr[_Fs_]]);
}

FPURECOMPILE_CONSTCODE(MOV_S, XMMINFO_WRITED|XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MSUB XMM
//------------------------------------------------------------------
void recMSUBtemp(int info, int regd)
{
int t1reg;
	int t0reg = _allocTempXMMreg(XMMT_FPS, -1);

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			if(regd == EEREC_S) {
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(t0reg); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(t0reg));
				if (info & PROCESS_EE_ACC) { xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_ACC)); }
				else { xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(t0reg, regd, 1);
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(t0reg));
			}
			else if (regd == EEREC_ACC){
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(EEREC_S); fpuFloat2(t0reg); }
				xMUL.SS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(regd, t0reg, 1);
			}
			else {
				xMOVSSZX(xRegisterSSE(regd), ptr[&fpuRegs.fpr[_Ft_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(EEREC_S); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(EEREC_S));
				if (info & PROCESS_EE_ACC) { xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_ACC)); }
				else { xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(t0reg, regd, 1);
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(t0reg));
			}
			break;
		case PROCESS_EE_T:
			if(regd == EEREC_T) {
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Fs_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(t0reg); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(t0reg));
				if (info & PROCESS_EE_ACC) { xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_ACC)); }
				else { xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(t0reg, regd, 1);
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(t0reg));
			}
			else if (regd == EEREC_ACC){
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Fs_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(EEREC_T); fpuFloat2(t0reg); }
				xMUL.SS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(regd, t0reg, 1);
			}
			else {
				xMOVSSZX(xRegisterSSE(regd), ptr[&fpuRegs.fpr[_Fs_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(EEREC_T); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(EEREC_T));
				if (info & PROCESS_EE_ACC) { xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_ACC)); }
				else { xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(t0reg, regd, 1);
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(t0reg));
			}
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			if(regd == EEREC_S) {
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(EEREC_T); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(EEREC_T));
				if (info & PROCESS_EE_ACC) { xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_ACC)); }
				else { xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(t0reg, regd, 1);
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(t0reg));
			}
			else if(regd == EEREC_T) {
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(EEREC_S); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(EEREC_S));
				if (info & PROCESS_EE_ACC) { xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_ACC)); }
				else { xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(t0reg, regd, 1);
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(t0reg));
			}
			else if(regd == EEREC_ACC) {
				xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(t0reg); fpuFloat2(EEREC_T); }
				xMUL.SS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(regd, t0reg, 1);
			}
			else {
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(EEREC_S));
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(EEREC_T); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(EEREC_T));
				if (info & PROCESS_EE_ACC) { xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_ACC)); }
				else { xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(t0reg, regd, 1);
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(t0reg));
			}
			break;
		default:
			if(regd == EEREC_ACC){
				t1reg = _allocTempXMMreg(XMMT_FPS, -1);
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Fs_]]);
				xMOVSSZX(xRegisterSSE(t1reg), ptr[&fpuRegs.fpr[_Ft_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(t0reg); fpuFloat2(t1reg); }
				xMUL.SS(xRegisterSSE(t0reg), xRegisterSSE(t1reg));
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(regd, t0reg, 1);
				_freeXMMreg(t1reg);
			}
			else
			{
				xMOVSSZX(xRegisterSSE(regd), ptr[&fpuRegs.fpr[_Fs_]]);
				xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat2(regd); fpuFloat2(t0reg); }
				xMUL.SS(xRegisterSSE(regd), xRegisterSSE(t0reg));
				if (info & PROCESS_EE_ACC)  { xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_ACC)); }
				else { xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.ACC]); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				FPU_ADD_SUB(t0reg, regd, 1);
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(t0reg));
			}
			break;
	}

     ClampValues(regd);
	 _freeXMMreg(t0reg);

}

void recMSUB_S_xmm(int info)
{
	//xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagO|FPUflagU)); // Clear O and U flags
	recMSUBtemp(info, EEREC_D);
}

FPURECOMPILE_CONSTCODE(MSUB_S, XMMINFO_WRITED|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);

void recMSUBA_S_xmm(int info)
{
	//xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagO|FPUflagU)); // Clear O and U flags
	recMSUBtemp(info, EEREC_ACC);
}

FPURECOMPILE_CONSTCODE(MSUBA_S, XMMINFO_WRITEACC|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MUL XMM
//------------------------------------------------------------------
void recMUL_S_xmm(int info)
{
	//xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagO|FPUflagU)); // Clear O and U flags
    ClampValues(recCommutativeOp(info, EEREC_D, 1));
}

FPURECOMPILE_CONSTCODE(MUL_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

void recMULA_S_xmm(int info)
{
	//xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagO|FPUflagU)); // Clear O and U flags
	ClampValues(recCommutativeOp(info, EEREC_ACC, 1));
}

FPURECOMPILE_CONSTCODE(MULA_S, XMMINFO_WRITEACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// NEG XMM
//------------------------------------------------------------------
void recNEG_S_xmm(int info) {
	if( info & PROCESS_EE_S ) xMOVSS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else xMOVSSZX(xRegisterSSE(EEREC_D), ptr[&fpuRegs.fpr[_Fs_]]);

	//xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagO|FPUflagU)); // Clear O and U flags
	xXOR.PS(xRegisterSSE(EEREC_D), ptr[&s_neg[0]]);
	ClampValues(EEREC_D);
}

FPURECOMPILE_CONSTCODE(NEG_S, XMMINFO_WRITED|XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// SUB XMM
//------------------------------------------------------------------
void recSUBhelper(int regd, int regt)
{
	if (CHECK_FPU_EXTRA_OVERFLOW /*&& !CHECK_FPUCLAMPHACK*/) { fpuFloat2(regd); fpuFloat2(regt); }
	FPU_ADD_SUB(regd, regt, 1);
}

void recSUBop(int info, int regd)
{
	int t0reg = _allocTempXMMreg(XMMT_FPS, -1);

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			if (regd != EEREC_S) xMOVSS(xRegisterSSE(regd), xRegisterSSE(EEREC_S));
			xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
			recSUBhelper(regd, t0reg);
			break;
		case PROCESS_EE_T:
			if (regd == EEREC_T) {
				xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
				xMOVSSZX(xRegisterSSE(regd), ptr[&fpuRegs.fpr[_Fs_]]);
				recSUBhelper(regd, t0reg);
			}
			else {
				xMOVSSZX(xRegisterSSE(regd), ptr[&fpuRegs.fpr[_Fs_]]);
				recSUBhelper(regd, EEREC_T);
			}
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			if (regd == EEREC_T) {
				xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(EEREC_S));
				recSUBhelper(regd, t0reg);
			}
			else {
				xMOVSS(xRegisterSSE(regd), xRegisterSSE(EEREC_S));
				recSUBhelper(regd, EEREC_T);
			}
			break;
		default:
			xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
			xMOVSSZX(xRegisterSSE(regd), ptr[&fpuRegs.fpr[_Fs_]]);
			recSUBhelper(regd, t0reg);
			break;
	}

	ClampValues(regd);
	_freeXMMreg(t0reg);
}

void recSUB_S_xmm(int info)
{
	recSUBop(info, EEREC_D);
}

FPURECOMPILE_CONSTCODE(SUB_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);


void recSUBA_S_xmm(int info)
{
	recSUBop(info, EEREC_ACC);
}

FPURECOMPILE_CONSTCODE(SUBA_S, XMMINFO_WRITEACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// SQRT XMM
//------------------------------------------------------------------
void recSQRT_S_xmm(int info)
{
	u8* pjmp;
	bool roundmodeFlag = false;

	if (g_sseMXCSR.GetRoundMode() != SSEround_Nearest)
	{
		// Set roundmode to nearest if it isn't already
		roundmode_nearest = g_sseMXCSR;
		roundmode_nearest.SetRoundMode( SSEround_Nearest );
		xLDMXCSR (roundmode_nearest);
		roundmodeFlag = true;
	}

	if( info & PROCESS_EE_T ) xMOVSS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else xMOVSSZX(xRegisterSSE(EEREC_D), ptr[&fpuRegs.fpr[_Ft_]]);

	{
		int tempReg = _allocX86reg(xEmptyReg, X86TYPE_TEMP, 0, 0);

		xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagI|FPUflagD)); // Clear I and D flags

		/*--- Check for negative SQRT ---*/
		xMOVMSKPS(xRegister32(tempReg), xRegisterSSE(EEREC_D));
		xAND(xRegister32(tempReg), 1);  //Check sign
		pjmp = JZ8(0); //Skip if none are
			xOR(ptr32[&fpuRegs.fprc[31]], FPUflagI|FPUflagSI); // Set I and SI flags
			xAND.PS(xRegisterSSE(EEREC_D), ptr[&s_pos[0]]); // Make EEREC_D Positive
		x86SetJ8(pjmp);

		_freeX86reg(tempReg);
	}

	if (CHECK_FPU_OVERFLOW) xMIN.SS(xRegisterSSE(EEREC_D), ptr[&g_maxvals[0]]);// Only need to do positive clamp, since EEREC_D is positive
	xSQRT.SS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
	if (CHECK_FPU_EXTRA_OVERFLOW) ClampValues(EEREC_D); // Shouldn't need to clamp again since SQRT of a number will always be smaller than the original number, doing it just incase :/

	if (roundmodeFlag) xLDMXCSR (g_sseMXCSR);
}

FPURECOMPILE_CONSTCODE(SQRT_S, XMMINFO_WRITED|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// RSQRT XMM
//------------------------------------------------------------------
void recRSQRThelper1(int regd, int t0reg) // Preforms the RSQRT function when regd <- Fs and t0reg <- Ft (Sets correct flags)
{
	u8 *pjmp1, *pjmp2;
	u32 *pjmp32;
	u8 *qjmp1, *qjmp2;
	int t1reg = _allocTempXMMreg(XMMT_FPS, -1);
	int tempReg = _allocX86reg(xEmptyReg, X86TYPE_TEMP, 0, 0);

	xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagI|FPUflagD)); // Clear I and D flags

	/*--- (first) Check for negative SQRT ---*/
	xMOVMSKPS(xRegister32(tempReg), xRegisterSSE(t0reg));
	xAND(xRegister32(tempReg), 1);  //Check sign
	pjmp2 = JZ8(0); //Skip if not set
		xOR(ptr32[&fpuRegs.fprc[31]], FPUflagI|FPUflagSI); // Set I and SI flags
		xAND.PS(xRegisterSSE(t0reg), ptr[&s_pos[0]]); // Make t0reg Positive
	x86SetJ8(pjmp2);

	/*--- Check for zero ---*/
	xXOR.PS(xRegisterSSE(t1reg), xRegisterSSE(t1reg));
	xCMPEQ.SS(xRegisterSSE(t1reg), xRegisterSSE(t0reg));
	xMOVMSKPS(xRegister32(tempReg), xRegisterSSE(t1reg));
	xAND(xRegister32(tempReg), 1);  //Check sign (if t0reg == zero, sign will be set)
	pjmp1 = JZ8(0); //Skip if not set
		/*--- Check for 0/0 ---*/
		xXOR.PS(xRegisterSSE(t1reg), xRegisterSSE(t1reg));
		xCMPEQ.SS(xRegisterSSE(t1reg), xRegisterSSE(regd));
		xMOVMSKPS(xRegister32(tempReg), xRegisterSSE(t1reg));
		xAND(xRegister32(tempReg), 1);  //Check sign (if regd == zero, sign will be set)
		qjmp1 = JZ8(0); //Skip if not set
			xOR(ptr32[&fpuRegs.fprc[31]], FPUflagI|FPUflagSI); // Set I and SI flags ( 0/0 )
			qjmp2 = JMP8(0);
		x86SetJ8(qjmp1); //x/0 but not 0/0
			xOR(ptr32[&fpuRegs.fprc[31]], FPUflagD|FPUflagSD); // Set D and SD flags ( x/0 )
		x86SetJ8(qjmp2);

		/*--- Make regd +/- Maximum ---*/
		xAND.PS(xRegisterSSE(regd), ptr[&s_neg[0]]); // Get the sign bit
		xOR.PS(xRegisterSSE(regd), ptr[&g_maxvals[0]]); // regd = +/- Maximum
		pjmp32 = JMP32(0);
	x86SetJ8(pjmp1);

	if (CHECK_FPU_EXTRA_OVERFLOW) {
		xMIN.SS(xRegisterSSE(t0reg), ptr[&g_maxvals[0]]); // Only need to do positive clamp, since t0reg is positive
		fpuFloat2(regd);
	}

	xSQRT.SS(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
	xDIV.SS(xRegisterSSE(regd), xRegisterSSE(t0reg));

	ClampValues(regd);
	x86SetJ32(pjmp32);

	_freeXMMreg(t1reg);
	_freeX86reg(tempReg);
}

void recRSQRThelper2(int regd, int t0reg) // Preforms the RSQRT function when regd <- Fs and t0reg <- Ft (Doesn't set flags)
{
	xAND.PS(xRegisterSSE(t0reg), ptr[&s_pos[0]]); // Make t0reg Positive
	if (CHECK_FPU_EXTRA_OVERFLOW) {
		xMIN.SS(xRegisterSSE(t0reg), ptr[&g_maxvals[0]]); // Only need to do positive clamp, since t0reg is positive
		fpuFloat2(regd);
	}
	xSQRT.SS(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
	xDIV.SS(xRegisterSSE(regd), xRegisterSSE(t0reg));
	ClampValues(regd);
}

void recRSQRT_S_xmm(int info)
{
	// iFPUd (Full mode) sets roundmode to nearest for rSQRT.
	// Should this do the same, or should Full mode leave roundmode alone? --air

	int t0reg = _allocTempXMMreg(XMMT_FPS, -1);

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			xMOVSS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
			recRSQRThelper1(EEREC_D, t0reg);
			break;
		case PROCESS_EE_T:
			xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
			xMOVSSZX(xRegisterSSE(EEREC_D), ptr[&fpuRegs.fpr[_Fs_]]);
			recRSQRThelper1(EEREC_D, t0reg);
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			xMOVSS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
			xMOVSS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			recRSQRThelper1(EEREC_D, t0reg);
			break;
		default:
			xMOVSSZX(xRegisterSSE(t0reg), ptr[&fpuRegs.fpr[_Ft_]]);
			xMOVSSZX(xRegisterSSE(EEREC_D), ptr[&fpuRegs.fpr[_Fs_]]);
			recRSQRThelper1(EEREC_D, t0reg);
			break;
	}
	_freeXMMreg(t0reg);
}

FPURECOMPILE_CONSTCODE(RSQRT_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

#endif // FPU_RECOMPILE

} } } }
