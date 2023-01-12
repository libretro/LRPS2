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
#include "x86emitter/x86emitter.h"
#include "iR5900.h"
#include "iFPU.h"

/* This is a version of the FPU that emulates an exponent of 0xff and overflow/underflow flags */

/* Can be made faster by not converting stuff back and forth between instructions. */


//----------------------------------------------------------------
// FPU emulation status:
// ADD, SUB (incl. accumulation stage of MADD/MSUB) - no known problems.
// Mul (incl. multiplication stage of MADD/MSUB) - incorrect. PS2's result mantissa is sometimes
//													smaller by 0x1 than IEEE's result (with round to zero).
// DIV, SQRT, RSQRT - incorrect. PS2's result varies between IEEE's result with round to zero
//													and IEEE's result with round to +/-infinity.
// other stuff - no known problems.
//----------------------------------------------------------------


using namespace x86Emitter;

#ifdef FPU_RECOMPILE

//------------------------------------------------------------------
namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
namespace COP1 {

u32 FPU_MUL_HACK(u32 s, u32 t);

namespace DOUBLE {

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

//------------------------------------------------------------------

//------------------------------------------------------------------
// *FPU Opcodes!*
//------------------------------------------------------------------

//------------------------------------------------------------------
// PS2 -> DOUBLE
//------------------------------------------------------------------

#define SINGLE(sign, exp, mant) (((u32)(sign)<<31) | ((u32)(exp)<<23) | (u32)(mant))
#define DOUBLE(sign, exp, mant) (((sign ## ULL)<<63) | ((exp ## ULL)<<52) | (mant ## ULL))

struct FPUd_Globals
{
	u32		neg[4], pos[4];

	u32		pos_inf[4], neg_inf[4],
			one_exp[4];

	u64		dbl_one_exp[2];

	u64		dbl_cvt_overflow,	// needs special code if above or equal
			dbl_ps2_overflow,	// overflow & clamp if above or equal
			dbl_underflow;		// underflow if below

	u64		padding;

	u64		dbl_s_pos[2];
	//u64		dlb_s_neg[2];
};

static const __aligned(32) FPUd_Globals s_const =
{
	{ 0x80000000, 0xffffffff, 0xffffffff, 0xffffffff },
	{ 0x7fffffff, 0xffffffff, 0xffffffff, 0xffffffff },

	{SINGLE(0,0xff,0), 0, 0, 0},
	{SINGLE(1,0xff,0), 0, 0, 0},
	{SINGLE(0,1,0), 0, 0, 0},

	{DOUBLE(0,1,0), 0},

	DOUBLE(0,1151,0),	// cvt_overflow
	DOUBLE(0,1152,0),	// ps2_overflow
	DOUBLE(0,897,0),	// underflow

	0,					// Padding!!

	{0x7fffffffffffffffULL, 0},
	//{0x8000000000000000ULL, 0},
};


// ToDouble : converts single-precision PS2 float to double-precision IEEE float

static void ToDouble(int reg)
{
	xUCOMI.SS(xRegisterSSE(reg), ptr[s_const.pos_inf]); // Sets ZF if reg is equal or incomparable to pos_inf
	u8 *to_complex = JE8(0); // Complex conversion if positive infinity or NaN
	xUCOMI.SS(xRegisterSSE(reg), ptr[s_const.neg_inf]);
	u8 *to_complex2 = JE8(0); // Complex conversion if negative infinity

	xCVTSS2SD(xRegisterSSE(reg), xRegisterSSE(reg)); // Simply convert
	u8 *end = JMP8(0);

	x86SetJ8(to_complex);
	x86SetJ8(to_complex2);

	// Special conversion for when IEEE sees the value in reg as an INF/NaN
	xPSUB.D(xRegisterSSE(reg), ptr[s_const.one_exp]); // Lower exponent by one
	xCVTSS2SD(xRegisterSSE(reg), xRegisterSSE(reg));
	xPADD.Q(xRegisterSSE(reg), ptr[s_const.dbl_one_exp]); // Raise exponent by one

	x86SetJ8(end);
}

//------------------------------------------------------------------
// DOUBLE -> PS2
//------------------------------------------------------------------

/*
	if FPU_RESULT is defined, results are more like the real PS2's FPU. But new issues may happen if
		the VU isn't clamping all operands since games may transfer FPU results into the VU.
		Ar tonelico 1 does this with the result from DIV/RSQRT (when a division by zero occurs)
	otherwise, results are still usually better than iFPU.cpp.
*/

// ToPS2FPU_Full - converts double-precision IEEE float to single-precision PS2 float

// converts small normal numbers to PS2 equivalent
// converts large normal numbers to PS2 equivalent (which represent NaN/inf in IEEE)
// converts really large normal numbers to PS2 signed max
// converts really small normal numbers to zero (flush)
// doesn't handle inf/nan/denormal

static void ToPS2FPU_Full(int reg, bool flags, int absreg, bool acc, bool addsub)
{
	if (flags)
		xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagO | FPUflagU));
	if (flags && acc)
		xAND(ptr32[&fpuRegs.ACCflag], ~1);

	xMOVAPS(xRegisterSSE(absreg), xRegisterSSE(reg));
	xAND.PD(xRegisterSSE(absreg), ptr[&s_const.dbl_s_pos]);

	xUCOMI.SD(xRegisterSSE(absreg), ptr[&s_const.dbl_cvt_overflow]);
	u8 *to_complex = JAE8(0);

	xUCOMI.SD(xRegisterSSE(absreg), ptr[&s_const.dbl_underflow]);
	u8 *to_underflow = JB8(0);

	xCVTSD2SS(xRegisterSSE(reg), xRegisterSSE(reg)); //simply convert
#ifdef __M_X86_64
	u32 *end = JMP32(0);
#else
	u8 *end = JMP8(0);
#endif

	x86SetJ8(to_complex);
	xUCOMI.SD(xRegisterSSE(absreg), ptr[&s_const.dbl_ps2_overflow]);
	u8 *to_overflow = JAE8(0);

	xPSUB.Q(xRegisterSSE(reg), ptr[&s_const.dbl_one_exp]); //lower exponent
	xCVTSD2SS(xRegisterSSE(reg), xRegisterSSE(reg)); //convert
	xPADD.D(xRegisterSSE(reg), ptr[s_const.one_exp]); //raise exponent
#ifdef __M_X86_64
	u32 *end2 = JMP32(0);
#else
	u8 *end2 = JMP8(0);
#endif

	x86SetJ8(to_overflow);
	xCVTSD2SS(xRegisterSSE(reg), xRegisterSSE(reg));
	xOR.PS(xRegisterSSE(reg), ptr[&s_const.pos]); //clamp
	if (flags)
	{
		xOR(ptr32[&fpuRegs.fprc[31]], (FPUflagO | FPUflagSO));
		if (acc)
			xOR(ptr32[&fpuRegs.ACCflag], 1);
	}
	u8 *end3 = JMP8(0);

	x86SetJ8(to_underflow);
	u8 *end4 = nullptr;
	if (flags) //set underflow flags if not zero
	{
		xXOR.PD(xRegisterSSE(absreg), xRegisterSSE(absreg));
		xUCOMI.SD(xRegisterSSE(reg), xRegisterSSE(absreg));
		u8 *is_zero = JE8(0);

		xOR(ptr32[&fpuRegs.fprc[31]], (FPUflagU | FPUflagSU));
		if (addsub)
		{
			//On ADD/SUB, the PS2 simply leaves the mantissa bits as they are (after normalization)
			//IEEE either clears them (FtZ) or returns the denormalized result.
			//not thoroughly tested : other operations such as MUL and DIV seem to clear all mantissa bits?
			xMOVAPS(xRegisterSSE(absreg), xRegisterSSE(reg));
			xPSLL.Q(xRegisterSSE(reg), 12); //mantissa bits
			xPSRL.Q(xRegisterSSE(reg), 41);
			xPSRL.Q(xRegisterSSE(absreg), 63); //sign bit
			xPSLL.Q(xRegisterSSE(absreg), 31);
			xPOR(xRegisterSSE(reg), xRegisterSSE(absreg));
			end4 = JMP8(0);
		}

		x86SetJ8(is_zero);
	}
	xCVTSD2SS(xRegisterSSE(reg), xRegisterSSE(reg));
	xAND.PS(xRegisterSSE(reg), ptr[s_const.neg]); //flush to zero

#ifdef __M_X86_64
	x86SetJ32(end);
	x86SetJ32(end2);
#else
	x86SetJ8(end);
	x86SetJ8(end2);
#endif
	x86SetJ8(end3);
	if (flags && addsub)
		x86SetJ8(end4);
}

//sets the maximum (positive or negative) value into regd.
//set regd to maximum
#define SetMaxValue(regd) xOR.PS(xRegisterSSE((regd)), ptr[&s_const.pos[0]]) 

#define GET_S(sreg) { \
	if( info & PROCESS_EE_S ) xMOVSS(xRegisterSSE(sreg), xRegisterSSE(EEREC_S)); \
	else xMOVSSZX(xRegisterSSE(sreg), ptr[&fpuRegs.fpr[_Fs_]]); }

#define ALLOC_S(sreg) { (sreg) = _allocTempXMMreg(XMMT_FPS, -1);  GET_S(sreg); }

#define GET_T(treg) { \
	if( info & PROCESS_EE_T ) xMOVSS(xRegisterSSE(treg), xRegisterSSE(EEREC_T)); \
	else xMOVSSZX(xRegisterSSE(treg), ptr[&fpuRegs.fpr[_Ft_]]); }

#define ALLOC_T(treg) { (treg) = _allocTempXMMreg(XMMT_FPS, -1);  GET_T(treg); }

#define GET_ACC(areg) { \
	if( info & PROCESS_EE_ACC ) xMOVSS(xRegisterSSE(areg), xRegisterSSE(EEREC_ACC)); \
	else xMOVSSZX(xRegisterSSE(areg), ptr[&fpuRegs.ACC]); }

#define ALLOC_ACC(areg) { (areg) = _allocTempXMMreg(XMMT_FPS, -1);  GET_ACC(areg); }

#define CLEAR_OU_FLAGS { xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagO | FPUflagU)); }


//------------------------------------------------------------------
// ABS XMM
//------------------------------------------------------------------
void recABS_S_xmm(int info)
{
	GET_S(EEREC_D);

	CLEAR_OU_FLAGS;

	xAND.PS(xRegisterSSE(EEREC_D), ptr[s_const.pos]);
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
static void FPU_ADD_SUB(int tempd, int tempt) //tempd and tempt are overwritten, they are floats
{
	int tempecx = _allocX86reg(ecx, X86TYPE_TEMP, 0, 0); //receives regd
	int temp2 = _allocX86reg(xEmptyReg, X86TYPE_TEMP, 0, 0); //receives regt
	int xmmtemp = _allocTempXMMreg(XMMT_FPS, -1); //temporary for anding with regd/regt

	xMOVD(xRegister32(tempecx), xRegisterSSE(tempd));
	xMOVD(xRegister32(temp2), xRegisterSSE(tempt));

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
	xAND.PS(xRegisterSSE(tempd), xRegisterSSE(xmmtemp));
	j8Ptr[4] = JMP8(0);

	x86SetJ8(j8Ptr[0]);
	//diff = 25 .. 255 , expt < expd
	xAND.PS(xRegisterSSE(tempt), ptr[s_const.neg]);
	j8Ptr[5] = JMP8(0);

	x86SetJ8(j8Ptr[1]);
	//diff = 1 .. 24, expt < expd
	xDEC(xRegister32(tempecx));
	xMOV(xRegister32(temp2), 0xffffffff);
	xSHL(xRegister32(temp2), cl); //temp2 = 0xffffffff << tempecx
	xMOVDZX(xRegisterSSE(xmmtemp), xRegister32(temp2));
	xAND.PS(xRegisterSSE(tempt), xRegisterSSE(xmmtemp));
	j8Ptr[6] = JMP8(0);

	x86SetJ8(j8Ptr[3]);
	//diff = -255 .. -25, expd < expt
	xAND.PS(xRegisterSSE(tempd), ptr[s_const.neg]);

	x86SetJ8(j8Ptr[2]);
	//diff == 0

	x86SetJ8(j8Ptr[4]);
	x86SetJ8(j8Ptr[5]);
	x86SetJ8(j8Ptr[6]);

	_freeXMMreg(xmmtemp);
	_freeX86reg(temp2);
	_freeX86reg(tempecx);
}

void FPU_MUL(int info, int regd, int sreg, int treg, bool acc)
{
	u8 *noHack;
	u32 *endMul = nullptr;

	if (CHECK_FPUMULHACK)
	{
		xMOVD(arg1regd, xRegisterSSE(sreg));
		xMOVD(arg2regd, xRegisterSSE(treg));
		xFastCall((void*)(uptr)&FPU_MUL_HACK, arg1regd, arg2regd); //returns the hacked result or 0
		xTEST(eax, eax);
		noHack = JZ8(0);
			xMOVDZX(xRegisterSSE(regd), eax);
			endMul = JMP32(0);
		x86SetJ8(noHack);
	}

	ToDouble(sreg); ToDouble(treg);
	xMUL.SD(xRegisterSSE(sreg), xRegisterSSE(treg));
	ToPS2FPU_Full(sreg, true, treg, acc, false);
	xMOVSS(xRegisterSSE(regd), xRegisterSSE(sreg));

	if (CHECK_FPUMULHACK)
		x86SetJ32(endMul);
}

//------------------------------------------------------------------
// CommutativeOp XMM (used for ADD and SUB opcodes. that's it.)
//------------------------------------------------------------------
static void (*recFPUOpXMM_to_XMM[] )(x86SSERegType, x86SSERegType) = {
	SSE2_ADDSD_XMM_to_XMM, SSE2_SUBSD_XMM_to_XMM };

void recFPUOp(int info, int regd, int op, bool acc)
{
	int sreg, treg;
	ALLOC_S(sreg);
	ALLOC_T(treg);

	FPU_ADD_SUB(sreg, treg);

	ToDouble(sreg);
	ToDouble(treg);

	recFPUOpXMM_to_XMM[op](sreg, treg);

	ToPS2FPU_Full(sreg, true, treg, acc, true);
	xMOVSS(xRegisterSSE(regd), xRegisterSSE(sreg));

	_freeXMMreg(sreg); _freeXMMreg(treg);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ADD XMM
//------------------------------------------------------------------
void recADD_S_xmm(int info)
{
    recFPUOp(info, EEREC_D, 0, false);
}

FPURECOMPILE_CONSTCODE(ADD_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

void recADDA_S_xmm(int info)
{
    recFPUOp(info, EEREC_ACC, 0, true);
}

FPURECOMPILE_CONSTCODE(ADDA_S, XMMINFO_WRITEACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------

void recCMP(int info)
{
	int sreg, treg;
	ALLOC_S(sreg); ALLOC_T(treg);
	ToDouble(sreg); ToDouble(treg);

	xUCOMI.SD(xRegisterSSE(sreg), xRegisterSSE(treg));

	_freeXMMreg(sreg); _freeXMMreg(treg);
}

//------------------------------------------------------------------
// C.x.S XMM
//------------------------------------------------------------------
void recC_EQ_xmm(int info)
{
	recCMP(info);

	j8Ptr[0] = JZ8(0);
		xAND(ptr32[&fpuRegs.fprc[31]], ~FPUflagC );
		j8Ptr[1] = JMP8(0);
	x86SetJ8(j8Ptr[0]);
		xOR(ptr32[&fpuRegs.fprc[31]], FPUflagC);
	x86SetJ8(j8Ptr[1]);
}

FPURECOMPILE_CONSTCODE(C_EQ, XMMINFO_READS|XMMINFO_READT);

void recC_LE_xmm(int info )
{
	recCMP(info);

	j8Ptr[0] = JBE8(0);
		xAND(ptr32[&fpuRegs.fprc[31]], ~FPUflagC );
		j8Ptr[1] = JMP8(0);
	x86SetJ8(j8Ptr[0]);
		xOR(ptr32[&fpuRegs.fprc[31]], FPUflagC);
	x86SetJ8(j8Ptr[1]);
}

FPURECOMPILE_CONSTCODE(C_LE, XMMINFO_READS|XMMINFO_READT);

void recC_LT_xmm(int info)
{
	recCMP(info);

	j8Ptr[0] = JB8(0);
		xAND(ptr32[&fpuRegs.fprc[31]], ~FPUflagC );
		j8Ptr[1] = JMP8(0);
	x86SetJ8(j8Ptr[0]);
		xOR(ptr32[&fpuRegs.fprc[31]], FPUflagC);
	x86SetJ8(j8Ptr[1]);
}

FPURECOMPILE_CONSTCODE(C_LT, XMMINFO_READS|XMMINFO_READT);
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

void recCVT_W(void) //called from iFPU.cpp's recCVT_W
{
	int regs = _checkXMMreg(XMMTYPE_FPREG, _Fs_, MODE_READ);

	if( regs >= 0 )
	{
		xCVTTSS2SI(eax, xRegisterSSE(regs));
		xMOVMSKPS(edx, xRegisterSSE(regs));	//extract the signs
		xAND(edx, 1);					//keep only LSB
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

	//--- Check for divide by zero ---
	xXOR.PS(xRegisterSSE(t1reg), xRegisterSSE(t1reg));
	xCMPEQ.SS(xRegisterSSE(t1reg), xRegisterSSE(regt));
	xMOVMSKPS(xRegister32(tempReg), xRegisterSSE(t1reg));
	xAND(xRegister32(tempReg), 1);  //Check sign (if regt == zero, sign will be set)
	ajmp32 = JZ32(0); //Skip if not set

		//--- Check for 0/0 ---
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

		//--- Make regd +/- Maximum ---
		xXOR.PS(xRegisterSSE(regd), xRegisterSSE(regt)); // Make regd Positive or Negative
		SetMaxValue(regd); //clamp to max
		bjmp32 = JMP32(0);

	x86SetJ32(ajmp32);

	//--- Normal Divide ---
	ToDouble(regd);
	ToDouble(regt);

	xDIV.SD(xRegisterSSE(regd), xRegisterSSE(regt));

	ToPS2FPU_Full(regd, false, regt, false, false);

	x86SetJ32(bjmp32);

	_freeXMMreg(t1reg);
	_freeX86reg(tempReg);
}

static __aligned16 SSE_MXCSR roundmode_nearest, roundmode_neg;

void recDIV_S_xmm(int info)
{
	bool roundmodeFlag = false;

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

	int sreg, treg;

	ALLOC_S(sreg);
	ALLOC_T(treg);

	recDIVhelper1(sreg, treg);

	xMOVSS(xRegisterSSE(EEREC_D), xRegisterSSE(sreg));

	if (roundmodeFlag) xLDMXCSR (g_sseMXCSR);
	_freeXMMreg(sreg); _freeXMMreg(treg);
}

FPURECOMPILE_CONSTCODE(DIV_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MADD/MSUB XMM
//------------------------------------------------------------------

// Unlike what the documentation implies, it seems that MADD/MSUB support all numbers just like other operations
// The complex overflow conditions the document describes apparently test whether the multiplication's result
// has overflowed and whether the last operation that used ACC as a destination has overflowed.
// For example,   { adda.s -MAX, 0.0 ; madd.s fd, MAX, 1.0 } -> fd = 0
// while          { adda.s -MAX, -MAX ; madd.s fd, MAX, 1.0 } -> fd = -MAX
// (where MAX is 0x7fffffff and -MAX is 0xffffffff)
static void recMaddsub(int info, int regd, int op, bool acc)
{
	int sreg, treg;
	ALLOC_S(sreg);
	ALLOC_T(treg);

	FPU_MUL(info, sreg, sreg, treg, false);

	GET_ACC(treg);

	FPU_ADD_SUB(treg, sreg); //might be problematic for something!!!!

	//          TEST FOR ACC/MUL OVERFLOWS, PROPOGATE THEM IF THEY OCCUR

	xTEST(ptr32[&fpuRegs.fprc[31]], FPUflagO);
	u8 *mulovf = JNZ8(0);
	ToDouble(sreg); //else, convert

	xTEST(ptr32[&fpuRegs.ACCflag], 1);
	u8 *accovf = JNZ8(0);
	ToDouble(treg); //else, convert
	u8 *operation = JMP8(0);

	x86SetJ8(mulovf);
	if (op == 1) //sub
		xXOR.PS(xRegisterSSE(sreg), ptr[s_const.neg]);
	xMOVAPS(xRegisterSSE(treg), xRegisterSSE(sreg));  //fall through below

	x86SetJ8(accovf);
	SetMaxValue(treg); //just in case... I think it has to be a MaxValue already here
	CLEAR_OU_FLAGS; //clear U flag
	xOR(ptr32[&fpuRegs.fprc[31]], FPUflagO | FPUflagSO);
	if (acc)
		xOR(ptr32[&fpuRegs.ACCflag], 1);
	u32 *skipall = JMP32(0);

	//PERFORM THE ACCUMULATION AND TEST RESULT. CONVERT TO SINGLE

	x86SetJ8(operation);
	if (op == 1)
		xSUB.SD(xRegisterSSE(treg), xRegisterSSE(sreg));
	else
		xADD.SD(xRegisterSSE(treg), xRegisterSSE(sreg));

	ToPS2FPU_Full(treg, true, sreg, acc, true);
	x86SetJ32(skipall);

	xMOVSS(xRegisterSSE(regd), xRegisterSSE(treg));

	_freeXMMreg(sreg);
	_freeXMMreg(treg);
}

void recMADD_S_xmm(int info)
{
	recMaddsub(info, EEREC_D, 0, false);
}

FPURECOMPILE_CONSTCODE(MADD_S, XMMINFO_WRITED|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);

void recMADDA_S_xmm(int info)
{
	recMaddsub(info, EEREC_ACC, 0, true);
}

FPURECOMPILE_CONSTCODE(MADDA_S, XMMINFO_WRITEACC|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MAX / MIN XMM
//------------------------------------------------------------------

static const __aligned16 u32 minmax_mask[8] =
{
	0xffffffff,	0x80000000, 0, 0,
	0,			0x40000000, 0, 0
};

// FPU's MAX/MIN work with all numbers (including "denormals"). Check VU's logical min max for more info.
static void recMINMAX(int info, bool ismin)
{
	int sreg, treg;
	ALLOC_S(sreg);
	ALLOC_T(treg);

	CLEAR_OU_FLAGS;

	xPSHUF.D(xRegisterSSE(sreg), xRegisterSSE(sreg), 0x00);
	xPAND(xRegisterSSE(sreg), ptr[minmax_mask]);
	xPOR(xRegisterSSE(sreg), ptr[&minmax_mask[4]]);
	xPSHUF.D(xRegisterSSE(treg), xRegisterSSE(treg), 0x00);
	xPAND(xRegisterSSE(treg), ptr[minmax_mask]);
	xPOR(xRegisterSSE(treg), ptr[&minmax_mask[4]]);
	if (ismin)
		xMIN.SD(xRegisterSSE(sreg), xRegisterSSE(treg));
	else
		xMAX.SD(xRegisterSSE(sreg), xRegisterSSE(treg));

	xMOVSS(xRegisterSSE(EEREC_D), xRegisterSSE(sreg));

	_freeXMMreg(sreg);
	_freeXMMreg(treg);
}

void recMAX_S_xmm(int info)
{
	recMINMAX(info, false);
}

FPURECOMPILE_CONSTCODE(MAX_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

void recMIN_S_xmm(int info)
{
	recMINMAX(info, true);
}

FPURECOMPILE_CONSTCODE(MIN_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MOV XMM
//------------------------------------------------------------------
void recMOV_S_xmm(int info)
{
	GET_S(EEREC_D);
}

FPURECOMPILE_CONSTCODE(MOV_S, XMMINFO_WRITED|XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MSUB XMM
//------------------------------------------------------------------

void recMSUB_S_xmm(int info)
{
	recMaddsub(info, EEREC_D, 1, false);
}

FPURECOMPILE_CONSTCODE(MSUB_S, XMMINFO_WRITED|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);

void recMSUBA_S_xmm(int info)
{
	recMaddsub(info, EEREC_ACC, 1, true);
}

FPURECOMPILE_CONSTCODE(MSUBA_S, XMMINFO_WRITEACC|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------

//------------------------------------------------------------------
// MUL XMM
//------------------------------------------------------------------
void recMUL_S_xmm(int info)
{
	int sreg, treg;
	ALLOC_S(sreg); ALLOC_T(treg);

	FPU_MUL(info, EEREC_D, sreg, treg, false);
	_freeXMMreg(sreg); _freeXMMreg(treg);
}

FPURECOMPILE_CONSTCODE(MUL_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

void recMULA_S_xmm(int info)
{
	int sreg, treg;
	ALLOC_S(sreg);
	ALLOC_T(treg);

	FPU_MUL(info, EEREC_ACC, sreg, treg, true);
	_freeXMMreg(sreg);
	_freeXMMreg(treg);
}

FPURECOMPILE_CONSTCODE(MULA_S, XMMINFO_WRITEACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// NEG XMM
//------------------------------------------------------------------
void recNEG_S_xmm(int info)
{
	GET_S(EEREC_D);

	CLEAR_OU_FLAGS;

	xXOR.PS(xRegisterSSE(EEREC_D), ptr[&s_const.neg[0]]);
}

FPURECOMPILE_CONSTCODE(NEG_S, XMMINFO_WRITED|XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// SUB XMM
//------------------------------------------------------------------

void recSUB_S_xmm(int info)
{
	recFPUOp(info, EEREC_D, 1, false);
}

FPURECOMPILE_CONSTCODE(SUB_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);


void recSUBA_S_xmm(int info)
{
	recFPUOp(info, EEREC_ACC, 1, true);
}

FPURECOMPILE_CONSTCODE(SUBA_S, XMMINFO_WRITEACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// SQRT XMM
//------------------------------------------------------------------
void recSQRT_S_xmm(int info)
{
	u8 *pjmp;
	int roundmodeFlag = 0;
	int tempReg = _allocX86reg(xEmptyReg, X86TYPE_TEMP, 0, 0);
	int t1reg = _allocTempXMMreg(XMMT_FPS, -1);

	if (g_sseMXCSR.GetRoundMode() != SSEround_Nearest)
	{
		// Set roundmode to nearest if it isn't already
		roundmode_nearest = g_sseMXCSR;
		roundmode_nearest.SetRoundMode( SSEround_Nearest );
		xLDMXCSR (roundmode_nearest);
		roundmodeFlag = 1;
	}

	GET_T(EEREC_D);

	xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagI|FPUflagD)); // Clear I and D flags

	//--- Check for negative SQRT --- (sqrt(-0) = 0, unlike what the docs say)
	xMOVMSKPS(xRegister32(tempReg), xRegisterSSE(EEREC_D));
	xAND(xRegister32(tempReg), 1);  //Check sign
	pjmp = JZ8(0); //Skip if none are
	xOR(ptr32[&fpuRegs.fprc[31]], FPUflagI|FPUflagSI); // Set I and SI flags
	xAND.PS(xRegisterSSE(EEREC_D), ptr[&s_const.pos[0]]); // Make EEREC_D Positive
	x86SetJ8(pjmp);


	ToDouble(EEREC_D);

	xSQRT.SD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));

	ToPS2FPU_Full(EEREC_D, false, t1reg, false, false);

	if (roundmodeFlag == 1) {
		xLDMXCSR (g_sseMXCSR);
	}

	_freeX86reg(tempReg);
	_freeXMMreg(t1reg);
}

FPURECOMPILE_CONSTCODE(SQRT_S, XMMINFO_WRITED|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// RSQRT XMM
//------------------------------------------------------------------
void recRSQRThelper1(int regd, int regt) // Preforms the RSQRT function when regd <- Fs and regt <- Ft (Sets correct flags)
{
	u8 *pjmp1, *pjmp2;
	u8 *qjmp1, *qjmp2;
	u32 *pjmp32;
	int t1reg = _allocTempXMMreg(XMMT_FPS, -1);
	int tempReg = _allocX86reg(xEmptyReg, X86TYPE_TEMP, 0, 0);

	xAND(ptr32[&fpuRegs.fprc[31]], ~(FPUflagI|FPUflagD)); // Clear I and D flags

	//--- (first) Check for negative SQRT ---
	xMOVMSKPS(xRegister32(tempReg), xRegisterSSE(regt));
	xAND(xRegister32(tempReg), 1);  //Check sign
	pjmp2 = JZ8(0); //Skip if not set
		xOR(ptr32[&fpuRegs.fprc[31]], FPUflagI|FPUflagSI); // Set I and SI flags
		xAND.PS(xRegisterSSE(regt), ptr[&s_const.pos[0]]); // Make regt Positive
	x86SetJ8(pjmp2);

	//--- Check for zero ---
	xXOR.PS(xRegisterSSE(t1reg), xRegisterSSE(t1reg));
	xCMPEQ.SS(xRegisterSSE(t1reg), xRegisterSSE(regt));
	xMOVMSKPS(xRegister32(tempReg), xRegisterSSE(t1reg));
	xAND(xRegister32(tempReg), 1);  //Check sign (if regt == zero, sign will be set)
	pjmp1 = JZ8(0); //Skip if not set

		//--- Check for 0/0 ---
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

		SetMaxValue(regd); //clamp to max
		pjmp32 = JMP32(0);
	x86SetJ8(pjmp1);

	ToDouble(regt);
	ToDouble(regd);

	xSQRT.SD(xRegisterSSE(regt), xRegisterSSE(regt));
	xDIV.SD(xRegisterSSE(regd), xRegisterSSE(regt));

	ToPS2FPU_Full(regd, false, regt, false, false);
	x86SetJ32(pjmp32);

	_freeXMMreg(t1reg);
	_freeX86reg(tempReg);
}

void recRSQRThelper2(int regd, int regt) // Preforms the RSQRT function when regd <- Fs and regt <- Ft (Doesn't set flags)
{
	xAND.PS(xRegisterSSE(regt), ptr[&s_const.pos[0]]); // Make regt Positive

	ToDouble(regt);
	ToDouble(regd);

	xSQRT.SD(xRegisterSSE(regt), xRegisterSSE(regt));
	xDIV.SD(xRegisterSSE(regd), xRegisterSSE(regt));

	ToPS2FPU_Full(regd, false, regt, false, false);
}

void recRSQRT_S_xmm(int info)
{
	int sreg, treg;

	// iFPU (regular FPU) doesn't touch roundmode for rSQRT.
	// Should this do the same?  or is changing the roundmode to nearest the better
	// behavior for both recs? --air

	bool roundmodeFlag = false;
	if (g_sseMXCSR.GetRoundMode() != SSEround_Nearest)
	{
		// Set roundmode to nearest if it isn't already
		roundmode_nearest = g_sseMXCSR;
		roundmode_nearest.SetRoundMode( SSEround_Nearest );
		xLDMXCSR (roundmode_nearest);
		roundmodeFlag = true;
	}

	ALLOC_S(sreg);
	ALLOC_T(treg);

	recRSQRThelper1(sreg, treg);

	xMOVSS(xRegisterSSE(EEREC_D), xRegisterSSE(sreg));

	_freeXMMreg(treg);
	_freeXMMreg(sreg);

	if (roundmodeFlag) xLDMXCSR (g_sseMXCSR);
}

FPURECOMPILE_CONSTCODE(RSQRT_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);


} } } } }
#endif
