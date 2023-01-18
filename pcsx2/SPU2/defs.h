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

#pragma once

#include "Mixer.h"
#include "Global.h"

#ifdef __GNUC__
#ifdef NDEBUG
#define SPU2_FORCEINLINE __attribute__((always_inline, unused))
#else
#define SPU2_FORCEINLINE __attribute__((unused))
#endif
#else
#ifndef SPU2_FORCEINLINE
#define SPU2_FORCEINLINE __forceinline
#endif
#endif

// --------------------------------------------------------------------------------------
//  SPU2 Memory Indexers
// --------------------------------------------------------------------------------------

#define spu2Rs16(mmem) (*(s16*)((s8*)spu2regs + ((mmem)&0x1fff)))
#define spu2Ru16(mmem) (*(u16*)((s8*)spu2regs + ((mmem)&0x1fff)))

extern s16* spu2regs;
extern s16* _spu2mem;
#define GETMEMPTR(addr) ((_spu2mem) + (addr))
#define GetMemPtr(addr) (GETMEMPTR(addr))

#define VOLFLAG_REVERSE_PHASE (1ul << 0)
#define VOLFLAG_DECREMENT (1ul << 1)
#define VOLFLAG_EXPONENTIAL (1ul << 2)
#define VOLFLAG_SLIDE_ENABLE (1ul << 3)

extern void spu2M_Write(u32 addr, s16 value);

struct V_VolumeLR
{
	static V_VolumeLR Max;

	s32 Left;
	s32 Right;

	V_VolumeLR() = default;
	V_VolumeLR(s32 both)
		: Left(both)
		, Right(both)
	{
	}
};

struct V_VolumeSlide
{
	// Holds the "original" value of the volume for this voice, prior to slides.
	// (ie, the volume as written to the register)

	s16 Reg_VOL;
	s32 Value;
	s8 Increment;
	s8 Mode;

public:
	V_VolumeSlide() = default;
	V_VolumeSlide(s16 regval, s32 fullvol)
		: Reg_VOL(regval)
		, Value(fullvol)
		, Increment(0)
		, Mode(0)
	{
	}
	void Update();
};

struct V_VolumeSlideLR
{
	static V_VolumeSlideLR Max;

	V_VolumeSlide Left;
	V_VolumeSlide Right;

public:
	V_VolumeSlideLR() = default;
	V_VolumeSlideLR(s16 regval, s32 bothval)
		: Left(regval, bothval)
		, Right(regval, bothval)
	{
	}
};

struct V_ADSR
{
	union
	{
		u32 reg32;

		struct
		{
			u16 regADSR1;
			u16 regADSR2;
		};

		struct
		{
			u32 SustainLevel : 4,
				DecayRate : 4,
				AttackRate : 7,
				AttackMode : 1, // 0 for linear (+lin), 1 for pseudo exponential (+exp)

				ReleaseRate : 5,
				ReleaseMode : 1, // 0 for linear (-lin), 1 for exponential (-exp)
				SustainRate : 7,
				SustainMode : 3; // 0 = +lin, 1 = -lin, 2 = +exp, 3 = -exp
		};
	};

	s32 Value;      // Ranges from 0 to 0x7fffffff (signed values are clamped to 0) [Reg_ENVX]
	u8 Phase;       // monitors current phase of ADSR envelope
	bool Releasing; // Ready To Release, triggered by Voice.Stop();

public:
	bool Calculate();
};


struct V_Voice
{
	u32 PlayCycle; // SPU2 cycle where the Playing started

	V_VolumeSlideLR Volume;

	// Envelope
	V_ADSR ADSR;
	// Pitch (also Reg_PITCH)
	u16 Pitch;
	// Loop Start address (also Reg_LSAH/L)
	u32 LoopStartA;
	// Sound Start address (also Reg_SSAH/L)
	u32 StartA;
	// Next Read Data address (also Reg_NAXH/L)
	u32 NextA;
	// Voice Decoding State
	s32 Prev1;
	s32 Prev2;

	// Pitch Modulated by previous voice
	bool Modulated;
	// Source (Wave/Noise)
	bool Noise;

	s8 LoopMode;
	s8 LoopFlags;

	// Sample pointer (19:12 bit fixed point)
	s32 SP;

	// Sample pointer for Cubic Interpolation
	// Cubic interpolation mixes a sample behind Linear, so that it
	// can have sample data to either side of the end points from which
	// to extrapolate.  This SP represents that late sample position.
	s32 SPc;

	// Previous sample values - used for interpolation
	// Inverted order of these members to match the access order in the
	//   code (might improve cache hits).
	s32 PV4;
	s32 PV3;
	s32 PV2;
	s32 PV1;

	// Last outputted audio value, used for voice modulation.
	s32 OutX;
	s32 NextCrest; // temp value for Crest calculation

	// SBuffer now points directly to an ADPCM cache entry.
	s16* SBuffer;

	// sample position within the current decoded packet.
	s32 SCurrent;

	// it takes a few ticks for voices to start on the real SPU2?
	void Start();
	void Stop();
};

struct V_Reverb
{
	s16 IN_COEF_L;
	s16 IN_COEF_R;

	u32 APF1_SIZE;
	u32 APF2_SIZE;

	s16 APF1_VOL;
	s16 APF2_VOL;

	u32 SAME_L_SRC;
	u32 SAME_R_SRC;
	u32 DIFF_L_SRC;
	u32 DIFF_R_SRC;
	u32 SAME_L_DST;
	u32 SAME_R_DST;
	u32 DIFF_L_DST;
	u32 DIFF_R_DST;

	s16 IIR_VOL;
	s16 WALL_VOL;

	u32 COMB1_L_SRC;
	u32 COMB1_R_SRC;
	u32 COMB2_L_SRC;
	u32 COMB2_R_SRC;
	u32 COMB3_L_SRC;
	u32 COMB3_R_SRC;
	u32 COMB4_L_SRC;
	u32 COMB4_R_SRC;

	s16 COMB1_VOL;
	s16 COMB2_VOL;
	s16 COMB3_VOL;
	s16 COMB4_VOL;

	u32 APF1_L_DST;
	u32 APF1_R_DST;
	u32 APF2_L_DST;
	u32 APF2_R_DST;
};

struct V_ReverbBuffers
{
	s32 SAME_L_SRC;
	s32 SAME_R_SRC;
	s32 DIFF_R_SRC;
	s32 DIFF_L_SRC;
	s32 SAME_L_DST;
	s32 SAME_R_DST;
	s32 DIFF_L_DST;
	s32 DIFF_R_DST;

	s32 COMB1_L_SRC;
	s32 COMB1_R_SRC;
	s32 COMB2_L_SRC;
	s32 COMB2_R_SRC;
	s32 COMB3_L_SRC;
	s32 COMB3_R_SRC;
	s32 COMB4_L_SRC;
	s32 COMB4_R_SRC;

	s32 APF1_L_DST;
	s32 APF1_R_DST;
	s32 APF2_L_DST;
	s32 APF2_R_DST;

	s32 SAME_L_PRV;
	s32 SAME_R_PRV;
	s32 DIFF_L_PRV;
	s32 DIFF_R_PRV;

	s32 APF1_L_SRC;
	s32 APF1_R_SRC;
	s32 APF2_L_SRC;
	s32 APF2_R_SRC;

	bool NeedsUpdated;
};

struct V_SPDIF
{
	u16 Out;
	u16 Info;
	u16 Unknown1;
	u16 Mode;
	u16 Media;
	u16 Unknown2;
	u16 Protection;
};

struct V_CoreRegs
{
	u32 PMON;
	u32 NON;
	u32 VMIXL;
	u32 VMIXR;
	u32 VMIXEL;
	u32 VMIXER;
	u32 ENDX;

	u16 MMIX;
	u16 STATX;
	u16 ATTR;
	u16 _1AC;
};

struct V_VoiceGates
{
	s16 DryL; // 'AND Gate' for Direct Output to Left Channel
	s16 DryR; // 'AND Gate' for Direct Output for Right Channel
	s16 WetL; // 'AND Gate' for Effect Output for Left Channel
	s16 WetR; // 'AND Gate' for Effect Output for Right Channel
};

struct V_CoreGates
{
	union
	{
		u128 v128;

		struct
		{
			s16 InpL; // Sound Data Input to Direct Output (Left)
			s16 InpR; // Sound Data Input to Direct Output (Right)
			s16 SndL; // Voice Data to Direct Output (Left)
			s16 SndR; // Voice Data to Direct Output (Right)
			s16 ExtL; // External Input to Direct Output (Left)
			s16 ExtR; // External Input to Direct Output (Right)
		};
	};
};

struct VoiceMixSet
{
	static const VoiceMixSet Empty;
	StereoOut32 Dry, Wet;

	VoiceMixSet() {}
	VoiceMixSet(const StereoOut32& dry, const StereoOut32& wet)
		: Dry(dry)
		, Wet(wet)
	{
	}
};

#define NUM_VOICES 24

struct V_Core
{
	u32 Index; // Core index identifier.

	// Voice Gates -- These are SSE-related values, and must always be
	// first to ensure 16 byte alignment

	V_VoiceGates VoiceGates[NUM_VOICES];
	V_CoreGates DryGate;
	V_CoreGates WetGate;

	V_VolumeSlideLR MasterVol; // Master Volume
	V_VolumeLR ExtVol;         // Volume for External Data Input
	V_VolumeLR InpVol;         // Volume for Sound Data Input
	V_VolumeLR FxVol;          // Volume for Output from Effects

	V_Voice Voices[NUM_VOICES];

	u32 IRQA; // Interrupt Address
	u32 TSA;  // DMA Transfer Start Address

	bool IRQEnable; // Interrupt Enable
	bool FxEnable;  // Effect Enable
	bool Mute;      // Mute
	bool AdmaInProgress;

	s8 DMABits;        // DMA related?
	s8 NoiseClk;       // Noise Clock
	u16 AutoDMACtrl;   // AutoDMA Status
	s32 DMAICounter;   // DMA Interrupt Counter
	u32 InputDataLeft; // Input Buffer
	u32 InputPosRead;
	u32 InputPosWrite;
	u32 InputDataProgress;

	V_Reverb Revb;              // Reverb Registers
	V_ReverbBuffers RevBuffers; // buffer pointers for reverb, pre-calculated and pre-clipped.
	u32 EffectsStartA;
	u32 EffectsEndA;
	u32 ExtEffectsStartA;
	u32 ExtEffectsEndA;
	u32 ReverbX;

	// Current size of and position of the effects buffer.  Pre-caculated when the effects start
	// or end position registers are written.  CAN BE NEGATIVE OR ZERO, in which
	// case reverb should be disabled.
	s32 EffectsBufferSize;
	u32 EffectsBufferStart;

	V_CoreRegs Regs; // Registers

	// Preserves the channel processed last cycle
	StereoOut32 LastEffect;

	u8 CoreEnabled;

	u8 AttrBit0;
	u8 DmaMode;

	u16* DMAPtr;
	u16* DMARPtr; // Mem pointer for DMA Reads
	u32 ReadSize;
	bool IsDMARead;
	u32 MADR;
	u32 TADR;

	u32 KeyOn; // not the KON register (though maybe it is)

	// psxmode caches
	u16 psxSoundDataTransferControl;

	// ----------------------------------------------------------------------------------
	//  V_Core Methods
	// ----------------------------------------------------------------------------------

	// uninitialized constructor
	V_Core()
		: Index(-1)
		, DMAPtr(nullptr)
	{
	}
	V_Core(int idx); // our badass constructor
	~V_Core();

	void Init(int index);
	void UpdateEffectsBufferSize();

	s32 EffectsBufferIndexer(s32 offset) const;

	void WriteRegPS1(u32 mem, u16 value);
	u16 ReadRegPS1(u32 mem);

	// --------------------------------------------------------------------------------------
	//  Mixer Section
	// --------------------------------------------------------------------------------------

	StereoOut32 Mix(const VoiceMixSet& inVoices, const StereoOut32& Input, const StereoOut32& Ext);
	void Reverb_AdvanceBuffer();
	StereoOut32 DoReverb(const StereoOut32& Input);
	s32 RevbGetIndexer(s32 offset);

	StereoOut32 ReadInput();
	StereoOut32 ReadInput_HiFi();

	// --------------------------------------------------------------------------
	//  DMA Section
	// --------------------------------------------------------------------------
	SPU2_FORCEINLINE u16 DmaRead(void)
	{
		u32 _addr     = TSA & 0xfffff;
		const u16 ret = (u16)*GETMEMPTR(_addr);
		++TSA;
		TSA &= 0xfffff;
		return ret;
	}

	SPU2_FORCEINLINE void DmaWrite(u16 value)
	{
		spu2M_Write(TSA, (s16)value);
		++TSA;
		TSA &= 0xfffff;
	}

	void DoDMAwrite(u16* pMem, u32 size);
	void DoDMAread(u16* pMem, u32 size);
	void FinishDMAread();

	void AutoDMAReadBuffer(int mode);
	void StartADMAWrite(u16* pMem, u32 sz);
	void PlainDMAWrite(u16* pMem, u32 sz);
};

extern V_Core Cores[2];
extern V_SPDIF Spdif;

// Output Buffer Writing Position (the same for all data);
extern s16 OutPos;
// Input Buffer Reading Position (the same for all data);
extern s16 InputPos;
// SPU Mixing Cycles ("Ticks mixed" counter)
extern u32 Cycles;

extern int PlayMode;

extern void SetIrqCall(int core);
extern void InitADSR();

namespace SPU2Savestate
{
	struct DataBlock;

	extern s32 FreezeIt(DataBlock& spud);
	extern s32 ThawIt(DataBlock& spud);
	extern s32 SizeIt(void);
} // namespace SPU2Savestate
 

#define SPU2_TICK_INTERVAL 768
#define SPU2_SANITY_INTERVAL 4800

// --------------------------------------------------------------------------------------
//  ADPCM Decoder Cache
// --------------------------------------------------------------------------------------

// The SPU2 has a dynamic memory range which is used for several internal operations, such as
// registers, CORE 1/2 mixing, AutoDMAs, and some other fancy stuff.  We exclude this range
// from the cache here:
#define SPU2_DYN_MEMLINE 0x2800

// 8 short words per encoded PCM block. (as stored in SPU2 ram)
#define PCM_WORDS_PER_BLOCK 8

// number of cachable ADPCM blocks (any blocks above the SPU2_DYN_MEMLINE)
// formula: 0x100000 / PCM_WORDS_PER_BLOCK
#define PCM_BLOCK_COUNT 0x20000

// 28 samples per decoded PCM block (as stored in our cache)
#define PCM_DECODED_SAMPLES_PER_BLOCK 28

struct PcmCacheEntry
{
	bool Validated;
	s16 Sampledata[PCM_DECODED_SAMPLES_PER_BLOCK];
};

extern PcmCacheEntry* pcm_cache_data;
extern bool has_to_call_irq;

typedef void RegWriteHandler(u16 value);
extern RegWriteHandler* const tbl_reg_writes[0x401];
