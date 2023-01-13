/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "Utilities/MemcpyFast.h"
#include "Common.h"

#include "IPU.h"
#include "IPUdma.h"
#include "mpeg2lib/Mpeg.h"

#include <limits.h>
#include "AppConfig.h"

#include "Utilities/MemsetFast.inl"

// the BP doesn't advance and returns -1 if there is no data to be read
__aligned16 tIPU_cmd ipu_cmd;
__aligned16 tIPU_BP g_BP;
__aligned16 decoder_t decoder;

// Quantization matrix
static rgb16_t vqclut[16];			//clut conversion table
static u16 s_thresh[2];				//thresholds for color conversions
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
		case (IPU_CMD & 0xff) : // IPU_CMD
		{
			if (ipu_cmd.CMD != SCE_IPU_FDEC && ipu_cmd.CMD != SCE_IPU_VDEC)
			{
				if (getBits32((u8*)&ipuRegs.cmd.DATA, 0))
					ipuRegs.cmd.DATA = BigEndian(ipuRegs.cmd.DATA);
			}
			return ipuRegs.cmd.DATA;
		}

		case (IPU_CTRL & 0xff): // IPU_CTRL
		{
			ipuRegs.ctrl.IFC = g_BP.IFC;
			ipuRegs.ctrl.CBP = coded_block_pattern;
			return ipuRegs.ctrl._u32;
		}

		case (IPU_BP & 0xff): // IPU_BP
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
		case (IPU_CMD & 0xff): // IPU_CMD
		{
			if (ipu_cmd.CMD != SCE_IPU_FDEC && ipu_cmd.CMD != SCE_IPU_VDEC)
			{
				if (getBits32((u8*)&ipuRegs.cmd.DATA, 0))
					ipuRegs.cmd.DATA = BigEndian(ipuRegs.cmd.DATA);
			}

			return ipuRegs.cmd._u64;
		}

		case (IPU_CTRL & 0xff):
		case (IPU_BP   & 0xff):
		case (IPU_TOP  & 0xff): // IPU_TOP
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
		case (IPU_CMD & 0xff): // IPU_CMD
			IPUCMD_WRITE(value);
			IPUProcessInterrupt();
			return false;

		case (IPU_CTRL & 0xff): // IPU_CTRL
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

// returns false when the writeback is handled, true if the caller should do the
// writeback itself.
__fi bool ipuWrite64(u32 mem, u64 value)
{
	mem &= 0xfff;

	switch (mem)
	{
		case (IPU_CMD & 0xff):
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

	return true;
}

// IPU-correct yuv conversions by Pseudonym
// SSE2 Implementation by Pseudonym

// The IPU's colour space conversion conforms to ITU-R Recommendation BT.601 if anyone wants to make a
// faster or "more accurate" implementation, but this is the precise documented integer method used by
// the hardware and is fast enough with SSE2.

#define IPU_Y_BIAS    16
#define IPU_C_BIAS    128
#define IPU_Y_COEFF   0x95	//  1.1640625
#define IPU_GCR_COEFF (-0x68)	// -0.8125
#define IPU_GCB_COEFF (-0x32)	// -0.390625
#define IPU_RCR_COEFF 0xcc	//  1.59375
#define IPU_BCB_COEFF 0x102	//  2.015625

#if !defined(_M_SSE)
#if defined(__GNUC__)
#if defined(__SSE2__)
#define _M_SSE 0x200
#endif
#endif

#if !defined(_M_SSE) && (!defined(_WIN32) || defined(_M_AMD64) || defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define _M_SSE 0x200
#endif
#endif

static __ri void yuv2rgb(void)
{
#if _M_SSE >= 0x200
	// Suikoden Tactics FMV speed results: Reference - ~72fps, SSE2 - ~120fps
	// An AVX2 version is only slightly faster than an SSE2 version (+2-3fps)
	// (or I'm a poor optimiser), though it might be worth attempting again
	// once we've ported to 64 bits (the extra registers should help).
	const __m128i c_bias          = _mm_set1_epi8(s8(IPU_C_BIAS));
	const __m128i y_bias          = _mm_set1_epi8(IPU_Y_BIAS);
	const __m128i y_mask          = _mm_set1_epi16(s16(0xFF00));
	// Specifying round off instead of round down as everywhere else
	// implies that this is right
	const __m128i round_1bit      = _mm_set1_epi16(0x0001);

	const __m128i y_coefficient   = _mm_set1_epi16(s16(IPU_Y_COEFF << 2));
	const __m128i gcr_coefficient = _mm_set1_epi16(s16(u16(IPU_GCR_COEFF) << 2));
	const __m128i gcb_coefficient = _mm_set1_epi16(s16(u16(IPU_GCB_COEFF) << 2));
	const __m128i rcr_coefficient = _mm_set1_epi16(s16(IPU_RCR_COEFF << 2));
	const __m128i bcb_coefficient = _mm_set1_epi16(s16(IPU_BCB_COEFF << 2));

	// Alpha set to 0x80 here. The threshold stuff is done later.
	const __m128i& alpha = c_bias;

	for (int n = 0; n < 8; ++n) {
		// could skip the loadl_epi64 but most SSE instructions require 128-bit
		// alignment so two versions would be needed.
		__m128i cb = _mm_loadl_epi64(reinterpret_cast<__m128i*>(&decoder.mb8.Cb[n][0]));
		__m128i cr = _mm_loadl_epi64(reinterpret_cast<__m128i*>(&decoder.mb8.Cr[n][0]));

		// (Cb - 128) << 8, (Cr - 128) << 8
		cb = _mm_xor_si128(cb, c_bias);
		cr = _mm_xor_si128(cr, c_bias);
		cb = _mm_unpacklo_epi8(_mm_setzero_si128(), cb);
		cr = _mm_unpacklo_epi8(_mm_setzero_si128(), cr);

		__m128i rc = _mm_mulhi_epi16(cr, rcr_coefficient);
		__m128i gc = _mm_adds_epi16(_mm_mulhi_epi16(cr, gcr_coefficient), _mm_mulhi_epi16(cb, gcb_coefficient));
		__m128i bc = _mm_mulhi_epi16(cb, bcb_coefficient);

		for (int m = 0; m < 2; ++m) {
			__m128i y = _mm_load_si128(reinterpret_cast<__m128i*>(&decoder.mb8.Y[n * 2 + m][0]));
			y = _mm_subs_epu8(y, y_bias);
			// Y << 8 for pixels 0, 2, 4, 6, 8, 10, 12, 14
			__m128i y_even = _mm_slli_epi16(y, 8);
			// Y << 8 for pixels 1, 3, 5, 7 ,9, 11, 13, 15
			__m128i y_odd = _mm_and_si128(y, y_mask);

			y_even = _mm_mulhi_epu16(y_even, y_coefficient);
			y_odd  = _mm_mulhi_epu16(y_odd,  y_coefficient);

			__m128i r_even = _mm_adds_epi16(rc, y_even);
			__m128i r_odd  = _mm_adds_epi16(rc, y_odd);
			__m128i g_even = _mm_adds_epi16(gc, y_even);
			__m128i g_odd  = _mm_adds_epi16(gc, y_odd);
			__m128i b_even = _mm_adds_epi16(bc, y_even);
			__m128i b_odd  = _mm_adds_epi16(bc, y_odd);

			// round
			r_even = _mm_srai_epi16(_mm_add_epi16(r_even, round_1bit), 1);
			r_odd  = _mm_srai_epi16(_mm_add_epi16(r_odd,  round_1bit), 1);
			g_even = _mm_srai_epi16(_mm_add_epi16(g_even, round_1bit), 1);
			g_odd  = _mm_srai_epi16(_mm_add_epi16(g_odd,  round_1bit), 1);
			b_even = _mm_srai_epi16(_mm_add_epi16(b_even, round_1bit), 1);
			b_odd  = _mm_srai_epi16(_mm_add_epi16(b_odd,  round_1bit), 1);

			// combine even and odd bytes in original order
			__m128i r = _mm_packus_epi16(r_even, r_odd);
			__m128i g = _mm_packus_epi16(g_even, g_odd);
			__m128i b = _mm_packus_epi16(b_even, b_odd);

			r = _mm_unpacklo_epi8(r, _mm_shuffle_epi32(r, _MM_SHUFFLE(3, 2, 3, 2)));
			g = _mm_unpacklo_epi8(g, _mm_shuffle_epi32(g, _MM_SHUFFLE(3, 2, 3, 2)));
			b = _mm_unpacklo_epi8(b, _mm_shuffle_epi32(b, _MM_SHUFFLE(3, 2, 3, 2)));

			// Create RGBA (we could generate A here, but we don't) quads
			__m128i rg_l = _mm_unpacklo_epi8(r, g);
			__m128i ba_l = _mm_unpacklo_epi8(b, alpha);
			__m128i rgba_ll = _mm_unpacklo_epi16(rg_l, ba_l);
			__m128i rgba_lh = _mm_unpackhi_epi16(rg_l, ba_l);

			__m128i rg_h = _mm_unpackhi_epi8(r, g);
			__m128i ba_h = _mm_unpackhi_epi8(b, alpha);
			__m128i rgba_hl = _mm_unpacklo_epi16(rg_h, ba_h);
			__m128i rgba_hh = _mm_unpackhi_epi16(rg_h, ba_h);

			_mm_store_si128(reinterpret_cast<__m128i*>(&decoder.rgb32.c[n * 2 + m][0]), rgba_ll);
			_mm_store_si128(reinterpret_cast<__m128i*>(&decoder.rgb32.c[n * 2 + m][4]), rgba_lh);
			_mm_store_si128(reinterpret_cast<__m128i*>(&decoder.rgb32.c[n * 2 + m][8]), rgba_hl);
			_mm_store_si128(reinterpret_cast<__m128i*>(&decoder.rgb32.c[n * 2 + m][12]), rgba_hh);
		}
	}
#else
	// conforming implementation for reference, do not optimise
	const macroblock_8& mb8 = decoder.mb8;
	macroblock_rgb32& rgb32 = decoder.rgb32;

	for (int y = 0; y < 16; y++)
		for (int x = 0; x < 16; x++)
		{
			s32 lum = (IPU_Y_COEFF * (std::max(0, (s32)mb8.Y[y][x] - IPU_Y_BIAS))) >> 6;
			s32 rcr = (IPU_RCR_COEFF * ((s32)mb8.Cr[y>>1][x>>1] - 128)) >> 6;
			s32 gcr = (IPU_GCR_COEFF * ((s32)mb8.Cr[y>>1][x>>1] - 128)) >> 6;
			s32 gcb = (IPU_GCB_COEFF * ((s32)mb8.Cb[y>>1][x>>1] - 128)) >> 6;
			s32 bcb = (IPU_BCB_COEFF * ((s32)mb8.Cb[y>>1][x>>1] - 128)) >> 6;

			rgb32.c[y][x].r = std::max(0, std::min(255, (lum + rcr + 1) >> 1));
			rgb32.c[y][x].g = std::max(0, std::min(255, (lum + gcr + gcb + 1) >> 1));
			rgb32.c[y][x].b = std::max(0, std::min(255, (lum + bcb + 1) >> 1));
			rgb32.c[y][x].a = 0x80; // the norm to save doing this on the alpha pass
		}
#endif
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
