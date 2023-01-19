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

#include <wx/stream.h>

#include "Utilities/pxStreams.h"

#include "Global.h"
#include "spu2.h"
#include "../R3000A.h"
#include "../IopDma.h"
#include "../../libretro/options_tools.h"

#define PCORE(c, p) U16P(Cores[c].p)
#define PVCP(c, v, p) PCORE(c, Voices[v].p)
#define PVC(c, v)                          \
	PVCP(c, v, Volume.Left.Reg_VOL)        \
	,                                      \
		PVCP(c, v, Volume.Right.Reg_VOL),  \
		PVCP(c, v, Pitch),                 \
		PVCP(c, v, ADSR.regADSR1),         \
		PVCP(c, v, ADSR.regADSR2),         \
		PVCP(c, v, ADSR.Value) + 1,        \
		PVCP(c, v, Volume.Left.Value) + 1, \
		PVCP(c, v, Volume.Right.Value) + 1
#define PVCA(c, v)                  \
	PVCP(c, v, StartA) + 1,         \
		PVCP(c, v, StartA),         \
		PVCP(c, v, LoopStartA) + 1, \
		PVCP(c, v, LoopStartA),     \
		PVCP(c, v, NextA) + 1,      \
		PVCP(c, v, NextA)
#define PRAW(a) ((u16*)nullptr)
#define PREVB_REG(c, n)   \
	PCORE(c, Revb.n) + 1, \
		PCORE(c, Revb.n)

static u16 const* const regtable_original[0x401] =
	{
		// Voice Params: 8 params, 24 voices = 0x180 bytes
		PVC(0, 0), PVC(0, 1), PVC(0, 2), PVC(0, 3), PVC(0, 4), PVC(0, 5),
		PVC(0, 6), PVC(0, 7), PVC(0, 8), PVC(0, 9), PVC(0, 10), PVC(0, 11),
		PVC(0, 12), PVC(0, 13), PVC(0, 14), PVC(0, 15), PVC(0, 16), PVC(0, 17),
		PVC(0, 18), PVC(0, 19), PVC(0, 20), PVC(0, 21), PVC(0, 22), PVC(0, 23),

		PCORE(0, Regs.PMON),
		PCORE(0, Regs.PMON) + 1,
		PCORE(0, Regs.NON),
		PCORE(0, Regs.NON) + 1,
		PCORE(0, Regs.VMIXL),
		PCORE(0, Regs.VMIXL) + 1,
		PCORE(0, Regs.VMIXEL),
		PCORE(0, Regs.VMIXEL) + 1,
		PCORE(0, Regs.VMIXR),
		PCORE(0, Regs.VMIXR) + 1,
		PCORE(0, Regs.VMIXER),
		PCORE(0, Regs.VMIXER) + 1,

		PCORE(0, Regs.MMIX),
		PCORE(0, Regs.ATTR),

		PCORE(0, IRQA) + 1,
		PCORE(0, IRQA),

		nullptr, nullptr,
		nullptr, nullptr,

		PCORE(0, TSA) + 1,
		PCORE(0, TSA),

		PRAW(REG__1AC), PRAW(REG__1AE),

		PCORE(0, AutoDMACtrl),

		PRAW(0x1b2), PRAW(0x1b4), PRAW(0x1b6), PRAW(0x1b8), PRAW(0x1ba), PRAW(0x1bc), PRAW(0x1be), // unknown

		// Voice Addresses
		PVCA(0, 0), PVCA(0, 1), PVCA(0, 2), PVCA(0, 3), PVCA(0, 4), PVCA(0, 5),
		PVCA(0, 6), PVCA(0, 7), PVCA(0, 8), PVCA(0, 9), PVCA(0, 10), PVCA(0, 11),
		PVCA(0, 12), PVCA(0, 13), PVCA(0, 14), PVCA(0, 15), PVCA(0, 16), PVCA(0, 17),
		PVCA(0, 18), PVCA(0, 19), PVCA(0, 20), PVCA(0, 21), PVCA(0, 22), PVCA(0, 23),

		PCORE(0, ExtEffectsStartA) + 1,
		PCORE(0, ExtEffectsStartA),

		PREVB_REG(0, APF1_SIZE),
		PREVB_REG(0, APF2_SIZE),
		PREVB_REG(0, SAME_L_DST),
		PREVB_REG(0, SAME_R_DST),
		PREVB_REG(0, COMB1_L_SRC),
		PREVB_REG(0, COMB1_R_SRC),
		PREVB_REG(0, COMB2_L_SRC),
		PREVB_REG(0, COMB2_R_SRC),
		PREVB_REG(0, SAME_L_SRC),
		PREVB_REG(0, SAME_R_SRC),
		PREVB_REG(0, DIFF_L_DST),
		PREVB_REG(0, DIFF_R_DST),
		PREVB_REG(0, COMB3_L_SRC),
		PREVB_REG(0, COMB3_R_SRC),
		PREVB_REG(0, COMB4_L_SRC),
		PREVB_REG(0, COMB4_R_SRC),
		PREVB_REG(0, DIFF_L_SRC),
		PREVB_REG(0, DIFF_R_SRC),
		PREVB_REG(0, APF1_L_DST),
		PREVB_REG(0, APF1_R_DST),
		PREVB_REG(0, APF2_L_DST),
		PREVB_REG(0, APF2_R_DST),

		PCORE(0, ExtEffectsEndA) + 1,
		PCORE(0, ExtEffectsEndA),

		PCORE(0, Regs.ENDX),
		PCORE(0, Regs.ENDX) + 1,
		PCORE(0, Regs.STATX),

		//0x346 here
		PRAW(0x346),
		PRAW(0x348), PRAW(0x34A), PRAW(0x34C), PRAW(0x34E),
		PRAW(0x350), PRAW(0x352), PRAW(0x354), PRAW(0x356),
		PRAW(0x358), PRAW(0x35A), PRAW(0x35C), PRAW(0x35E),
		PRAW(0x360), PRAW(0x362), PRAW(0x364), PRAW(0x366),
		PRAW(0x368), PRAW(0x36A), PRAW(0x36C), PRAW(0x36E),
		PRAW(0x370), PRAW(0x372), PRAW(0x374), PRAW(0x376),
		PRAW(0x378), PRAW(0x37A), PRAW(0x37C), PRAW(0x37E),
		PRAW(0x380), PRAW(0x382), PRAW(0x384), PRAW(0x386),
		PRAW(0x388), PRAW(0x38A), PRAW(0x38C), PRAW(0x38E),
		PRAW(0x390), PRAW(0x392), PRAW(0x394), PRAW(0x396),
		PRAW(0x398), PRAW(0x39A), PRAW(0x39C), PRAW(0x39E),
		PRAW(0x3A0), PRAW(0x3A2), PRAW(0x3A4), PRAW(0x3A6),
		PRAW(0x3A8), PRAW(0x3AA), PRAW(0x3AC), PRAW(0x3AE),
		PRAW(0x3B0), PRAW(0x3B2), PRAW(0x3B4), PRAW(0x3B6),
		PRAW(0x3B8), PRAW(0x3BA), PRAW(0x3BC), PRAW(0x3BE),
		PRAW(0x3C0), PRAW(0x3C2), PRAW(0x3C4), PRAW(0x3C6),
		PRAW(0x3C8), PRAW(0x3CA), PRAW(0x3CC), PRAW(0x3CE),
		PRAW(0x3D0), PRAW(0x3D2), PRAW(0x3D4), PRAW(0x3D6),
		PRAW(0x3D8), PRAW(0x3DA), PRAW(0x3DC), PRAW(0x3DE),
		PRAW(0x3E0), PRAW(0x3E2), PRAW(0x3E4), PRAW(0x3E6),
		PRAW(0x3E8), PRAW(0x3EA), PRAW(0x3EC), PRAW(0x3EE),
		PRAW(0x3F0), PRAW(0x3F2), PRAW(0x3F4), PRAW(0x3F6),
		PRAW(0x3F8), PRAW(0x3FA), PRAW(0x3FC), PRAW(0x3FE),

		//AND... we reached 0x400!
		// Voice Params: 8 params, 24 voices = 0x180 bytes
		PVC(1, 0), PVC(1, 1), PVC(1, 2), PVC(1, 3), PVC(1, 4), PVC(1, 5),
		PVC(1, 6), PVC(1, 7), PVC(1, 8), PVC(1, 9), PVC(1, 10), PVC(1, 11),
		PVC(1, 12), PVC(1, 13), PVC(1, 14), PVC(1, 15), PVC(1, 16), PVC(1, 17),
		PVC(1, 18), PVC(1, 19), PVC(1, 20), PVC(1, 21), PVC(1, 22), PVC(1, 23),

		PCORE(1, Regs.PMON),
		PCORE(1, Regs.PMON) + 1,
		PCORE(1, Regs.NON),
		PCORE(1, Regs.NON) + 1,
		PCORE(1, Regs.VMIXL),
		PCORE(1, Regs.VMIXL) + 1,
		PCORE(1, Regs.VMIXEL),
		PCORE(1, Regs.VMIXEL) + 1,
		PCORE(1, Regs.VMIXR),
		PCORE(1, Regs.VMIXR) + 1,
		PCORE(1, Regs.VMIXER),
		PCORE(1, Regs.VMIXER) + 1,
		PCORE(1, Regs.MMIX),

		PCORE(1, Regs.ATTR),

		PCORE(1, IRQA) + 1,
		PCORE(1, IRQA),

		nullptr, nullptr,
		nullptr, nullptr,

		PCORE(1, TSA) + 1,
		PCORE(1, TSA),

		PRAW(0x5ac), PRAW(0x5ae),

		PCORE(1, AutoDMACtrl),

		PRAW(0x5b2), PRAW(0x5b4), PRAW(0x5b6), PRAW(0x5b8), PRAW(0x5ba), PRAW(0x5bc), PRAW(0x5be), // unknown

		// Voice Addresses
		PVCA(1, 0), PVCA(1, 1), PVCA(1, 2), PVCA(1, 3), PVCA(1, 4), PVCA(1, 5),
		PVCA(1, 6), PVCA(1, 7), PVCA(1, 8), PVCA(1, 9), PVCA(1, 10), PVCA(1, 11),
		PVCA(1, 12), PVCA(1, 13), PVCA(1, 14), PVCA(1, 15), PVCA(1, 16), PVCA(1, 17),
		PVCA(1, 18), PVCA(1, 19), PVCA(1, 20), PVCA(1, 21), PVCA(1, 22), PVCA(1, 23),

		PCORE(1, ExtEffectsStartA) + 1,
		PCORE(1, ExtEffectsStartA),

		PREVB_REG(1, APF1_SIZE),
		PREVB_REG(1, APF2_SIZE),
		PREVB_REG(1, SAME_L_DST),
		PREVB_REG(1, SAME_R_DST),
		PREVB_REG(1, COMB1_L_SRC),
		PREVB_REG(1, COMB1_R_SRC),
		PREVB_REG(1, COMB2_L_SRC),
		PREVB_REG(1, COMB2_R_SRC),
		PREVB_REG(1, SAME_L_SRC),
		PREVB_REG(1, SAME_R_SRC),
		PREVB_REG(1, DIFF_L_DST),
		PREVB_REG(1, DIFF_R_DST),
		PREVB_REG(1, COMB3_L_SRC),
		PREVB_REG(1, COMB3_R_SRC),
		PREVB_REG(1, COMB4_L_SRC),
		PREVB_REG(1, COMB4_R_SRC),
		PREVB_REG(1, DIFF_L_SRC),
		PREVB_REG(1, DIFF_R_SRC),
		PREVB_REG(1, APF1_L_DST),
		PREVB_REG(1, APF1_R_DST),
		PREVB_REG(1, APF2_L_DST),
		PREVB_REG(1, APF2_R_DST),

		PCORE(1, ExtEffectsEndA) + 1,
		PCORE(1, ExtEffectsEndA),

		PCORE(1, Regs.ENDX),
		PCORE(1, Regs.ENDX) + 1,
		PCORE(1, Regs.STATX),

		PRAW(0x746),
		PRAW(0x748), PRAW(0x74A), PRAW(0x74C), PRAW(0x74E),
		PRAW(0x750), PRAW(0x752), PRAW(0x754), PRAW(0x756),
		PRAW(0x758), PRAW(0x75A), PRAW(0x75C), PRAW(0x75E),

		//0x760: weird area
		PCORE(0, MasterVol.Left.Reg_VOL),
		PCORE(0, MasterVol.Right.Reg_VOL),
		PCORE(0, FxVol.Left) + 1,
		PCORE(0, FxVol.Right) + 1,
		PCORE(0, ExtVol.Left) + 1,
		PCORE(0, ExtVol.Right) + 1,
		PCORE(0, InpVol.Left) + 1,
		PCORE(0, InpVol.Right) + 1,
		PCORE(0, MasterVol.Left.Value) + 1,
		PCORE(0, MasterVol.Right.Value) + 1,
		PCORE(0, Revb.IIR_VOL),
		PCORE(0, Revb.COMB1_VOL),
		PCORE(0, Revb.COMB2_VOL),
		PCORE(0, Revb.COMB3_VOL),
		PCORE(0, Revb.COMB4_VOL),
		PCORE(0, Revb.WALL_VOL),
		PCORE(0, Revb.APF1_VOL),
		PCORE(0, Revb.APF2_VOL),
		PCORE(0, Revb.IN_COEF_L),
		PCORE(0, Revb.IN_COEF_R),

		PCORE(1, MasterVol.Left.Reg_VOL),
		PCORE(1, MasterVol.Right.Reg_VOL),
		PCORE(1, FxVol.Left) + 1,
		PCORE(1, FxVol.Right) + 1,
		PCORE(1, ExtVol.Left) + 1,
		PCORE(1, ExtVol.Right) + 1,
		PCORE(1, InpVol.Left) + 1,
		PCORE(1, InpVol.Right) + 1,
		PCORE(1, MasterVol.Left.Value) + 1,
		PCORE(1, MasterVol.Right.Value) + 1,
		PCORE(1, Revb.IIR_VOL),
		PCORE(1, Revb.COMB1_VOL),
		PCORE(1, Revb.COMB2_VOL),
		PCORE(1, Revb.COMB3_VOL),
		PCORE(1, Revb.COMB4_VOL),
		PCORE(1, Revb.WALL_VOL),
		PCORE(1, Revb.APF1_VOL),
		PCORE(1, Revb.APF2_VOL),
		PCORE(1, Revb.IN_COEF_L),
		PCORE(1, Revb.IN_COEF_R),

		PRAW(0x7B0), PRAW(0x7B2), PRAW(0x7B4), PRAW(0x7B6),
		PRAW(0x7B8), PRAW(0x7BA), PRAW(0x7BC), PRAW(0x7BE),

		// SPDIF interface
		U16P(Spdif.Out),
		U16P(Spdif.Info),
		U16P(Spdif.Unknown1),
		U16P(Spdif.Mode),
		U16P(Spdif.Media),
		U16P(Spdif.Unknown2),
		U16P(Spdif.Protection),

		PRAW(0x7CE),
		PRAW(0x7D0), PRAW(0x7D2), PRAW(0x7D4), PRAW(0x7D6),
		PRAW(0x7D8), PRAW(0x7DA), PRAW(0x7DC), PRAW(0x7DE),
		PRAW(0x7E0), PRAW(0x7E2), PRAW(0x7E4), PRAW(0x7E6),
		PRAW(0x7E8), PRAW(0x7EA), PRAW(0x7EC), PRAW(0x7EE),
		PRAW(0x7F0), PRAW(0x7F2), PRAW(0x7F4), PRAW(0x7F6),
		PRAW(0x7F8), PRAW(0x7FA), PRAW(0x7FC), PRAW(0x7FE),

		nullptr};

extern retro_audio_sample_t sample_cb;

static int SampleRate = 48000;

u32 lClocks = 0;

static void TimeUpdate(u32 cClocks)
{
	u32 dClocks = cClocks - lClocks;

	// Sanity Checks:
	//  It's not totally uncommon for the IOP's clock to jump backwards a cycle or two, and in
	//  such cases we just want to ignore the TimeUpdate call.

	if (dClocks > (u32)-15)
		return;

	//  But if for some reason our clock value seems way off base (typically due to bad dma
	//  timings from PCSX2), just mix out a little bit, skip the rest, and hope the ship
	//  "rights" itself later on.

	if (dClocks > (u32)(SPU2_TICK_INTERVAL * SPU2_SANITY_INTERVAL))
	{
		dClocks = SPU2_TICK_INTERVAL * SPU2_SANITY_INTERVAL;
		lClocks = cClocks - dClocks;
	}
	//Update Mixing Progress
	while (dClocks >= SPU2_TICK_INTERVAL)
	{
		if (has_to_call_irq)
		{
			has_to_call_irq = false;
			spu2Irq();
		}

		//Update DMA4 interrupt delay counter
		if (Cores[0].DMAICounter > 0)
		{
			Cores[0].DMAICounter -= SPU2_TICK_INTERVAL;
			if (Cores[0].DMAICounter <= 0)
			{
				if (Cores[0].IsDMARead)
					Cores[0].FinishDMAread();

				Cores[0].MADR = Cores[0].TADR;
				Cores[0].DMAICounter = 0;
				spu2DMA4Irq();
			}
			else
				Cores[0].MADR += SPU2_TICK_INTERVAL << 1;
		}

		//Update DMA7 interrupt delay counter
		if (Cores[1].DMAICounter > 0)
		{
			Cores[1].DMAICounter -= SPU2_TICK_INTERVAL;
			if (Cores[1].DMAICounter <= 0)
			{
				if (Cores[1].IsDMARead)
					Cores[1].FinishDMAread();

				Cores[1].MADR = Cores[1].TADR;
				Cores[1].DMAICounter = 0;
				spu2DMA7Irq();
			}
			else
				Cores[1].MADR += SPU2_TICK_INTERVAL << 1;
		}

		dClocks -= SPU2_TICK_INTERVAL;
		lClocks += SPU2_TICK_INTERVAL;
		Cycles++;

		SPU2_Mix();
	}
}


// --------------------------------------------------------------------------------------
//  DMA 4/7 Callbacks from Core Emulator
// --------------------------------------------------------------------------------------

u32 SPU2ReadMemAddr(int core)
{
	return Cores[core].MADR;
}

void SPU2WriteMemAddr(int core, u32 value)
{
	Cores[core].MADR = value;
}

void SPU2readDMA4Mem(u16* pMem, u32 size) // size now in 16bit units
{
	TimeUpdate(psxRegs.cycle);

	Cores[0].DoDMAread(pMem, size);
}

void SPU2writeDMA4Mem(u16* pMem, u32 size) // size now in 16bit units
{
	TimeUpdate(psxRegs.cycle);

	Cores[0].DoDMAwrite(pMem, size);
}

void SPU2interruptDMA4(void)
{
	Cores[0].Regs.STATX |= 0x80;
}

void SPU2interruptDMA7(void)
{
	Cores[1].Regs.STATX |= 0x80;
}

void SPU2readDMA7Mem(u16* pMem, u32 size)
{
	TimeUpdate(psxRegs.cycle);

	Cores[1].DoDMAread(pMem, size);
}

void SPU2writeDMA7Mem(u16* pMem, u32 size)
{
	TimeUpdate(psxRegs.cycle);

	Cores[1].DoDMAwrite(pMem, size);
}

s32 SPU2reset(void)
{
	/* TODO/FIXME - we would want to call
	   set_av_info here to set the audio samplerate */
	if (SampleRate != 48000)
		SampleRate = 48000;

	memset(spu2regs, 0, 0x010000);
	memset(_spu2mem, 0, 0x200000);
	memset(_spu2mem + 0x2800, 7, 0x10); // from BIOS reversal. Locks the voices so they don't run free.

	/* TODO/FIXME - this breaks Klonoa 2 right now and might
	 * break other games too, might need to enable this specifically
	 * for Mega Man X7 if no other way can be found */
	//memset(_spu2mem + 0xe870, 7, 0x10); // Loop which gets left over by the BIOS, Megaman X7 relies on it being there.
	Spdif.Info = 0; // Reset IRQ Status if it got set in a previously run game

	Cores[0].Init(0);
	Cores[1].Init(1);
	return 0;
}

s32 SPU2ps1reset(void)
{
	/* TODO/FIXME - we would want to call
	   set_av_info here to set the audio samplerate */
	if (SampleRate != 44100)
		SampleRate = 44100;

	return 0;
}

s32 SPU2init(void)
{
	spu2regs = (s16*)malloc(0x010000);
	_spu2mem = (s16*)malloc(0x200000);

	//  ADPCM decoder cache:
	//  the cache data size is determined by taking the number of adpcm blocks
	//  (2MB / 16) and multiplying it by the decoded block size (28 samples).
	//  Thus: pcm_cache_data = 7,340,032 bytes (ouch!)
	//  Expanded: 16 bytes expands to 56 bytes [3.5:1 ratio]
	//    Resulting in 2MB * 3.5.

	pcm_cache_data = (PcmCacheEntry*)calloc(PCM_BLOCK_COUNT, sizeof(PcmCacheEntry));

	if (!spu2regs || !_spu2mem || !pcm_cache_data)
		return -1;

	// Patch up a copy of regtable that directly maps "nullptrs" to SPU2 memory.

	memcpy(regtable, regtable_original, sizeof(regtable));

	for (uint mem = 0; mem < 0x800; mem++)
	{
		u16* ptr = regtable[mem >> 1];
		if (!ptr)
			regtable[mem >> 1] = &(spu2Ru16(mem));
	}

	SPU2reset();
	InitADSR();

	return 0;
}

s32 SPU2open(void)
{
	lClocks  = psxRegs.cycle;

	return 0;
}

void SPU2close(void)
{
	has_to_call_irq = false;
}

void SPU2shutdown(void)
{
	SPU2close();

	safe_free(spu2regs);
	safe_free(_spu2mem);
	safe_free(pcm_cache_data);
}

void SPU2async(u32 cycles)
{
	TimeUpdate(psxRegs.cycle);
}

u16 SPU2read(u32 rmem)
{
	u32 core = 0, mem = rmem & 0xFFFF, omem = mem;
	if (mem & 0x400)
	{
		omem ^= 0x400;
		core = 1;
	}

	if (omem == 0x1f9001AC)
		return Cores[core].DmaRead();

	TimeUpdate(psxRegs.cycle);

	if (rmem >> 16 == 0x1f80)
		return Cores[0].ReadRegPS1(rmem);
	else if (mem >= 0x800)
		return spu2Ru16(mem);
	return *(regtable[(mem >> 1)]);
}

void SPU2write(u32 rmem, u16 value)
{
	// Note: Reverb/Effects are very sensitive to having precise update timings.
	// If the SPU2 isn't in in sync with the IOP, samples can end up playing at rather
	// incorrect pitches and loop lengths.

	TimeUpdate(psxRegs.cycle);

	if (rmem >> 16 == 0x1f80)
		Cores[0].WriteRegPS1(rmem, value);
	else
		tbl_reg_writes[(rmem & 0x7ff) / 2](value);
}

s32 SPU2freeze(int mode, freezeData* data)
{
	if (!data)
		return -1;

	if (mode == FREEZE_SIZE)
	{
		data->size = SPU2Savestate::SizeIt();
		return 0;
	}

	if (data->data == nullptr)
		return -1;

	SPU2Savestate::DataBlock& spud = (SPU2Savestate::DataBlock&)*(data->data);

	switch (mode)
	{
		case FREEZE_LOAD:
			return SPU2Savestate::ThawIt(spud);
		case FREEZE_SAVE:
			return SPU2Savestate::FreezeIt(spud);
		default:
			break;
	}

	// technically unreachable, but kills a warning:
	return 0;
}

void SPU2DoFreezeOut(void* dest)
{
	freezeData fP = {0, (s8*)dest};
	if (SPU2freeze(FREEZE_SIZE, &fP) != 0)
		return;
	if (!fP.size)
		return;
	SPU2freeze(FREEZE_SAVE, &fP);
}


void SPU2DoFreezeIn(pxInputStream& infp)
{
	freezeData fP = {0, nullptr};
	if (SPU2freeze(FREEZE_SIZE, &fP) != 0)
		fP.size = 0;

	// no state data to read, but SPU2 expects some state data?
	// Issue a warning to console...
	if (!infp.IsOk() || !infp.Length())
		return;

	ScopedAlloc<s8> data(fP.size);
	fP.data = data.GetPtr();

	infp.Read(fP.data, fP.size);
	SPU2freeze(FREEZE_LOAD, &fP);
}
