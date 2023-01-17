/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "Pcsx2Types.h"
#include "../../pcsx2/GifTypes.h"

#define VM_SIZE 4194304u
#define HALF_VM_SIZE (VM_SIZE / 2u)
#define PAGE_SIZE 8192u
#define BLOCK_SIZE 256u
#define COLUMN_SIZE 64u

#define MAX_PAGES (VM_SIZE / PAGE_SIZE)
#define MAX_BLOCKS (VM_SIZE / BLOCK_SIZE)
#define MAX_COLUMNS (VM_SIZE / COLUMN_SIZE)

#include <map>
#include "GSVector.h"

#pragma pack(push, 1)

enum GS_PRIM
{
	GS_POINTLIST		= 0,
	GS_LINELIST		= 1,
	GS_LINESTRIP		= 2,
	GS_TRIANGLELIST		= 3,
	GS_TRIANGLESTRIP	= 4,
	GS_TRIANGLEFAN		= 5,
	GS_SPRITE		= 6,
	GS_INVALID		= 7
};

enum GS_PRIM_CLASS
{
	GS_POINT_CLASS		= 0,
	GS_LINE_CLASS		= 1,
	GS_TRIANGLE_CLASS	= 2,
	GS_SPRITE_CLASS		= 3,
	GS_INVALID_CLASS	= 7
};

enum GS_PSM
{
	PSM_PSMCT32	= 0,  // 0000-0000
	PSM_PSMCT24	= 1,  // 0000-0001
	PSM_PSMCT16	= 2,  // 0000-0010
	PSM_PSMCT16S	= 10, // 0000-1010
	PSM_PSGPU24     = 18, // 0001-0010
	PSM_PSMT8	= 19, // 0001-0011
	PSM_PSMT4	= 20, // 0001-0100
	PSM_PSMT8H	= 27, // 0001-1011
	PSM_PSMT4HL	= 36, // 0010-0100
	PSM_PSMT4HH	= 44, // 0010-1100
	PSM_PSMZ32	= 48, // 0011-0000
	PSM_PSMZ24	= 49, // 0011-0001
	PSM_PSMZ16	= 50, // 0011-0010
	PSM_PSMZ16S	= 58  // 0011-1010
};

enum GS_TFX
{
	TFX_MODULATE	= 0,
	TFX_DECAL	= 1,
	TFX_HIGHLIGHT	= 2,
	TFX_HIGHLIGHT2	= 3,
	TFX_NONE	= 4
};

enum GS_CLAMP
{
	CLAMP_REPEAT		= 0,
	CLAMP_CLAMP		= 1,
	CLAMP_REGION_CLAMP	= 2,
	CLAMP_REGION_REPEAT	= 3
};

enum GS_ZTST
{
	ZTST_NEVER	= 0,
	ZTST_ALWAYS	= 1,
	ZTST_GEQUAL	= 2,
	ZTST_GREATER	= 3
};

enum GS_ATST
{
	ATST_NEVER	= 0,
	ATST_ALWAYS	= 1,
	ATST_LESS	= 2,
	ATST_LEQUAL	= 3,
	ATST_EQUAL	= 4,
	ATST_GEQUAL	= 5,
	ATST_GREATER	= 6,
	ATST_NOTEQUAL	= 7
};

enum GS_AFAIL
{
	AFAIL_KEEP	= 0,
	AFAIL_FB_ONLY	= 1,
	AFAIL_ZB_ONLY	= 2,
	AFAIL_RGB_ONLY	= 3
};

enum class GS_MIN_FILTER : u8
{
	Nearest                = 0,
	Linear                 = 1,
	Nearest_Mipmap_Nearest = 2,
	Nearest_Mipmap_Linear  = 3,
	Linear_Mipmap_Nearest  = 4,
	Linear_Mipmap_Linear   = 5
};

enum class GSRendererType : int8_t
{
	Undefined = -1,
	DX1011_HW = 3,
	OGL_HW = 12,
	OGL_SW,
#ifdef _WIN32
	Default = Undefined
#else
	// Use ogl renderer as default otherwise it crash at startup
	// GSRenderOGL only GSDeviceOGL (not GSDeviceNULL)
	Default = OGL_HW
#endif

};


#define REG32(name) \
union name			\
{					\
	u32 U32;	\
	struct {		\

#define REG64(name) \
union name			\
{					\
	u64 U64;		\
	u32 U32[2];	\
	void operator = (const GSVector4i& v) {GSVector4i::storel(this, v);} \
	bool operator == (const union name& r) const {return ((GSVector4i)r).eq(*this);} \
	bool operator != (const union name& r) const {return !((GSVector4i)r).eq(*this);} \
	operator GSVector4i() const {return GSVector4i::loadl(this);} \
	struct {		\

#define REG128(name)\
union name			\
{					\
	u64 U64[2];	\
	u32 U32[4];	\
	struct {		\

#define REG32_(prefix, name) REG32(prefix##name)
#define REG64_(prefix, name) REG64(prefix##name)
#define REG128_(prefix, name) REG128(prefix##name)

#define REG_END }; };
#define REG_END2 };

#define REG32_SET(name) \
union name			\
{					\
	u32 U32;	\

#define REG64_SET(name) \
union name			\
{					\
	u64 U64;		\
	u32 U32[2];	\

#define REG128_SET(name)\
union name			\
{					\
	__m128i m128;  \
	u64 U64[2];	\
	u32 U32[4];	\

#define REG_SET_END };

REG64_(GSReg, BGCOLOR)
	u8 R;
	u8 G;
	u8 B;
	u8 _PAD1[5];
REG_END

REG64_(GSReg, BUSDIR)
	u32 DIR  :1;
	u32 _PAD1:31;
	u32 _PAD2:32;
REG_END

REG64_(GSReg, CSR)
	u32 rSIGNAL:1;
	u32 rFINISH:1;
	u32 rHSINT:1;
	u32 rVSINT:1;
	u32 rEDWINT:1;
	u32 rZERO1:1;
	u32 rZERO2:1;
	u32 r_PAD1:1;
	u32 rFLUSH:1;
	u32 rRESET:1;
	u32 r_PAD2:2;
	u32 rNFIELD:1;
	u32 rFIELD:1;
	u32 rFIFO:2;
	u32 rREV:8;
	u32 rID:8;
	u32 wSIGNAL:1;
	u32 wFINISH:1;
	u32 wHSINT:1;
	u32 wVSINT:1;
	u32 wEDWINT:1;
	u32 wZERO1:1;
	u32 wZERO2:1;
	u32 w_PAD1:1;
	u32 wFLUSH:1;
	u32 wRESET:1;
	u32 w_PAD2:2;
	u32 wNFIELD:1;
	u32 wFIELD:1;
	u32 wFIFO:2;
	u32 wREV:8;
	u32 wID:8;
REG_END

REG64_(GSReg, DISPFB) // (-1/2)
	u32 FBP:9;
	u32 FBW:6;
	u32 PSM:5;
	u32 _PAD:12;
	u32 DBX:11;
	u32 DBY:11;
	u32 _PAD2:10;
REG_END2
	u32 Block() const {return FBP << 5;}
REG_END2

REG64_(GSReg, DISPLAY) // (-1/2)
	u32 DX:12;
	u32 DY:11;
	u32 MAGH:4;
	u32 MAGV:2;
	u32 _PAD:3;
	u32 DW:12;
	u32 DH:11;
	u32 _PAD2:9;
REG_END

REG64_(GSReg, EXTBUF)
	u32 EXBP:14;
	u32 EXBW:6;
	u32 FBIN:2;
	u32 WFFMD:1;
	u32 EMODA:2;
	u32 EMODC:2;
	u32 _PAD1:5;
	u32 WDX:11;
	u32 WDY:11;
	u32 _PAD2:10;
REG_END

REG64_(GSReg, EXTDATA)
	u32 SX:12;
	u32 SY:11;
	u32 SMPH:4;
	u32 SMPV:2;
	u32 _PAD1:3;
	u32 WW:12;
	u32 WH:11;
	u32 _PAD2:9;
REG_END

REG64_(GSReg, EXTWRITE)
	u32 WRITE:1;
	u32 _PAD1:31;
	u32 _PAD2:32;
REG_END

REG64_(GSReg, IMR)
	u32 _PAD1:8;
	u32 SIGMSK:1;
	u32 FINISHMSK:1;
	u32 HSMSK:1;
	u32 VSMSK:1;
	u32 EDWMSK:1;
	u32 _PAD2:19;
	u32 _PAD3:32;
REG_END

REG64_(GSReg, PMODE)
union
{
	struct
	{
		u32 EN1:1;
		u32 EN2:1;
		u32 CRTMD:3;
		u32 MMOD:1;
		u32 AMOD:1;
		u32 SLBG:1;
		u32 ALP:8;
		u32 _PAD:16;
		u32 _PAD1:32;
	};

	struct
	{
		u32 EN:2;
		u32 _PAD2:30;
		u32 _PAD3:32;
	};
};
REG_END

REG64_(GSReg, SIGLBLID)
	u32 SIGID;
	u32 LBLID;
REG_END

REG64_(GSReg, SMODE1)
	u32 RC:3;
	u32 LC:7;
	u32 T1248:2;
	u32 SLCK:1;
	u32 CMOD:2;
	u32 EX:1;
	u32 PRST:1;
	u32 SINT:1;
	u32 XPCK:1;
	u32 PCK2:2;
	u32 SPML:4;
	u32 GCONT:1; // YCrCb
	u32 PHS:1;
	u32 PVS:1;
	u32 PEHS:1;
	u32 PEVS:1;
	u32 CLKSEL:2;
	u32 NVCK:1;
	u32 SLCK2:1;
	u32 VCKSEL:2;
	u32 VHP:1;
	u32 _PAD1:27;
REG_END

/*

// pal

CLKSEL=1 CMOD=3 EX=0 GCONT=0 LC=32 NVCK=1 PCK2=0 PEHS=0 PEVS=0 PHS=0 PRST=1 PVS=0 RC=4 SINT=0 SLCK=0 SLCK2=1 SPML=4 T1248=1 VCKSEL=1 VHP=0 XPCK=0

// ntsc

CLKSEL=1 CMOD=2 EX=0 GCONT=0 LC=32 NVCK=1 PCK2=0 PEHS=0 PEVS=0 PHS=0 PRST=1 PVS=0 RC=4 SINT=0 SLCK=0 SLCK2=1 SPML=4 T1248=1 VCKSEL=1 VHP=0 XPCK=0

// ntsc progressive (SoTC)

CLKSEL=1 CMOD=0 EX=0 GCONT=0 LC=32 NVCK=1 PCK2=0 PEHS=0 PEVS=0 PHS=0 PRST=1 PVS=0 RC=4 SINT=0 SLCK=0 SLCK2=1 SPML=2 T1248=1 VCKSEL=1 VHP=1 XPCK=0

*/

REG64_(GSReg, SMODE2)
	u32 INT:1;
	u32 FFMD:1;
	u32 DPMS:2;
	u32 _PAD2:28;
	u32 _PAD3:32;
REG_END

REG64_(GSReg, SRFSH)
	u32 _DUMMY;
	// TODO
REG_END

REG64_(GSReg, SYNCH1)
	u32 _DUMMY;
	// TODO
REG_END

REG64_(GSReg, SYNCH2)
	u32 _DUMMY;
	// TODO
REG_END

REG64_(GSReg, SYNCV)
	u32 VFP:10; // Vertical Front Porchinterval (?s)
	u32 VFPE:10; // Vertical Front Porchinterval End (?s)
	u32 VBP:12; // Vertical Back Porchinterval (?s)
	u32 VBPE:10; // Vertical Back Porchinterval End (?s)
	u32 VDP:11; // Vertical Differential Phase
	u32 VS:11; // Vertical Synchronization Timing
REG_END

REG64_SET(GSReg)
	GSRegBGCOLOR	BGCOLOR;
	GSRegBUSDIR		BUSDIR;
	GSRegCSR		CSR;
	GSRegDISPFB		DISPFB;
	GSRegDISPLAY	DISPLAY;
	GSRegEXTBUF		EXTBUF;
	GSRegEXTDATA	EXTDATA;
	GSRegEXTWRITE	EXTWRITE;
	GSRegIMR		IMR;
	GSRegPMODE		PMODE;
	GSRegSIGLBLID	SIGLBLID;
	GSRegSMODE1		SMODE1;
	GSRegSMODE2		SMODE2;
REG_SET_END

//
// GIFTag

REG128(GIFTag)
	u32 NLOOP:15;
	u32 EOP:1;
	u32 _PAD1:16;
	u32 _PAD2:14;
	u32 PRE:1;
	u32 PRIM:11;
	u32 FLG:2; // enum GIF_FLG
	u32 NREG:4;
	u64 REGS;
REG_END

// GIFReg

REG64_(GIFReg, ALPHA)
	u32 A:2;
	u32 B:2;
	u32 C:2;
	u32 D:2;
	u32 _PAD1:24;
	u8 FIX;
	u8 _PAD2[3];
REG_END2
	// opaque => output will be Cs/As
	GS_FORCEINLINE bool IsOpaque() const {return ((A == B || (C == 2 && FIX == 0)) && D == 0) || (A == 0 && B == D && C == 2 && FIX == 0x80);}
	GS_FORCEINLINE bool IsOpaque(int amin, int amax) const {return ((A == B || amax == 0) && D == 0) || (A == 0 && B == D && amin == 0x80 && amax == 0x80);}
	GS_FORCEINLINE bool IsCd() { return (A == B) && (D == 1);}
REG_END2

REG64_(GIFReg, BITBLTBUF)
	u32 SBP:14;
	u32 _PAD1:2;
	u32 SBW:6;
	u32 _PAD2:2;
	u32 SPSM:6;
	u32 _PAD3:2;
	u32 DBP:14;
	u32 _PAD4:2;
	u32 DBW:6;
	u32 _PAD5:2;
	u32 DPSM:6;
	u32 _PAD6:2;
REG_END

REG64_(GIFReg, CLAMP)
union
{
	struct
	{
		u32 WMS:2;
		u32 WMT:2;
		u32 MINU:10;
		u32 MAXU:10;
		u32 _PAD1:8;
		u32 _PAD2:2;
		u32 MAXV:10;
		u32 _PAD3:20;
	};

	struct
	{
		u64 _PAD4:24;
		u64 MINV:10;
		u64 _PAD5:30;
	};
};
REG_END

REG64_(GIFReg, COLCLAMP)
	u32 CLAMP:1;
	u32 _PAD1:31;
	u32 _PAD2:32;
REG_END

REG64_(GIFReg, DIMX)
	s32 DM00:3;
	s32 _PAD00:1;
	s32 DM01:3;
	s32 _PAD01:1;
	s32 DM02:3;
	s32 _PAD02:1;
	s32 DM03:3;
	s32 _PAD03:1;
	s32 DM10:3;
	s32 _PAD10:1;
	s32 DM11:3;
	s32 _PAD11:1;
	s32 DM12:3;
	s32 _PAD12:1;
	s32 DM13:3;
	s32 _PAD13:1;
	s32 DM20:3;
	s32 _PAD20:1;
	s32 DM21:3;
	s32 _PAD21:1;
	s32 DM22:3;
	s32 _PAD22:1;
	s32 DM23:3;
	s32 _PAD23:1;
	s32 DM30:3;
	s32 _PAD30:1;
	s32 DM31:3;
	s32 _PAD31:1;
	s32 DM32:3;
	s32 _PAD32:1;
	s32 DM33:3;
	s32 _PAD33:1;
REG_END

REG64_(GIFReg, DTHE)
	u32 DTHE:1;
	u32 _PAD1:31;
	u32 _PAD2:32;
REG_END

REG64_(GIFReg, FBA)
	u32 FBA:1;
	u32 _PAD1:31;
	u32 _PAD2:32;
REG_END

REG64_(GIFReg, FINISH)
	u32 _PAD1[2];
REG_END

REG64_(GIFReg, FOG)
	u8 _PAD1[7];
	u8 F;
REG_END

REG64_(GIFReg, FOGCOL)
	u8 FCR;
	u8 FCG;
	u8 FCB;
	u8 _PAD1[5];
REG_END

REG64_(GIFReg, FRAME)
	u32 FBP:9;
	u32 _PAD1:7;
	u32 FBW:6;
	u32 _PAD2:2;
	u32 PSM:6;
	u32 _PAD3:2;
	u32 FBMSK;
REG_END2
REG_END2

#define GIFREG_FRAME_BLOCK(x) ((x).FBP << 5)

REG64_(GIFReg, HWREG)
	u32 DATA_LOWER;
	u32 DATA_UPPER;
REG_END

REG64_(GIFReg, LABEL)
	u32 ID;
	u32 IDMSK;
REG_END

REG64_(GIFReg, MIPTBP1)
	u64 TBP1:14;
	u64 TBW1:6;
	u64 TBP2:14;
	u64 TBW2:6;
	u64 TBP3:14;
	u64 TBW3:6;
	u64 _PAD:4;
REG_END

REG64_(GIFReg, MIPTBP2)
	u64 TBP4:14;
	u64 TBW4:6;
	u64 TBP5:14;
	u64 TBW5:6;
	u64 TBP6:14;
	u64 TBW6:6;
	u64 _PAD:4;
REG_END

REG64_(GIFReg, NOP)
	u32 _PAD[2];
REG_END

REG64_(GIFReg, PABE)
	u32 PABE:1;
	u32 _PAD1:31;
	u32 _PAD2:32;
REG_END

REG64_(GIFReg, PRIM)
	u32 PRIM:3;
	u32 IIP:1;
	u32 TME:1;
	u32 FGE:1;
	u32 ABE:1;
	u32 AA1:1;
	u32 FST:1;
	u32 CTXT:1;
	u32 FIX:1;
	u32 _PAD1:21;
	u32 _PAD2:32;
REG_END

REG64_(GIFReg, PRMODE)
	u32 _PRIM:3;
	u32 IIP:1;
	u32 TME:1;
	u32 FGE:1;
	u32 ABE:1;
	u32 AA1:1;
	u32 FST:1;
	u32 CTXT:1;
	u32 FIX:1;
	u32 _PAD2:21;
	u32 _PAD3:32;
REG_END

REG64_(GIFReg, PRMODECONT)
	u32 AC:1;
	u32 _PAD1:31;
	u32 _PAD2:32;
REG_END

REG64_(GIFReg, RGBAQ)
	u8 R;
	u8 G;
	u8 B;
	u8 A;
	float Q;
REG_END

REG64_(GIFReg, SCANMSK)
	u32 MSK:2;
	u32 _PAD1:30;
	u32 _PAD2:32;
REG_END

REG64_(GIFReg, SCISSOR)
	u32 SCAX0:11;
	u32 _PAD1:5;
	u32 SCAX1:11;
	u32 _PAD2:5;
	u32 SCAY0:11;
	u32 _PAD3:5;
	u32 SCAY1:11;
	u32 _PAD4:5;
REG_END

REG64_(GIFReg, SIGNAL)
	u32 ID;
	u32 IDMSK;
REG_END

REG64_(GIFReg, ST)
	float S;
	float T;
REG_END

REG64_(GIFReg, TEST)
	u32 ATE:1;
	u32 ATST:3;
	u32 AREF:8;
	u32 AFAIL:2;
	u32 DATE:1;
	u32 DATM:1;
	u32 ZTE:1;
	u32 ZTST:2;
	u32 _PAD1:13;
	u32 _PAD2:32;
REG_END2
	GS_FORCEINLINE bool DoFirstPass() const {return !ATE || ATST != ATST_NEVER;} // not all pixels fail automatically
	GS_FORCEINLINE bool DoSecondPass() const {return ATE && ATST != ATST_ALWAYS && AFAIL != AFAIL_KEEP;} // pixels may fail, write fb/z
	GS_FORCEINLINE bool NoSecondPass() const {return ATE && ATST != ATST_ALWAYS && AFAIL == AFAIL_KEEP;} // pixels may fail, no output
REG_END2

REG64_(GIFReg, TEX0)
union
{
	struct
	{
		u32 TBP0:14;
		u32 TBW:6;
		u32 PSM:6;
		u32 TW:4;
		u32 _PAD1:2;
		u32 _PAD2:2;
		u32 TCC:1;
		u32 TFX:2;
		u32 CBP:14;
		u32 CPSM:4;
		u32 CSM:1;
		u32 CSA:5;
		u32 CLD:3;
	};

	struct
	{
		u64 _PAD3:30;
		u64 TH:4;
		u64 _PAD4:30;
	};
};
REG_END2
	GS_FORCEINLINE bool IsRepeating() const
	{
		if(TBW < 2)
		{
			if(PSM == PSM_PSMT8) return TW > 7 || TH > 6;
			if(PSM == PSM_PSMT4) return TW > 7 || TH > 7;
		}

		// The recast of TBW seems useless but it avoid tons of warning from GCC...
		return ((u32)TBW << 6u) < (1u << TW);
	}
REG_END2

REG64_(GIFReg, TEX1)
	u32 LCM:1;
	u32 _PAD1:1;
	u32 MXL:3;
	u32 MMAG:1;
	u32 MMIN:3;
	u32 MTBA:1;
	u32 _PAD2:9;
	u32 L:2;
	u32 _PAD3:11;
	s32  K:12; // 1:7:4
	u32 _PAD4:20;
REG_END2
REG_END2

REG64_(GIFReg, TEX2)
	u32 _PAD1:20;
	u32 PSM:6;
	u32 _PAD2:6;
	u32 _PAD3:5;
	u32 CBP:14;
	u32 CPSM:4;
	u32 CSM:1;
	u32 CSA:5;
	u32 CLD:3;
REG_END

REG64_(GIFReg, TEXA)
	u8 TA0;
	u8 _PAD1:7;
	u8 AEM:1;
	u16 _PAD2;
	u8 TA1:8;
	u8 _PAD3[3];
REG_END

REG64_(GIFReg, TEXCLUT)
	u32 CBW:6;
	u32 COU:6;
	u32 COV:10;
	u32 _PAD1:10;
	u32 _PAD2:32;
REG_END

REG64_(GIFReg, TEXFLUSH)
	u32 _PAD1:32;
	u32 _PAD2:32;
REG_END

REG64_(GIFReg, TRXDIR)
	u32 XDIR:2;
	u32 _PAD1:30;
	u32 _PAD2:32;
REG_END

REG64_(GIFReg, TRXPOS)
	u32 SSAX:11;
	u32 _PAD1:5;
	u32 SSAY:11;
	u32 _PAD2:5;
	u32 DSAX:11;
	u32 _PAD3:5;
	u32 DSAY:11;
	u32 DIRY:1;
	u32 DIRX:1;
	u32 _PAD4:3;
REG_END

REG64_(GIFReg, TRXREG)
	u32 RRW:12;
	u32 _PAD1:20;
	u32 RRH:12;
	u32 _PAD2:20;
REG_END

// GSState::GIFPackedRegHandlerUV and GSState::GIFRegHandlerUV will make sure that the _PAD1/2 bits are set to zero

REG64_(GIFReg, UV)
	u16 U;
//	u32 _PAD1:2;
	u16 V;
//	u32 _PAD2:2;
	u32 _PAD3;
REG_END

// GSState::GIFRegHandlerXYOFFSET will make sure that the _PAD1/2 bits are set to zero

REG64_(GIFReg, XYOFFSET)
	u32 OFX; // :16; u32 _PAD1:16;
	u32 OFY; // :16; u32 _PAD2:16;
REG_END

REG64_(GIFReg, XYZ)
	u16 X;
	u16 Y;
	u32 Z;
REG_END

REG64_(GIFReg, XYZF)
	u16 X;
	u16 Y;
	u32 Z:24;
	u32 F:8;
REG_END

REG64_(GIFReg, ZBUF)
	u32 ZBP:9;
	u32 _PAD1:15;
	// u32 PSM:4;
	// u32 _PAD2:4;
	u32 PSM:6;
	u32 _PAD2:2;
	u32 ZMSK:1;
	u32 _PAD3:31;
REG_END2
REG_END2

#define GIFREG_ZBUF_BLOCK(x) ((x).ZBP << 5)

REG64_SET(GIFReg)
	GIFRegALPHA			ALPHA;
	GIFRegBITBLTBUF		BITBLTBUF;
	GIFRegCLAMP			CLAMP;
	GIFRegCOLCLAMP		COLCLAMP;
	GIFRegDIMX			DIMX;
	GIFRegDTHE			DTHE;
	GIFRegFBA			FBA;
	GIFRegFINISH		FINISH;
	GIFRegFOG			FOG;
	GIFRegFOGCOL		FOGCOL;
	GIFRegFRAME			FRAME;
	GIFRegHWREG			HWREG;
	GIFRegLABEL			LABEL;
	GIFRegMIPTBP1		MIPTBP1;
	GIFRegMIPTBP2		MIPTBP2;
	GIFRegNOP			NOP;
	GIFRegPABE			PABE;
	GIFRegPRIM			PRIM;
	GIFRegPRMODE		PRMODE;
	GIFRegPRMODECONT	PRMODECONT;
	GIFRegRGBAQ			RGBAQ;
	GIFRegSCANMSK		SCANMSK;
	GIFRegSCISSOR		SCISSOR;
	GIFRegSIGNAL		SIGNAL;
	GIFRegST			ST;
	GIFRegTEST			TEST;
	GIFRegTEX0			TEX0;
	GIFRegTEX1			TEX1;
	GIFRegTEX2			TEX2;
	GIFRegTEXA			TEXA;
	GIFRegTEXCLUT		TEXCLUT;
	GIFRegTEXFLUSH		TEXFLUSH;
	GIFRegTRXDIR		TRXDIR;
	GIFRegTRXPOS		TRXPOS;
	GIFRegTRXREG		TRXREG;
	GIFRegUV			UV;
	GIFRegXYOFFSET		XYOFFSET;
	GIFRegXYZ			XYZ;
	GIFRegXYZF			XYZF;
	GIFRegZBUF			ZBUF;
REG_SET_END

// GIFPacked

REG128_(GIFPacked, PRIM)
	u32 PRIM:11;
	u32 _PAD1:21;
	u32 _PAD2[3];
REG_END

REG128_(GIFPacked, RGBA)
	u8 R;
	u8 _PAD1[3];
	u8 G;
	u8 _PAD2[3];
	u8 B;
	u8 _PAD3[3];
	u8 A;
	u8 _PAD4[3];
REG_END

REG128_(GIFPacked, STQ)
	float S;
	float T;
	float Q;
	u32 _PAD1:32;
REG_END

REG128_(GIFPacked, UV)
	u32 U:14;
	u32 _PAD1:18;
	u32 V:14;
	u32 _PAD2:18;
	u32 _PAD3:32;
	u32 _PAD4:32;
REG_END

REG128_(GIFPacked, XYZF2)
	u16 X;
	u16 _PAD1;
	u16 Y;
	u16 _PAD2;
	u32 _PAD3:4;
	u32 Z:24;
	u32 _PAD4:4;
	u32 _PAD5:4;
	u32 F:8;
	u32 _PAD6:3;
	u32 ADC:1;
	u32 _PAD7:16;
REG_END2
	u32 Skip() const {return U32[3] & 0x8000;}
REG_END2

REG128_(GIFPacked, XYZ2)
	u16 X;
	u16 _PAD1;
	u16 Y;
	u16 _PAD2;
	u32 Z;
	u32 _PAD3:15;
	u32 ADC:1;
	u32 _PAD4:16;
REG_END2
	u32 Skip() const {return U32[3] & 0x8000;}
REG_END2

REG128_(GIFPacked, FOG)
	u32 _PAD1;
	u32 _PAD2;
	u32 _PAD3;
	u32 _PAD4:4;
	u32 F:8;
	u32 _PAD5:20;
REG_END

REG128_(GIFPacked, A_D)
	u64 DATA;
	u8 ADDR:8; // enum GIF_A_D_REG
	u8 _PAD1[3+4];
REG_END

REG128_(GIFPacked, NOP)
	u32 _PAD1;
	u32 _PAD2;
	u32 _PAD3;
	u32 _PAD4;
REG_END

REG128_SET(GIFPackedReg)
	GIFReg			r;
	GIFPackedPRIM	PRIM;
	GIFPackedRGBA	RGBA;
	GIFPackedSTQ	STQ;
	GIFPackedUV		UV;
	GIFPackedXYZF2	XYZF2;
	GIFPackedXYZ2	XYZ2;
	GIFPackedFOG	FOG;
	GIFPackedA_D	A_D;
	GIFPackedNOP	NOP;
REG_SET_END

struct alignas(32) GIFPath
{
	GIFTag tag;
	u32 nloop;
	u32 nreg;
	u32 reg;
	u32 type;
	GSVector4i regs;

	enum {TYPE_UNKNOWN, TYPE_ADONLY, TYPE_STQRGBAXYZF2, TYPE_STQRGBAXYZ2};

	GS_FORCEINLINE void SetTag(const void* mem)
	{
		const GIFTag* RESTRICT src = (const GIFTag*)mem;

		// the compiler has a hard time not reloading every time a field of src is accessed

		u32 a = src->U32[0];
		u32 b = src->U32[1];

		tag.U32[0] = a;
		tag.U32[1] = b;

		nloop = a & 0x7fff;

		if(nloop == 0) return;

		GSVector4i v = GSVector4i::loadl(&src->REGS); // REGS not stored to tag.REGS, only into this->regs, restored before saving the state though

		nreg = (b & 0xf0000000) ? (b >> 28) : 16; // src->NREG
		regs = v.upl8(v >> 4) & GSVector4i::x0f(nreg);
		reg  = 0;

		type = TYPE_UNKNOWN;

		if(tag.FLG == GIF_FLG_PACKED)
		{
			if(regs.eq8(GSVector4i(0x0e0e0e0e)).mask() == (1 << nreg) - 1)
			{
				type = TYPE_ADONLY;
			}
			else
			{
				switch(nreg)
				{
					case 3:
						if(regs.U32[0] == 0x00040102) type = TYPE_STQRGBAXYZF2; // many games, TODO: formats mixed with NOPs (xeno2: 040f010f02, 04010f020f, mgs3: 04010f0f02, 0401020f0f, 04010f020f)
						if(regs.U32[0] == 0x00050102) type = TYPE_STQRGBAXYZ2; // GoW (has other crazy formats, like ...030503050103)
						// TODO: common types with UV instead
						break;
					case 9:
						if(regs.U32[0] == 0x02040102 && regs.U32[1] == 0x01020401 && regs.U32[2] == 0x00000004) {type = TYPE_STQRGBAXYZF2; nreg = 3; nloop *= 3;} // FFX
						break;
					case 12:
						if(regs.U32[0] == 0x02040102 && regs.U32[1] == 0x01020401 && regs.U32[2] == 0x04010204) {type = TYPE_STQRGBAXYZF2; nreg = 3; nloop *= 4;} // DQ8 (not many, mostly 040102)
						break;
					case 1:
					case 2:
					case 4:
					case 5:
					case 6:
					case 7:
					case 8:
					case 10:
					case 11:
					case 13:
					case 14:
					case 15:
					case 16:
					default:
						break;
				}
			}
		}
	}

	GS_FORCEINLINE bool StepReg()
	{
		if(++reg == nreg)
		{
			reg = 0;

			if(--nloop == 0)
				return false;
		}

		return true;
	}
};

struct GSPrivRegSet
{
	union
	{
		struct
		{
			GSRegPMODE		PMODE;
			u64			_pad1;
			GSRegSMODE1		SMODE1;
			u64			_pad2;
			GSRegSMODE2		SMODE2;
			u64			_pad3;
			GSRegSRFSH		SRFSH;
			u64			_pad4;
			GSRegSYNCH1		SYNCH1;
			u64			_pad5;
			GSRegSYNCH2		SYNCH2;
			u64			_pad6;
			GSRegSYNCV		SYNCV;
			u64			_pad7;
			struct {
				GSRegDISPFB		DISPFB;
				u64			_pad1;
				GSRegDISPLAY	DISPLAY;
				u64			_pad2;
			} DISP[2];
			GSRegEXTBUF		EXTBUF;
			u64			_pad8;
			GSRegEXTDATA	EXTDATA;
			u64			_pad9;
			GSRegEXTWRITE	EXTWRITE;
			u64			_pad10;
			GSRegBGCOLOR	BGCOLOR;
			u64			_pad11;
		};

		u8 _pad12[0x1000];
	};

	union
	{
		struct
		{
			GSRegCSR		CSR;
			u64			_pad13;
			GSRegIMR		IMR;
			u64			_pad14;
			u64			_unk1[4];
			GSRegBUSDIR		BUSDIR;
			u64			_pad15;
			u64			_unk2[6];
			GSRegSIGLBLID	SIGLBLID;
			u64			_pad16;
		};

		u8 _pad17[0x1000];
	};
};

#pragma pack(pop)

#define FREEZE_LOAD 0
#define FREEZE_SAVE 1
#define FREEZE_SIZE 2

struct GSFreezeData
{
	int size;
	u8* data;
};

enum class GSVideoMode : u8
{
	Unknown,
	NTSC,
	PAL,
	VESA,
	SDTV_480P,
	HDTV_720P,
	HDTV_1080I
};

// Ordering was done to keep compatibility with older ini file.
enum class BiFiltering : u8
{
	Nearest,
	Forced,
	PS2,
	Forced_But_Sprite,
};

enum class TriFiltering : u8
{
	None,
	PS2,
	Forced,
};

enum class HWMipmapLevel : int
{
	Automatic = -1,
	Off,
	Basic,
	Full
};

enum class CRCHackLevel : s8
{
	Automatic = -1,
	None,
	Minimum,
	Partial,
	Full,
	Aggressive
};

#ifdef ENABLE_ACCURATE_BUFFER_EMULATION
const GSVector2i default_rt_size(2048, 2048);
#else
const GSVector2i default_rt_size(1280, 1024);
#endif

class GSApp
{
	std::map< std::string, std::string > m_current_configuration;

public:
	GSApp();

	void Init();

	// Avoid issue with overloading
	int    GetConfigI(const char* entry);
	bool   GetConfigB(const char* entry);
};

GSRendererType GetCurrentRendererType(void);

GSVector4i GSClientRect(void);

extern GSApp theApp;
