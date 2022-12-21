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

#pragma once

enum tag_id
{
	TAG_CNTS = 0,
	TAG_REFE = 0, 	// Transfer Packet According to ADDR field, clear STR, and end
	TAG_CNT, 		// Transfer QWC following the tag.
	TAG_NEXT,		// Transfer QWC following tag. TADR = ADDR
	TAG_REF,			// Transfer QWC from ADDR field
	TAG_REFS,		// Transfer QWC from ADDR field (Stall Control)
	TAG_CALL,		// Transfer QWC following the tag, save succeeding tag
	TAG_RET,			// Transfer QWC following the tag, load next tag
	TAG_END			// Transfer QWC following the tag
};

enum mfd_type
{
	NO_MFD = 0,
	MFD_RESERVED,
	MFD_VIF1,
	MFD_GIF
};

enum sts_type
{
	NO_STS = 0,
	STS_SIF0,
	STS_fromSPR,
	STS_fromIPU
};

enum std_type
{
	NO_STD = 0,
	STD_VIF1,
	STD_GIF,
	STD_SIF1
};

enum LogicalTransferMode
{
	NORMAL_MODE = 0,
	CHAIN_MODE,
	INTERLEAVE_MODE,
	UNDEFINED_MODE
};

//
// --- DMA ---
//

// Doing double duty as both the top 32 bits *and* the lower 32 bits of a chain tag.
// Theoretically should probably both be in a u64 together, but with the way the
// code is layed out, this is easier for the moment.

union tDMA_TAG {
	struct {
		u32 QWC : 16;
		u32 _reserved2 : 10;
		u32 PCE : 2;
		u32 ID : 3;
		u32 IRQ : 1;
	};
	struct {
		u32 ADDR : 31;
		u32 SPR : 1;
	};
	u32 _u32;

	tDMA_TAG() {}

	tDMA_TAG(u32 val) { _u32 = val; }
};

#define DMA_TAG(value) ((tDMA_TAG)(value))

union tDMA_CHCR {
	struct {
		u32 DIR : 1;        // Direction: 0 - to memory, 1 - from memory. VIF1 & SIF2 only.
		u32 _reserved1 : 1;
		u32 MOD : 2;		// Logical transfer mode. Normal, Chain, or Interleave (see LogicalTransferMode enum)
		u32 ASP : 2;        // ASP1 & ASP2; Address stack pointer. 0, 1, or 2 addresses.
		u32 TTE : 1;        // Tag Transfer Enable. 0 - Disable / 1 - Enable.
		u32 TIE : 1;        // Tag Interrupt Enable. 0 - Disable / 1 - Enable.
		u32 STR : 1;        // Start. 0 while stopping DMA, 1 while it's running.
		u32 _reserved2 : 7;
		u32 TAG : 16;		// Maintains upper 16 bits of the most recently read DMAtag.
	};
	u32 _u32;

	tDMA_CHCR( u32 val) { _u32 = val; }

	void set(u32 value) { _u32 = value; }
	tDMA_TAG tag() { return (tDMA_TAG)_u32; }
};

union tDMA_SADR {
	struct {
		u32 ADDR : 14;
		u32 reserved2 : 18;
	};
	u32 _u32;

	tDMA_SADR(u32 val) { _u32 = val; }

	tDMA_TAG tag() const { return (tDMA_TAG)_u32; }
};

union tDMA_QWC {
	struct {
		u32 QWC;
	};
	u32 _u32;

	tDMA_QWC(u32 val) { _u32 = val; }

	tDMA_TAG tag() const { return (tDMA_TAG)_u32; }
};

struct DMACh {
	tDMA_CHCR chcr;
	u32 _null0[3];
	u32 madr;
	u32 _null1[3];
	u32 qwc;
	u32 _null2[3];
	u32 tadr;
	u32 _null3[3];
	u32 asr0;
	u32 _null4[3];
	u32 asr1;
	u32 _null5[11];
	u32 sadr;

	void chcrTransfer(tDMA_TAG* ptag)
	{
	    chcr.TAG = ptag[0]._u32 >> 16;
	}

	void qwcTransfer(tDMA_TAG* ptag)
	{
	    qwc = ptag[0].QWC;
	}

	bool transfer(const char *s, tDMA_TAG* ptag);
	void unsafeTransfer(tDMA_TAG* ptag);
	tDMA_TAG *getAddr(u32 addr, u32 num, bool write);
	tDMA_TAG *DMAtransfer(u32 addr, u32 num);
};

enum INTCIrqs
{
	INTC_GS = 0,
	INTC_SBUS,
	INTC_VBLANK_S,
	INTC_VBLANK_E,
	INTC_VIF0,
	INTC_VIF1,
	INTC_VU0,
	INTC_VU1,
	INTC_IPU,
	INTC_TIM0,
	INTC_TIM1,
	INTC_TIM2,
	INTC_TIM3,
	INTC_SFIFO,
	INTVU0_WD
};

enum dmac_conditions
{
	DMAC_STAT_SIS	= (1<<13),	 // stall condition
	DMAC_STAT_MEIS	= (1<<14),	 // mfifo empty
	DMAC_STAT_BEIS	= (1<<15),	 // bus error
	DMAC_STAT_SIM	= (1<<29),	 // stall mask
	DMAC_STAT_MEIM	= (1<<30)	 // mfifo mask
};

//DMA interrupts & masks
enum DMAInter
{
	BEISintr = 0x00008000,
	VIF0intr = 0x00010001,
	VIF1intr = 0x00020002,
	GIFintr =  0x00040004,
	IPU0intr = 0x00080008,
	IPU1intr = 0x00100010,
	SIF0intr = 0x00200020,
	SIF1intr = 0x00400040,
	SIF2intr = 0x00800080,
	SPR0intr = 0x01000100,
	SPR1intr = 0x02000200,
	SISintr  = 0x20002000,
	MEISintr = 0x40004000
};

union tDMAC_QUEUE
{
	struct
	{
	    u16 VIF0 : 1;
	    u16 VIF1 : 1;
	    u16 GIF  : 1;
	    u16 IPU0 : 1;
	    u16 IPU1 : 1;
	    u16 SIF0 : 1;
	    u16 SIF1 : 1;
	    u16 SIF2 : 1;
	    u16 SPR0 : 1;
	    u16 SPR1 : 1;
	    u16 SIS  : 1;
	    u16 MEIS : 1;
	    u16 BEIS : 1;
	};
	u16 _u16;
};

union tDMAC_CTRL {
	struct {
		u32 DMAE : 1;       // 0/1 - disables/enables all DMAs
		u32 RELE : 1;       // 0/1 - cycle stealing off/on
		u32 MFD : 2;        // Memory FIFO drain channel (mfd_type)
		u32 STS : 2;        // Stall Control source channel (sts type)
		u32 STD : 2;        // Stall Control drain channel (std_type)
		u32 RCYC : 3;       // Release cycle (8/16/32/64/128/256)
		u32 _reserved1 : 21;
	};
	u32 _u32;

	tDMAC_CTRL(u32 val) { _u32 = val; }
};

union tDMAC_STAT {
	struct {
		u32 CIS : 10;
		u32 _reserved1 : 3;
		u32 SIS : 1;
		u32 MEIS : 1;
		u32 BEIS : 1;
		u32 CIM : 10;
		u32 _reserved2 : 3;
		u32 SIM : 1;
		u32 MEIM : 1;
		u32 _reserved3 : 1;
	};
	u32 _u32;
	u16 _u16[2];

	tDMAC_STAT(u32 val) { _u32 = val; }
};

union tDMAC_PCR {
	struct {
		u32 CPC : 10;
		u32 _reserved1 : 6;
		u32 CDE : 10;
		u32 _reserved2 : 5;
		u32 PCE : 1;
	};
	u32 _u32;

	tDMAC_PCR(u32 val) { _u32 = val; }
};

union tDMAC_SQWC {
	struct {
		u32 SQWC : 8;
		u32 _reserved1 : 8;
		u32 TQWC : 8;
		u32 _reserved2 : 8;
	};
	u32 _u32;

	tDMAC_SQWC(u32 val) { _u32 = val; }
};

union tDMAC_RBSR {
	struct {
		u32 RMSK : 31;
		u32 _reserved1 : 1;
	};
	u32 _u32;

	tDMAC_RBSR(u32 val) { _u32 = val; }
};

union tDMAC_RBOR {
	struct {
		u32 ADDR : 31;
		u32 _reserved1 : 1;
	};
	u32 _u32;

	tDMAC_RBOR(u32 val) { _u32 = val; }
};

// --------------------------------------------------------------------------------------
//  tDMAC_ADDR
// --------------------------------------------------------------------------------------
// This struct is used for several DMA address types, including some that do not have
// effective SPR bit (the bit is ignored for all addresses that are not "allowed" to access
// the scratchpad, including STADR, toSPR.MADR, fromSPR.MADR, etc.).
//
union tDMAC_ADDR
{
	struct {
		u32 ADDR : 31;	// Transfer memory address
		u32 SPR : 1;	// Memory/SPR Address (only effective for MADR and TADR of non-SPR DMAs)
	};
	u32 _u32;

	tDMAC_ADDR() {}
	tDMAC_ADDR(u32 val) { _u32 = val; }
};

struct DMACregisters
{
	tDMAC_CTRL	ctrl;
	u32 _padding[3];
	tDMAC_STAT	stat;
	u32 _padding1[3];
	tDMAC_PCR	pcr;
	u32 _padding2[3];

	tDMAC_SQWC	sqwc;
	u32 _padding3[3];
	tDMAC_RBSR	rbsr;
	u32 _padding4[3];
	tDMAC_RBOR	rbor;
	u32 _padding5[3];
	tDMAC_ADDR	stadr;
	u32 _padding6[3];
};

// Currently guesswork.
union tINTC_STAT {
	struct {
		u32 interrupts : 10;
	    u32 _placeholder : 22;
	};
	u32 _u32;

	tINTC_STAT(u32 val) { _u32 = val; }
};

union tINTC_MASK {
	struct {
	    u32 int_mask : 10;
	    u32 _placeholder:22;
	};
	u32 _u32;

	tINTC_MASK(u32 val) { _u32 = val; }
};

struct INTCregisters
{
	tINTC_STAT  stat;
	u32 _padding1[3];
	tINTC_MASK  mask;
	u32 _padding2[3];
};

static DMACregisters& dmacRegs	= (DMACregisters&)eeHw[0xE000];

// Various useful locations
static DMACh& vif0ch	= (DMACh&)eeHw[0x8000];
static DMACh& vif1ch	= (DMACh&)eeHw[0x9000];
static DMACh& gifch		= (DMACh&)eeHw[0xA000];
static DMACh& spr0ch	= (DMACh&)eeHw[0xD000];
static DMACh& spr1ch	= (DMACh&)eeHw[0xD400];
static DMACh& ipu0ch	= (DMACh&)eeHw[0xb000];
static DMACh& ipu1ch	= (DMACh&)eeHw[0xb400];
static DMACh& sif0ch	= (DMACh&)eeHw[0xc000];
static DMACh& sif1ch	= (DMACh&)eeHw[0xc400];
static DMACh& sif2dma	= (DMACh&)eeHw[0xc800];

extern tDMA_TAG *SPRdmaGetAddr(u32 addr, bool write);
extern tDMA_TAG *dmaGetAddr(u32 addr, bool write);

extern void hwIntcIrq(int n);
extern void hwDmacIrq(int n);

extern void FireMFIFOEmpty();
extern bool hwMFIFOWrite(u32 addr, const u128* data, uint size_qwc);
extern void hwMFIFOResume(u32 transferred);
extern void hwDmacSrcTadrInc(DMACh& dma);
extern bool hwDmacSrcChainWithStack(DMACh& dma, int id);
extern bool hwDmacSrcChain(DMACh& dma, int id);

template< uint page > u32 dmacRead32( u32 mem );
template< uint page > extern bool dmacWrite32( u32 mem, mem32_t& value );
