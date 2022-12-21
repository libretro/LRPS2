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

#include "PrecompiledHeader.h"
#include "Common.h"

#include "IPU.h"
#include "IPUdma.h"
#include "yuv2rgb.h"
#include "mpeg2lib/Mpeg.h"

#include "Vif.h"
#include "Gif.h"
#include "Vif_Dma.h"
#include <limits.h>
#include "AppConfig.h"

#include "Utilities/MemsetFast.inl"

#define ipumsk( src ) ( (src) & 0xff )

// the BP doesn't advance and returns -1 if there is no data to be read
__aligned16 tIPU_cmd ipu_cmd;
__aligned16 tIPU_BP g_BP;
__aligned16 decoder_t decoder;

// Quantization matrix
static rgb16_t vqclut[16];			//clut conversion table
static u8 s_thresh[2];				//thresholds for color conversions
int coded_block_pattern = 0;

alignas(16) static u8 indx4[16*16/2];

bool FMVstarted = false;
bool EnableFMV = false;

void tIPU_cmd::clear(void)
{
	memzero_sse_a(*this);
	current = 0xffffffff;
}

/////////////////////////////////////////////////////////
// Register accesses (run on EE thread)

void ipuReset(void)
{
	memzero(ipuRegs);
	memzero(g_BP);
	memzero(decoder);

	decoder.picture_structure = FRAME_PICTURE;      //default: progressive...my guess:P

	ipu_fifo.init();
	ipu_cmd.clear();
	ipuDmaReset();
}

void SaveStateBase::ipuFreeze()
{
	// Get a report of the status of the ipu variables when saving and loading savestates.
	FreezeTag("IPU");
	Freeze(ipu_fifo);

	Freeze(g_BP);
	Freeze(vqclut);
	Freeze(s_thresh);
	Freeze(coded_block_pattern);
	Freeze(decoder);
	Freeze(ipu_cmd);
}

__fi u32 ipuRead32(u32 mem)
{
	mem &= 0xff;	// IPU repeats every 0x100

	IPUProcessInterrupt();

	switch (mem)
	{
		case ipumsk(IPU_CMD) : // IPU_CMD
		{
			if (ipu_cmd.CMD != SCE_IPU_FDEC && ipu_cmd.CMD != SCE_IPU_VDEC)
			{
				if (getBits32((u8*)&ipuRegs.cmd.DATA, 0))
					ipuRegs.cmd.DATA = BigEndian(ipuRegs.cmd.DATA);
			}
			return ipuRegs.cmd.DATA;
		}

		case ipumsk(IPU_CTRL): // IPU_CTRL
		{
			ipuRegs.ctrl.IFC = g_BP.IFC;
			ipuRegs.ctrl.CBP = coded_block_pattern;
			return ipuRegs.ctrl._u32;
		}

		case ipumsk(IPU_BP): // IPU_BP
		{
			ipuRegs.ipubp  = g_BP.BP & 0x7f;
			ipuRegs.ipubp |= g_BP.IFC << 8;
			ipuRegs.ipubp |= g_BP.FP << 16;
			return ipuRegs.ipubp;
		}

		default:
		break;
	}

	return psHu32(IPU_CMD + mem);
}

__fi u64 ipuRead64(u32 mem)
{
	mem &= 0xff;	// IPU repeats every 0x100

	IPUProcessInterrupt();

	switch (mem)
	{
		case ipumsk(IPU_CMD): // IPU_CMD
		{
			if (ipu_cmd.CMD != SCE_IPU_FDEC && ipu_cmd.CMD != SCE_IPU_VDEC)
			{
				if (getBits32((u8*)&ipuRegs.cmd.DATA, 0))
					ipuRegs.cmd.DATA = BigEndian(ipuRegs.cmd.DATA);
			}

			return ipuRegs.cmd._u64;
		}

		case ipumsk(IPU_CTRL):
		case ipumsk(IPU_BP):
		case ipumsk(IPU_TOP): // IPU_TOP
		default:
			break;
	}
	return psHu64(IPU_CMD + mem);
}

static void ipuSoftReset(void)
{
	if (ipu1ch.chcr.STR && g_BP.IFC < 8 && IPU1Status.DataRequested)
		ipu1Interrupt();

	if (!ipu1ch.chcr.STR)
		psHu32(DMAC_STAT) &= ~(1 << DMAC_TO_IPU);

	ipu_fifo.clear();
	memzero(g_BP);

	coded_block_pattern = 0;

	ipuRegs.ctrl._u32 &= 0x7F33F00;
	ipuRegs.top = 0;
	ipu_cmd.clear();
	ipuRegs.cmd.BUSY = 0;
	ipuRegs.cmd.DATA = 0; // required for Enthusia - Professional Racing after fix, or will freeze at start of next video.

	hwIntcIrq(INTC_IPU); // required for FightBox
}

//////////////////////////////////////////////////////
// IPU Commands (exec on worker thread only)
static void ipuBCLR(u32 val)
{
	if (ipu1ch.chcr.STR && g_BP.IFC < 8 && IPU1Status.DataRequested)
		ipu1Interrupt();

	if(!ipu1ch.chcr.STR)
		psHu32(DMAC_STAT) &= ~(1 << DMAC_TO_IPU);

	ipu_fifo.in.clear();

	memzero(g_BP);
	g_BP.BP = val & 0x7F;

	ipuRegs.cmd.BUSY = 0;
}

//////////////////////////////////////////////////////
// IPU Commands (exec on worker thread only)
static __ri void ipuIDEC(tIPU_CMD_IDEC idec)
{
	//from IPU_CTRL
	ipuRegs.ctrl.PCT 		= I_TYPE; //Intra DECoding;)

	decoder.coding_type		= ipuRegs.ctrl.PCT;
	decoder.mpeg1			= ipuRegs.ctrl.MP1;
	decoder.q_scale_type		= ipuRegs.ctrl.QST;
	decoder.intra_vlc_format	= ipuRegs.ctrl.IVF;
	decoder.scantype		= ipuRegs.ctrl.AS;
	decoder.intra_dc_precision	= ipuRegs.ctrl.IDP;

	//from IDEC value
	decoder.quantizer_scale		= idec.QSC;
	decoder.frame_pred_frame_dct    = !idec.DTD;
	decoder.sgn 			= idec.SGN;
	decoder.dte 			= idec.DTE;
	decoder.ofm 			= idec.OFM;

	//other stuff
	decoder.dcr 			= 1; // resets DC prediction value
}

static __ri void ipuBDEC(tIPU_CMD_BDEC bdec)
{
	decoder.coding_type		= I_TYPE;
	decoder.mpeg1			= ipuRegs.ctrl.MP1;
	decoder.q_scale_type		= ipuRegs.ctrl.QST;
	decoder.intra_vlc_format	= ipuRegs.ctrl.IVF;
	decoder.scantype		= ipuRegs.ctrl.AS;
	decoder.intra_dc_precision	= ipuRegs.ctrl.IDP;

	//from BDEC value
	decoder.quantizer_scale		= decoder.q_scale_type ? non_linear_quantizer_scale [bdec.QSC] : bdec.QSC << 1;
	decoder.macroblock_modes	= bdec.DT ? DCT_TYPE_INTERLACED : 0;
	decoder.dcr			= bdec.DCR;
	decoder.macroblock_modes       |= bdec.MBI ? MACROBLOCK_INTRA : MACROBLOCK_PATTERN;

	memzero_sse_a(decoder.mb8);
	memzero_sse_a(decoder.mb16);
}


// When a command is written, we set some various busy flags and clear some other junk.
// The actual decoding will be handled by IPUworker.
static __fi void IPUCMD_WRITE(u32 val)
{
	ipuRegs.ctrl.ECD = 0;
	ipuRegs.ctrl.SCD = 0;
	ipu_cmd.clear();
	ipu_cmd.current = val;

	switch (ipu_cmd.CMD)
	{
		// BCLR and SETTH  require no data so they always execute inline:

		case SCE_IPU_BCLR:
			ipuBCLR(val);
			hwIntcIrq(INTC_IPU); //DMAC_TO_IPU
			ipuRegs.ctrl.BUSY = 0;
			return;

		case SCE_IPU_SETTH:
			s_thresh[0] = (val & 0x1ff);
			s_thresh[1] = ((val >> 16) & 0x1ff);
			hwIntcIrq(INTC_IPU);
			ipuRegs.ctrl.BUSY = 0;
			return;


		case SCE_IPU_IDEC:
			g_BP.Advance(val & 0x3F);
			ipuIDEC(val);
			ipuRegs.SetTopBusy();
			break;

		case SCE_IPU_BDEC:
			g_BP.Advance(val & 0x3F);
			ipuBDEC(val);
			ipuRegs.SetTopBusy();
			break;

		case SCE_IPU_VDEC:
			g_BP.Advance(val & 0x3F);
			ipuRegs.SetDataBusy();
			break;

		case SCE_IPU_FDEC:
			g_BP.Advance(val & 0x3F);
			ipuRegs.SetDataBusy();
			break;

		case SCE_IPU_SETIQ:
			g_BP.Advance(val & 0x3F);
			break;

		case SCE_IPU_SETVQ:
		case SCE_IPU_CSC:
		case SCE_IPU_PACK:
		default:
			break;
	}

	ipuRegs.ctrl.BUSY = 1;
}


__fi bool ipuWrite32(u32 mem, u32 value)
{
	mem &= 0xfff;

	switch (mem)
	{
		case ipumsk(IPU_CMD): // IPU_CMD
			IPUCMD_WRITE(value);
			IPUProcessInterrupt();
			return false;

		case ipumsk(IPU_CTRL): // IPU_CTRL
            // CTRL = the first 16 bits of ctrl [0x8000ffff], + value for the next 16 bits,
            // minus the reserved bits. (18-19; 27-29) [0x47f30000]
			ipuRegs.ctrl.write(value);
			if (ipuRegs.ctrl.IDP == 3)
				ipuRegs.ctrl.IDP = 1;

			if (ipuRegs.ctrl.RST)
				ipuSoftReset(); // RESET
			return false;
	}
	return true;
}

// returns FALSE when the writeback is handled, TRUE if the caller should do the
// writeback itself.
__fi bool ipuWrite64(u32 mem, u64 value)
{
	mem &= 0xfff;

	switch (mem)
	{
		case ipumsk(IPU_CMD):
			IPUCMD_WRITE((u32)value);
			IPUProcessInterrupt();
			return false;
	}

	return true;
}

// --------------------------------------------------------------------------------------
//  Buffer reader
// --------------------------------------------------------------------------------------
// whenever reading fractions of bytes. The low bits always come from the next byte
// while the high bits come from the current byte
static u8 getBits64(u8 *address, bool advance)
{
	if (!g_BP.FillBuffer(64)) return 0;

	const u8* readpos = &g_BP.internal_qwc[0]._u8[g_BP.BP/8];

	if (uint shift = (g_BP.BP & 7))
	{
		u64 mask = (0xff >> shift);
		mask = mask | (mask << 8) | (mask << 16) | (mask << 24) | (mask << 32) | (mask << 40) | (mask << 48) | (mask << 56);

		*(u64*)address = ((~mask & *(u64*)(readpos + 1)) >> (8 - shift)) | (((mask) & *(u64*)readpos) << shift);
	}
	else
		*(u64*)address = *(u64*)readpos;

	if (advance) g_BP.Advance(64);

	return 1;
}

static __ri bool ipuFDEC(u32 val)
{
	if (!getBits32((u8*)&ipuRegs.cmd.DATA, 0)) return false;

	ipuRegs.cmd.DATA = BigEndian(ipuRegs.cmd.DATA);
	ipuRegs.top = ipuRegs.cmd.DATA;

	return true;
}

static bool ipuSETIQ(u32 val)
{
	if ((val >> 27) & 1)
	{
		u8 (&niq)[64] = decoder.niq;

		for(;ipu_cmd.pos[0] < 8; ipu_cmd.pos[0]++)
		{
			if (!getBits64((u8*)niq + 8 * ipu_cmd.pos[0], 1)) return false;
		}
	}
	else
	{
		u8 (&iq)[64] = decoder.iq;

		for(;ipu_cmd.pos[0] < 8; ipu_cmd.pos[0]++)
		{
			if (!getBits64((u8*)iq + 8 * ipu_cmd.pos[0], 1)) return false;
		}
	}

	return true;
}

static bool ipuSETVQ(u32 val)
{
	for(;ipu_cmd.pos[0] < 4; ipu_cmd.pos[0]++)
	{
		if (!getBits64(((u8*)vqclut) + 8 * ipu_cmd.pos[0], 1)) return false;
	}

	return true;
}

// IPU Transfers are split into 8Qwords so we need to send ALL the data
static __ri bool ipuCSC(tIPU_CMD_CSC csc)
{
	for (;ipu_cmd.index < (int)csc.MBC; ipu_cmd.index++)
	{
		for(;ipu_cmd.pos[0] < 48; ipu_cmd.pos[0]++)
		{
			if (!getBits64((u8*)&decoder.mb8 + 8 * ipu_cmd.pos[0], 1)) return false;
		}

		ipu_csc(decoder.mb8, decoder.rgb32, 0);
		
		if (csc.OFM)
		{
			ipu_dither(decoder.rgb32, decoder.rgb16, csc.DTE);
			ipu_cmd.pos[1] += ipu_fifo.out.write(((u32*) & decoder.rgb16) + 4 * ipu_cmd.pos[1], 32 - ipu_cmd.pos[1]);
			if (ipu_cmd.pos[1] < 32) return false;
		}
		else
		{
			ipu_cmd.pos[1] += ipu_fifo.out.write(((u32*) & decoder.rgb32) + 4 * ipu_cmd.pos[1], 64 - ipu_cmd.pos[1]);
			if (ipu_cmd.pos[1] < 64) return false;
		}

		ipu_cmd.pos[0] = 0;
		ipu_cmd.pos[1] = 0;
	}

	return true;
}

static __fi void ipu_vq(macroblock_rgb16& rgb16, u8* indx4)
{
	const auto closest_index = [&](int i, int j) {
		u8 index = 0;
		int min_distance = std::numeric_limits<int>::max();
		for (u8 k = 0; k < 16; ++k)
		{
			const int dr = rgb16.c[i][j].r - vqclut[k].r;
			const int dg = rgb16.c[i][j].g - vqclut[k].g;
			const int db = rgb16.c[i][j].b - vqclut[k].b;
			const int distance = dr * dr + dg * dg + db * db;

			// XXX: If two distances are the same which index is used?
			if (min_distance > distance)
			{
				index = k;
				min_distance = distance;
			}
		}

		return index;
	};

	for (int i = 0; i < 16; ++i)
		for (int j = 0; j < 8; ++j)
			indx4[i * 8 + j] = closest_index(i, 2 * j + 1) << 4 | closest_index(i, 2 * j);
}

static __ri bool ipuPACK(tIPU_CMD_CSC csc)
{
	for (;ipu_cmd.index < (int)csc.MBC; ipu_cmd.index++)
	{
		for(;ipu_cmd.pos[0] < (int)sizeof(macroblock_rgb32) / 8; ipu_cmd.pos[0]++)
		{
			if (!getBits64((u8*)&decoder.rgb32 + 8 * ipu_cmd.pos[0], 1)) return false;
		}

		ipu_dither(decoder.rgb32, decoder.rgb16, csc.DTE);

		if (csc.OFM)
		{
			ipu_cmd.pos[1] += ipu_fifo.out.write(((u32*) & decoder.rgb16) + 4 * ipu_cmd.pos[1], 32 - ipu_cmd.pos[1]);
			if (ipu_cmd.pos[1] < 32) return false;
		}
		else
		{
			ipu_vq(decoder.rgb16, indx4);
			ipu_cmd.pos[1] += ipu_fifo.out.write(((u32*)indx4) + 4 * ipu_cmd.pos[1], 8 - ipu_cmd.pos[1]);
			if (ipu_cmd.pos[1] < 8) return false;
		}

		ipu_cmd.pos[0] = 0;
		ipu_cmd.pos[1] = 0;
	}

	return TRUE;
}

// --------------------------------------------------------------------------------------
//  CORE Functions (referenced from MPEG library)
// --------------------------------------------------------------------------------------
__fi void ipu_csc(macroblock_8& mb8, macroblock_rgb32& rgb32, int sgn)
{
	int i;
	u8* p = (u8*)&rgb32;

	yuv2rgb();

	if (s_thresh[0] > 0)
	{
		for (i = 0; i < 16*16; i++, p += 4)
		{
			if ((p[0] < s_thresh[0]) && (p[1] < s_thresh[0]) && (p[2] < s_thresh[0]))
				*(u32*)p = 0;
			else if ((p[0] < s_thresh[1]) && (p[1] < s_thresh[1]) && (p[2] < s_thresh[1]))
				p[3] = 0x40;
		}
	}
	else if (s_thresh[1] > 0)
	{
		for (i = 0; i < 16*16; i++, p += 4)
		{
			if ((p[0] < s_thresh[1]) && (p[1] < s_thresh[1]) && (p[2] < s_thresh[1]))
				p[3] = 0x40;
		}
	}
	if (sgn)
	{
		for (i = 0; i < 16*16; i++, p += 4)
			*(u32*)p ^= 0x808080;
	}
}

// --------------------------------------------------------------------------------------
//  Buffer reader
// --------------------------------------------------------------------------------------

// whenever reading fractions of bytes. The low bits always come from the next byte
// while the high bits come from the current byte
__fi u8 getBits32(u8 *address, bool advance)
{
	if (!g_BP.FillBuffer(32)) return 0;

	const u8* readpos = &g_BP.internal_qwc->_u8[g_BP.BP/8];
	
	if(uint shift = (g_BP.BP & 7))
	{
		u32 mask = (0xff >> shift);
		mask = mask | (mask << 8) | (mask << 16) | (mask << 24);

		*(u32*)address = ((~mask & *(u32*)(readpos + 1)) >> (8 - shift)) | (((mask) & *(u32*)readpos) << shift);
	}
	else
	{
		// Bit position-aligned -- no masking/shifting necessary
		*(u32*)address = *(u32*)readpos;
	}

	if (advance) g_BP.Advance(32);

	return 1;
}

static __fi bool ipuVDEC(u32 val)
{
	if (EmuConfig.Gamefixes.FMVinSoftwareHack)
	{
		static int count = 0;
		if (count++ > 5)
		{
			if (!FMVstarted)
			{
				EnableFMV = true;
				FMVstarted = true;
			}
			count = 0;
		}
	}
	switch (ipu_cmd.pos[0])
	{
		case 0:
			if (!bitstream_init()) return false;

			switch ((val >> 26) & 3)
			{
				case 0://Macroblock Address Increment
					decoder.mpeg1 = ipuRegs.ctrl.MP1;
					ipuRegs.cmd.DATA = get_macroblock_address_increment();
					break;

				case 1://Macroblock Type
					decoder.frame_pred_frame_dct = 1;
					decoder.coding_type = ipuRegs.ctrl.PCT > 0 ? ipuRegs.ctrl.PCT : 1; // Kaiketsu Zorro Mezase doesn't set a Picture type, seems happy with I
					ipuRegs.cmd.DATA = get_macroblock_modes();
					break;

				case 2://Motion Code
					ipuRegs.cmd.DATA = get_motion_delta(0);
					break;

				case 3://DMVector
					ipuRegs.cmd.DATA = get_dmv();
					break;

				default:
					break;
			}

			// HACK ATTACK!  This code OR's the MPEG decoder's bitstream position into the upper
			// 16 bits of DATA; which really doesn't make sense since (a) we already rewound the bits
			// back into the IPU internal buffer above, and (b) the IPU doesn't have an MPEG internal
			// 32-bit decoder buffer of its own anyway.  Furthermore, setting the upper 16 bits to
			// any value other than zero appears to work fine.  When set to zero, however, FMVs run
			// very choppy (basically only decoding/updating every 30th frame or so). So yeah,
			// someone with knowledge on the subject please feel free to explain this one. :) --air

			// The upper bits are the "length" of the decoded command, where the lower is the address.
			// This is due to differences with IPU and the MPEG standard. See get_macroblock_address_increment().

			ipuRegs.ctrl.ECD = (ipuRegs.cmd.DATA == 0);

		case 1:
			if (!getBits32((u8*)&ipuRegs.top, 0))
			{
				ipu_cmd.pos[0] = 1;
				return false;
			}

			ipuRegs.top = BigEndian(ipuRegs.top);
			return true;

		default:
			break;
	}

	return false;
}

// --------------------------------------------------------------------------------------
//  IPU Worker / Dispatcher
// --------------------------------------------------------------------------------------
__fi void IPUProcessInterrupt(void)
{
	if (ipuRegs.ctrl.BUSY)
	{
		switch (ipu_cmd.CMD)
		{
			case SCE_IPU_IDEC:
				if (!mpeg2sliceIDEC()) return;

				ipuRegs.topbusy = 0;
				ipuRegs.cmd.BUSY = 0;

				break;

			case SCE_IPU_BDEC:
				if (!mpeg2_slice()) return;

				ipuRegs.topbusy = 0;
				ipuRegs.cmd.BUSY = 0;

				break;

			case SCE_IPU_VDEC:
				if (!ipuVDEC(ipu_cmd.current)) return;

				ipuRegs.topbusy = 0;
				ipuRegs.cmd.BUSY = 0;
				break;

			case SCE_IPU_FDEC:
				if (!ipuFDEC(ipu_cmd.current)) return;

				ipuRegs.topbusy = 0;
				ipuRegs.cmd.BUSY = 0;
				break;

			case SCE_IPU_SETIQ:
				if (!ipuSETIQ(ipu_cmd.current)) return;
				break;

			case SCE_IPU_SETVQ:
				if (!ipuSETVQ(ipu_cmd.current)) return;
				break;

			case SCE_IPU_CSC:
				if (!ipuCSC(ipu_cmd.current)) return;
				break;

			case SCE_IPU_PACK:
				if (!ipuPACK(ipu_cmd.current)) return;
				break;

			default:
				break;
		}

		// success
		ipuRegs.ctrl.BUSY = 0;
		hwIntcIrq(INTC_IPU);
	}
}
