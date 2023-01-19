/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include <libretro.h>

#include "Global.h"

/* Forward declaration */
extern retro_audio_sample_t sample_cb;

/* Performs a 64-bit multiplication between two values and returns the
 * high 32 bits as a result (discarding the fractional 32 bits).
 * The combined fractional bits of both inputs must be 32 bits for this
 * to work properly.
 *
 * This is meant to be a drop-in replacement for times when the 'div' part
 * of a MulDiv is a constant.  (example: 1<<8, or 4096, etc)
 *
 * [Air] Performance breakdown: This is over 10 times faster than MulDiv in
 *   a *worst case* scenario.  It's also more accurate since it forces the
 *   caller to  extend the inputs so that they make use of all 32 bits of
 *   precision.
 */
#define MULSHR32(srcval, mulval) ((s64)(srcval) * (mulval) >> 32)

static void SPU2_FORCEINLINE XA_decode_block(s16* buffer, const s16* block, s32& prev1, s32& prev2)
{
	static const s32 tbl_XA_Factor[16][2] =
	{
		{0, 0},
		{60, 0},
		{115, -52},
		{98, -55},
		{122, -60}
	};
	const s32 header     = *block;
	const s32 shift      = (header & 0xF) + 16;
	const int id         = header >> 4 & 0xF;
	const s32 pred1      = tbl_XA_Factor[id][0];
	const s32 pred2      = tbl_XA_Factor[id][1];

	const s8* blockbytes = (s8*)&block[1];
	const s8* blockend   = &blockbytes[13];

	for (; blockbytes <= blockend; ++blockbytes)
	{
		s32 data     = ((*blockbytes) << 28) & 0xF0000000;
		s32 pcm      = (data >> shift) + (((pred1 * prev1) + (pred2 * prev2) + 32) >> 6);

		pcm          = std::min(std::max(pcm, -0x8000), 0x7fff);
		*(buffer++)  = pcm;

		data         = ((*blockbytes) << 24) & 0xF0000000;
		s32 pcm2     = (data >> shift) + (((pred1 * pcm) + (pred2 * prev1) + 32) >> 6);

		pcm2         = std::min(std::max(pcm2, -0x8000), 0x7fff);
		*(buffer++)  = pcm2;

		prev2        = pcm;
		prev1        = pcm2;
	}
}

/* Decoded PCM data, used to cache the decoded data so that it needn't be decoded
 * multiple times.  Cache chunks are decoded when the mixer requests the blocks, and
 * invalided when DMA transfers and memory writes are performed.
 */
PcmCacheEntry* pcm_cache_data = nullptr;

/* LOOP/END sets the ENDX bit and sets NAX to LSA, and the voice is muted if LOOP is not set
 * LOOP seems to only have any effect on the block with LOOP/END set, where it prevents muting the voice
 * (the documented requirement that every block in a loop has the LOOP bit set is nonsense according to tests)
 * LOOP/START sets LSA to NAX unless LSA was written manually since sound generation started
 * (see LoopMode, the method by which this is achieved on the real SPU2 is unknown)
 */
#define XAFLAG_LOOP_END (1ul << 0)
#define XAFLAG_LOOP (1ul << 1)
#define XAFLAG_LOOP_START (1ul << 2)

static SPU2_FORCEINLINE s32 GetNextDataBuffered(V_Core& thiscore, uint voiceidx)
{
	V_Voice& vc(thiscore.Voices[voiceidx]);

	if ((vc.SCurrent & 3) == 0)
	{
		/* Important!  Both cores signal IRQ when an address is read, regardless of
		 * which core actually reads the address. */
		if (Cores[0].IRQEnable && (vc.NextA == Cores[0].IRQA))
			SetIrqCall(0);
		if (Cores[1].IRQEnable && (vc.NextA == Cores[1].IRQA))
			SetIrqCall(1);

		vc.NextA++;
		vc.NextA &= 0xFFFFF;

		if ((vc.NextA & 7) == 0) // vc.SCurrent == 24 equivalent
		{
			if (vc.LoopFlags & XAFLAG_LOOP_END)
			{
				thiscore.Regs.ENDX |= (1 << voiceidx);
				vc.NextA = vc.LoopStartA | 1;
				if (!(vc.LoopFlags & XAFLAG_LOOP))
					vc.Stop();
			}
			else
				vc.NextA++; // no, don't IncrementNextA here.  We haven't read the header yet.
		}
	}

	if (vc.SCurrent == 28)
	{
		vc.SCurrent = 0;

		// We'll need the loop flags and buffer pointers regardless of cache status:
		if (Cores[0].IRQEnable && Cores[0].IRQA == (vc.NextA & 0xFFFF8))
			SetIrqCall(0);
		if (Cores[1].IRQEnable && Cores[1].IRQA == (vc.NextA & 0xFFFF8))
			SetIrqCall(1);

		s16* memptr  = GetMemPtr(vc.NextA & 0xFFFF8);
		vc.LoopFlags = *memptr >> 8; // grab loop flags from the upper byte.

		if ((vc.LoopFlags & XAFLAG_LOOP_START) && !vc.LoopMode)
			vc.LoopStartA = vc.NextA & 0xFFFF8;

		const int cacheIdx = vc.NextA / PCM_WORDS_PER_BLOCK;
		PcmCacheEntry& cacheLine = pcm_cache_data[cacheIdx];
		vc.SBuffer = cacheLine.Sampledata;

		if (cacheLine.Validated)
		{
			// Cached block!  Read from the cache directly.
			// Make sure to propagate the prev1/prev2 ADPCM:

			vc.Prev1 = vc.SBuffer[27];
			vc.Prev2 = vc.SBuffer[26];
		}
		else
		{
			// Only flag the cache if it's a non-dynamic memory range.
			if (vc.NextA >= SPU2_DYN_MEMLINE)
				cacheLine.Validated = true;

			XA_decode_block(vc.SBuffer, memptr, vc.Prev1, vc.Prev2);
		}
	}

	return vc.SBuffer[vc.SCurrent++];
}

static SPU2_FORCEINLINE void GetNextDataDummy(V_Core& thiscore, uint voiceidx)
{
	V_Voice& vc(thiscore.Voices[voiceidx]);

	/* Important!  Both cores signal IRQ when an address is read, regardless of
	 * which core actually reads the address. */
	if (Cores[0].IRQEnable && (vc.NextA == Cores[0].IRQA))
		SetIrqCall(0);
	if (Cores[1].IRQEnable && (vc.NextA == Cores[1].IRQA))
		SetIrqCall(1);

	vc.NextA++;
	vc.NextA &= 0xFFFFF;

	if ((vc.NextA & 7) == 0) // vc.SCurrent == 24 equivalent
	{
		if (vc.LoopFlags & XAFLAG_LOOP_END)
		{
			thiscore.Regs.ENDX |= (1 << voiceidx);
			vc.NextA = vc.LoopStartA | 1;
		}
		else
			vc.NextA++; // no, don't IncrementNextA here.  We haven't read the header yet.
	}

	if (vc.SCurrent == 28)
	{
		if (Cores[0].IRQEnable && Cores[0].IRQA == (vc.NextA & 0xFFFF8))
			SetIrqCall(0);
		if (Cores[1].IRQEnable && Cores[1].IRQA == (vc.NextA & 0xFFFF8))
			SetIrqCall(1);

		vc.LoopFlags = *GetMemPtr(vc.NextA & 0xFFFF8) >> 8; // grab loop flags from the upper byte.

		if ((vc.LoopFlags & XAFLAG_LOOP_START) && !vc.LoopMode)
			vc.LoopStartA = vc.NextA & 0xFFFF8;

		vc.SCurrent = 0;
	}

	vc.SP       -= 0x1000 * (4 - (vc.SCurrent & 3));
	vc.SCurrent += 4           - (vc.SCurrent & 3);
}

/* Data is expected to be 16 bit signed (typical stuff!).
 * volume is expected to be 32 bit signed (31 bits with reverse phase)
 * Data is shifted up by 1 bit to give the output an effective 16 bit range.
 */
#define APPLY_VOLUME(s1, s2, s3, s4) StereoOut32(MULSHR32((s1) << 1, (s2)), MULSHR32((s3) << 1, (s4)))

/* writes a signed value to the SPU2 ram
 * Performs no cache invalidation -- use only for dynamic memory ranges
 * of the SPU2 (between 0x0000 and SPU2_DYN_MEMLINE)
 */
static SPU2_FORCEINLINE void spu2M_WriteFast(u32 addr, s16 value)
{
	if (Cores[0].IRQEnable && Cores[0].IRQA == addr)
		SetIrqCall(0);
	if (Cores[1].IRQEnable && Cores[1].IRQA == addr)
		SetIrqCall(1);
	*GetMemPtr(addr) = value;
}

const VoiceMixSet VoiceMixSet::Empty((StereoOut32()), (StereoOut32())); // Don't use SteroOut32::Empty because C++ doesn't make any dep/order checks on global initializers.

static const s16 interpTable[0x200] = {
	-0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, //
	-0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, //
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, //
	0x0001, 0x0001, 0x0001, 0x0002, 0x0002, 0x0002, 0x0003, 0x0003, //
	0x0003, 0x0004, 0x0004, 0x0005, 0x0005, 0x0006, 0x0007, 0x0007, //
	0x0008, 0x0009, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, //
	0x000F, 0x0010, 0x0011, 0x0012, 0x0013, 0x0015, 0x0016, 0x0018, // entry
	0x0019, 0x001B, 0x001C, 0x001E, 0x0020, 0x0021, 0x0023, 0x0025, // 000..07F
	0x0027, 0x0029, 0x002C, 0x002E, 0x0030, 0x0033, 0x0035, 0x0038, //
	0x003A, 0x003D, 0x0040, 0x0043, 0x0046, 0x0049, 0x004D, 0x0050, //
	0x0054, 0x0057, 0x005B, 0x005F, 0x0063, 0x0067, 0x006B, 0x006F, //
	0x0074, 0x0078, 0x007D, 0x0082, 0x0087, 0x008C, 0x0091, 0x0096, //
	0x009C, 0x00A1, 0x00A7, 0x00AD, 0x00B3, 0x00BA, 0x00C0, 0x00C7, //
	0x00CD, 0x00D4, 0x00DB, 0x00E3, 0x00EA, 0x00F2, 0x00FA, 0x0101, //
	0x010A, 0x0112, 0x011B, 0x0123, 0x012C, 0x0135, 0x013F, 0x0148, //
	0x0152, 0x015C, 0x0166, 0x0171, 0x017B, 0x0186, 0x0191, 0x019C, //
	0x01A8, 0x01B4, 0x01C0, 0x01CC, 0x01D9, 0x01E5, 0x01F2, 0x0200, //
	0x020D, 0x021B, 0x0229, 0x0237, 0x0246, 0x0255, 0x0264, 0x0273, //
	0x0283, 0x0293, 0x02A3, 0x02B4, 0x02C4, 0x02D6, 0x02E7, 0x02F9, //
	0x030B, 0x031D, 0x0330, 0x0343, 0x0356, 0x036A, 0x037E, 0x0392, //
	0x03A7, 0x03BC, 0x03D1, 0x03E7, 0x03FC, 0x0413, 0x042A, 0x0441, //
	0x0458, 0x0470, 0x0488, 0x04A0, 0x04B9, 0x04D2, 0x04EC, 0x0506, //
	0x0520, 0x053B, 0x0556, 0x0572, 0x058E, 0x05AA, 0x05C7, 0x05E4, // entry
	0x0601, 0x061F, 0x063E, 0x065C, 0x067C, 0x069B, 0x06BB, 0x06DC, // 080..0FF
	0x06FD, 0x071E, 0x0740, 0x0762, 0x0784, 0x07A7, 0x07CB, 0x07EF, //
	0x0813, 0x0838, 0x085D, 0x0883, 0x08A9, 0x08D0, 0x08F7, 0x091E, //
	0x0946, 0x096F, 0x0998, 0x09C1, 0x09EB, 0x0A16, 0x0A40, 0x0A6C, //
	0x0A98, 0x0AC4, 0x0AF1, 0x0B1E, 0x0B4C, 0x0B7A, 0x0BA9, 0x0BD8, //
	0x0C07, 0x0C38, 0x0C68, 0x0C99, 0x0CCB, 0x0CFD, 0x0D30, 0x0D63, //
	0x0D97, 0x0DCB, 0x0E00, 0x0E35, 0x0E6B, 0x0EA1, 0x0ED7, 0x0F0F, //
	0x0F46, 0x0F7F, 0x0FB7, 0x0FF1, 0x102A, 0x1065, 0x109F, 0x10DB, //
	0x1116, 0x1153, 0x118F, 0x11CD, 0x120B, 0x1249, 0x1288, 0x12C7, //
	0x1307, 0x1347, 0x1388, 0x13C9, 0x140B, 0x144D, 0x1490, 0x14D4, //
	0x1517, 0x155C, 0x15A0, 0x15E6, 0x162C, 0x1672, 0x16B9, 0x1700, //
	0x1747, 0x1790, 0x17D8, 0x1821, 0x186B, 0x18B5, 0x1900, 0x194B, //
	0x1996, 0x19E2, 0x1A2E, 0x1A7B, 0x1AC8, 0x1B16, 0x1B64, 0x1BB3, //
	0x1C02, 0x1C51, 0x1CA1, 0x1CF1, 0x1D42, 0x1D93, 0x1DE5, 0x1E37, //
	0x1E89, 0x1EDC, 0x1F2F, 0x1F82, 0x1FD6, 0x202A, 0x207F, 0x20D4, //
	0x2129, 0x217F, 0x21D5, 0x222C, 0x2282, 0x22DA, 0x2331, 0x2389, // entry
	0x23E1, 0x2439, 0x2492, 0x24EB, 0x2545, 0x259E, 0x25F8, 0x2653, // 100..17F
	0x26AD, 0x2708, 0x2763, 0x27BE, 0x281A, 0x2876, 0x28D2, 0x292E, //
	0x298B, 0x29E7, 0x2A44, 0x2AA1, 0x2AFF, 0x2B5C, 0x2BBA, 0x2C18, //
	0x2C76, 0x2CD4, 0x2D33, 0x2D91, 0x2DF0, 0x2E4F, 0x2EAE, 0x2F0D, //
	0x2F6C, 0x2FCC, 0x302B, 0x308B, 0x30EA, 0x314A, 0x31AA, 0x3209, //
	0x3269, 0x32C9, 0x3329, 0x3389, 0x33E9, 0x3449, 0x34A9, 0x3509, //
	0x3569, 0x35C9, 0x3629, 0x3689, 0x36E8, 0x3748, 0x37A8, 0x3807, //
	0x3867, 0x38C6, 0x3926, 0x3985, 0x39E4, 0x3A43, 0x3AA2, 0x3B00, //
	0x3B5F, 0x3BBD, 0x3C1B, 0x3C79, 0x3CD7, 0x3D35, 0x3D92, 0x3DEF, //
	0x3E4C, 0x3EA9, 0x3F05, 0x3F62, 0x3FBD, 0x4019, 0x4074, 0x40D0, //
	0x412A, 0x4185, 0x41DF, 0x4239, 0x4292, 0x42EB, 0x4344, 0x439C, //
	0x43F4, 0x444C, 0x44A3, 0x44FA, 0x4550, 0x45A6, 0x45FC, 0x4651, //
	0x46A6, 0x46FA, 0x474E, 0x47A1, 0x47F4, 0x4846, 0x4898, 0x48E9, //
	0x493A, 0x498A, 0x49D9, 0x4A29, 0x4A77, 0x4AC5, 0x4B13, 0x4B5F, //
	0x4BAC, 0x4BF7, 0x4C42, 0x4C8D, 0x4CD7, 0x4D20, 0x4D68, 0x4DB0, //
	0x4DF7, 0x4E3E, 0x4E84, 0x4EC9, 0x4F0E, 0x4F52, 0x4F95, 0x4FD7, // entry
	0x5019, 0x505A, 0x509A, 0x50DA, 0x5118, 0x5156, 0x5194, 0x51D0, // 180..1FF
	0x520C, 0x5247, 0x5281, 0x52BA, 0x52F3, 0x532A, 0x5361, 0x5397, //
	0x53CC, 0x5401, 0x5434, 0x5467, 0x5499, 0x54CA, 0x54FA, 0x5529, //
	0x5558, 0x5585, 0x55B2, 0x55DE, 0x5609, 0x5632, 0x565B, 0x5684, //
	0x56AB, 0x56D1, 0x56F6, 0x571B, 0x573E, 0x5761, 0x5782, 0x57A3, //
	0x57C3, 0x57E2, 0x57FF, 0x581C, 0x5838, 0x5853, 0x586D, 0x5886, //
	0x589E, 0x58B5, 0x58CB, 0x58E0, 0x58F4, 0x5907, 0x5919, 0x592A, //
	0x593A, 0x5949, 0x5958, 0x5965, 0x5971, 0x597C, 0x5986, 0x598F, //
	0x5997, 0x599E, 0x59A4, 0x59A9, 0x59AD, 0x59B0, 0x59B2, 0x59B3  //
};

static SPU2_FORCEINLINE void MixCoreVoices(VoiceMixSet& dest, const uint coreidx)
{
	V_Core& thiscore(Cores[coreidx]);

	for (uint voiceidx = 0; voiceidx < NUM_VOICES; ++voiceidx)
	{
		StereoOut32 VVal;
		s32 Value = 0;

		V_Voice& vc(thiscore.Voices[voiceidx]);

		/* Most games don't use much volume slide effects.  So only call the UpdateVolume
		 * methods when needed by checking the flag outside the method here...
		 * (Note: Ys 6 : Ark of Nephistm uses these effects)
		 */

		if ((vc.Volume.Left.Mode & VOLFLAG_SLIDE_ENABLE)  && vc.Volume.Left.Increment  != 0x7f)
			vc.Volume.Left.Update();
		if ((vc.Volume.Right.Mode & VOLFLAG_SLIDE_ENABLE) && vc.Volume.Right.Increment != 0x7f)
			vc.Volume.Right.Update();

		/* SPU2 Note: The spu2 continues to process voices for eternity, always, so we
		 * have to run through all the motions of updating the voice regardless of it's
		 * audible status.  Otherwise IRQs might not trigger and emulation might fail.
		 */

		/* Update pitch */
		/* [Air] : re-ordered comparisons: Modulated is much more likely to be zero than voice,
		 *   and so the way it was before it's have to check both voice and modulated values
		 *   most of the time.  Now it'll just check Modulated and short-circuit past the voice
		 *   check (not that it amounts to much, but eh every little bit helps). */
		if ((vc.Modulated == 0) || ((voiceidx) == 0))
			vc.SP += std::min((s32)vc.Pitch, 0x3FFF);
		else
			vc.SP += std::min(std::max((vc.Pitch * (0x8000 + Cores[(coreidx)].Voices[(voiceidx) - 1].OutX)) >> 15, 0), 0x3FFF);

		if (vc.ADSR.Phase > 0)
		{
			if (vc.Noise)
			{
				/* Get noise values */
				static u16 lfsr = 0xC0FEu;
				u16 bit         = lfsr ^ (lfsr << 3) ^ (lfsr << 4) ^ (lfsr << 5);
				lfsr            = (lfsr << 1) | (bit >> 15);
				Value           = (s16)lfsr;
			}
			else
			{         
				/* Get voice values */
				while (vc.SP > 0)
				{
					vc.PV4 = vc.PV3;
					vc.PV3 = vc.PV2;
					vc.PV2 = vc.PV1;
					vc.PV1 = GetNextDataBuffered(thiscore, voiceidx);
					vc.SP -= 0x1000;
				}

				s32 mu  = vc.SP + 0x1000;
				s32 pv4 = vc.PV4; 
				s32 pv3 = vc.PV3;
				s32 pv2 = vc.PV2;
				s32 pv1 = vc.PV1;
				s32 i   = (mu & 0x0FF0) >> 4;
				/* Gaussian interpolation */
				Value   =   ((interpTable[0x0FF - i] * pv4) >> 15)
					+   ((interpTable[0x1FF - i] * pv3) >> 15)
					+   ((interpTable[0x100 + i] * pv2) >> 15)
					+   ((interpTable[0x000 + i] * pv1) >> 15);
			}

			/* Update and Apply ADSR  (applies to normal and noise sources)
			 *
			 * Note!  It's very important that ADSR stay as accurate as possible.  By the way
			 * it is used, various sound effects can end prematurely if we truncate more than
			 * one or two bits.  Best result comes from no truncation at all, which is why we
			 * use a full 64-bit multiply/result here.
			 */
			if (vc.ADSR.Phase == 0)
				vc.ADSR.Value = 0;
			else if (!vc.ADSR.Calculate())
				vc.Stop();

			Value    = MULSHR32(Value, vc.ADSR.Value);
			vc.OutX  = Value;

			VVal = APPLY_VOLUME(Value, vc.Volume.Left.Value, Value, vc.Volume.Right.Value);
		}
		else
		{
			while (vc.SP >= 0)
				GetNextDataDummy(thiscore, voiceidx); /* Dummy is enough */
			VVal.Left  = 0;
			VVal.Right = 0;
		}

		// Write-back of raw voice data (post ADSR applied)
		if (voiceidx == 1)
			spu2M_WriteFast(((0 == coreidx) ? 0x400 : 0xc00) + OutPos, Value);
		else if (voiceidx == 3)
			spu2M_WriteFast(((0 == coreidx) ? 0x600 : 0xe00) + OutPos, Value);

		// Note: Results from MixVoice are ranged at 16 bits.

		dest.Dry.Left  += VVal.Left  & thiscore.VoiceGates[voiceidx].DryL;
		dest.Dry.Right += VVal.Right & thiscore.VoiceGates[voiceidx].DryR;
		dest.Wet.Left  += VVal.Left  & thiscore.VoiceGates[voiceidx].WetL;
		dest.Wet.Right += VVal.Right & thiscore.VoiceGates[voiceidx].WetR;
	}
}

StereoOut32 V_Core::Mix(const VoiceMixSet& inVoices, const StereoOut32& Input, const StereoOut32& Ext)
{
	if ((MasterVol.Left.Mode & VOLFLAG_SLIDE_ENABLE)  && MasterVol.Left.Increment  != 0x7f)
		MasterVol.Left.Update();
	if ((MasterVol.Right.Mode & VOLFLAG_SLIDE_ENABLE) && MasterVol.Right.Increment != 0x7f)
		MasterVol.Right.Update();

	// Saturate final result to standard 16 bit range.
	const VoiceMixSet Voices(
			StereoOut32(
				CLAMP_MIX(inVoices.Dry.Left),
				CLAMP_MIX(inVoices.Dry.Right)),
			StereoOut32(
				CLAMP_MIX(inVoices.Wet.Left),
				CLAMP_MIX(inVoices.Wet.Right))
			);

	// Write Mixed results To Output Area
	if (Index == 0)
	{
		spu2M_WriteFast(0x1000 + OutPos, Voices.Dry.Left);
		spu2M_WriteFast(0x1200 + OutPos, Voices.Dry.Right);
		spu2M_WriteFast(0x1400 + OutPos, Voices.Wet.Left);
		spu2M_WriteFast(0x1600 + OutPos, Voices.Wet.Right);
	}
	else
	{
		spu2M_WriteFast(0x1800 + OutPos, Voices.Dry.Left);
		spu2M_WriteFast(0x1A00 + OutPos, Voices.Dry.Right);
		spu2M_WriteFast(0x1C00 + OutPos, Voices.Wet.Left);
		spu2M_WriteFast(0x1E00 + OutPos, Voices.Wet.Right);
	}

	// Mix in the Input data

	StereoOut32 TD(
		Input.Left & DryGate.InpL,
		Input.Right & DryGate.InpR);

	// Mix in the Voice data
	TD.Left += Voices.Dry.Left & DryGate.SndL;
	TD.Right += Voices.Dry.Right & DryGate.SndR;

	// Mix in the External (nothing/core0) data
	TD.Left += Ext.Left & DryGate.ExtL;
	TD.Right += Ext.Right & DryGate.ExtR;

	// ----------------------------------------------------------------------------
	//    Reverberation Effects Processing
	// ----------------------------------------------------------------------------
	// SPU2 has an FxEnable bit which seems to disable all reverb processing *and*
	// output, but does *not* disable the advancing buffers.  IRQs are not triggered
	// and reverb is rendered silent.
	//
	// Technically we should advance the buffers even when fx are disabled.  However
	// there are two things that make this very unlikely to matter:
	//
	//  1. Any SPU2 app wanting to avoid noise or pops needs to clear the reverb buffers
	//     when adjusting settings anyway; so the read/write positions in the reverb
	//     buffer after FxEnabled is set back to 1 doesn't really matter.
	//
	//  2. Writes to ESA (and possibly EEA) reset the buffer pointers to 0.
	//
	// On the other hand, updating the buffer is cheap and easy, so might as well. ;)

	Reverb_AdvanceBuffer(); // Updates the reverb work area as well, if needed.

	// ToDo:
	// Bad EndA causes memory corruption. Bad for us, unknown on PS2!
	// According to no$psx, effects always run but don't always write back, so the FxEnable check may be wrong
	if (!FxEnable || EffectsEndA >= 0x100000)
		return TD;

	StereoOut32 TW;

	// Mix Input, Voice, and External data:

	TW.Left = Input.Left & WetGate.InpL;
	TW.Right = Input.Right & WetGate.InpR;

	TW.Left += Voices.Wet.Left & WetGate.SndL;
	TW.Right += Voices.Wet.Right & WetGate.SndR;
	TW.Left += Ext.Left & WetGate.ExtL;
	TW.Right += Ext.Right & WetGate.ExtR;

	StereoOut32 RV           = DoReverb(TW);

	// Mix Dry + Wet
	// (master volume is applied later to the result of both outputs added together).
	const StereoOut32& right = APPLY_VOLUME(RV.Left, FxVol.Left, RV.Right, FxVol.Right);
	return StereoOut32(
			TD.Left + right.Left,
			TD.Right + right.Right);
}

void SPU2_Mix(void)
{
	const StereoOut32& core0_data = Cores[0].ReadInput();
	const StereoOut32& core1_data = Cores[1].ReadInput();

	// Note: Playmode 4 is SPDIF, which overrides other inputs.
	StereoOut32 InputData[2] =
		{
			// SPDIF is on Core 0:
			// Fixme:
			// 1. We do not have an AC3 decoder for the bitstream.
			// 2. Games usually provide a normal ADMA stream as well and want to see it getting read!
			/*(PlayMode&4) ? StereoOut32::Empty : */ APPLY_VOLUME(core0_data.Left, Cores[0].InpVol.Left, core0_data.Right, Cores[0].InpVol.Right),

			/* CDDA is on Core 1: */
			(PlayMode & 8) ? StereoOut32(0, 0) : APPLY_VOLUME(core1_data.Left, Cores[1].InpVol.Left, core1_data.Right, Cores[1].InpVol.Right)};

	/* Todo: Replace me with memzero initializer! */
	VoiceMixSet VoiceData[2] = {VoiceMixSet::Empty, VoiceMixSet::Empty}; /* mixed voice data for each core. */
	MixCoreVoices(VoiceData[0], 0);
	MixCoreVoices(VoiceData[1], 1);

	StereoOut32 Ext(Cores[0].Mix(VoiceData[0], InputData[0], StereoOut32(0, 0)));

	if ((PlayMode & 4) || (Cores[0].Mute != 0))
		Ext = StereoOut32(0, 0);
	else
	{
		const StereoOut32& sample = APPLY_VOLUME(Ext.Left, Cores[0].MasterVol.Left.Value, Ext.Right, Cores[0].MasterVol.Right.Value);
		Ext = StereoOut32(CLAMP_MIX(sample.Left), CLAMP_MIX(sample.Right));
	}

	/* Commit Core 0 output to ram before mixing Core 1: */
	spu2M_WriteFast(0x800 + OutPos, Ext.Left);
	spu2M_WriteFast(0xA00 + OutPos, Ext.Right);

	Ext = APPLY_VOLUME(Ext.Left, Cores[1].ExtVol.Left, Ext.Right, Cores[1].ExtVol.Right);
	StereoOut32 Out(Cores[1].Mix(VoiceData[1], InputData[1], Ext));

	if (PlayMode & 8)
	{
		/* Experimental CDDA support */
		// The CDDA overrides all other mixer output.  It's a direct feed!
		Out       = Cores[1].ReadInput_HiFi();
	}
	else
	{
		Out.Left  = MULSHR32(Out.Left, Cores[1].MasterVol.Left.Value);
		Out.Right = MULSHR32(Out.Right,Cores[1].MasterVol.Right.Value);
	}

	StereoOut16 out16;
	out16.Left        = (s16)CLAMP_MIX(Out.Left);
	out16.Right       = (s16)CLAMP_MIX(Out.Right);
	sample_cb(out16.Left, out16.Right);

	/* Update AutoDMA output positioning */
	OutPos++;
	if (OutPos >= 0x200)
		OutPos = 0;
}
