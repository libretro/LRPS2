/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "Utilities/FixedPointTypes.h"
#include "x86emitter/tools.h"

#include <wx/filename.h>

enum GamefixId
{
	GamefixId_FIRST = 0,

	Fix_VuAddSub = GamefixId_FIRST,
	Fix_FpuCompare,
	Fix_FpuMultiply,
	Fix_FpuNegDiv,
	Fix_XGKick,
	Fix_IpuWait,
	Fix_EETiming,
	Fix_SkipMpeg,
	Fix_OPHFlag,
	Fix_DMABusy,
	Fix_VIFFIFO,
	Fix_VIF1Stall,
	Fix_GIFFIFO,
	Fix_FMVinSoftware,
	Fix_GoemonTlbMiss,
	Fix_Ibit,
	Fix_VUKickstart,

	GamefixId_COUNT
};

enum SpeedhackId
{
	SpeedhackId_FIRST = 0,

	Speedhack_mvuFlag = SpeedhackId_FIRST,
	Speedhack_InstantVU1,

	SpeedhackId_COUNT
};

// Template function for casting enumerations to their underlying type
template <typename Enumeration>
typename std::underlying_type<Enumeration>::type enum_cast(Enumeration E)
{
	return static_cast<typename std::underlying_type<Enumeration>::type>(E);
}

ImplementEnumOperators( GamefixId );
ImplementEnumOperators( SpeedhackId );

// Macro used for removing some of the redtape involved in defining bitfield/union helpers.
//
#define BITFIELD32() \
    union            \
    {                \
        u32 bitset;  \
        struct       \
        {

#define BITFIELD_END \
    }                \
    ;                \
    }                \
    ;


// This macro is actually useful for about any and every possible application of C++
// equality operators.
#define OpEqu(field) (field == right.field)

//------------ DEFAULT sseMXCSR VALUES ---------------
#define DEFAULT_sseMXCSR	0xffc0 //FPU rounding > DaZ, FtZ, "chop"
#define DEFAULT_sseVUMXCSR	0xffc0 //VU  rounding > DaZ, FtZ, "chop"

// --------------------------------------------------------------------------------------
//  Pcsx2Config class
// --------------------------------------------------------------------------------------
// This is intended to be a public class library between the core emulator and GUI only.
// It is *not* meant to be shared data between core emulation and plugins, due to issues
// with version incompatibilities if the structure formats are changed.
//
// When GUI code performs modifications of this class, it must be done with strict thread
// safety, since the emu runs on a separate thread.  Additionally many components of the
// class require special emu-side resets or state save/recovery to be applied.  Please
// use the provided functions to lock the emulation into a safe state and then apply
// chances on the necessary scope (see Core_Pause, Core_ApplySettings, and Core_Resume).
//
struct Pcsx2Config
{
	// ------------------------------------------------------------------------
	struct RecompilerOptions
	{
		BITFIELD32()
			bool
				EnableEE		:1,
				EnableIOP		:1,
				EnableVU0		:1,
				EnableVU1		:1;

			bool
				vuOverflow		:1,
				vuExtraOverflow	:1,
				vuSignOverflow	:1,
				vuUnderflow		:1;

			bool
				fpuOverflow		:1,
				fpuExtraOverflow:1,
				fpuFullMode		:1;

		BITFIELD_END

		RecompilerOptions();

		bool operator ==( const RecompilerOptions& right ) const
		{
			return OpEqu( bitset );
		}

		bool operator !=( const RecompilerOptions& right ) const
		{
			return !OpEqu( bitset );
		}

	};

	// ------------------------------------------------------------------------
	struct CpuOptions
	{
		RecompilerOptions Recompiler;

		SSE_MXCSR sseMXCSR;
		SSE_MXCSR sseVUMXCSR;

		CpuOptions();

		bool operator ==( const CpuOptions& right ) const
		{
			return OpEqu( sseMXCSR ) && OpEqu( sseVUMXCSR ) && OpEqu( Recompiler );
		}

		bool operator !=( const CpuOptions& right ) const
		{
			return !this->operator ==( right );
		}
	};

	// ------------------------------------------------------------------------
	struct GSOptions
	{
		int		VsyncQueueSize;

		GSOptions();

		bool operator ==( const GSOptions& right ) const
		{
			return OpEqu( VsyncQueueSize );
		}

		bool operator !=( const GSOptions& right ) const
		{
			return !this->operator ==( right );
		}

	};

	// ------------------------------------------------------------------------
	// NOTE: The GUI's GameFixes panel is dependent on the order of bits in this structure.
	struct GamefixOptions
	{
        BITFIELD32()
        bool
            VuAddSubHack : 1,           // Tri-ace games, they use an encryption algorithm that requires VU ADDI opcode to be bit-accurate.
            FpuCompareHack : 1,         // Digimon Rumble Arena 2, fixes spinning/hanging on intro-menu.
            FpuMulHack : 1,             // Tales of Destiny hangs.
            FpuNegDivHack : 1,          // Gundam games messed up camera-view.
            XgKickHack : 1,             // Erementar Gerad, adds more delay to VU XGkick instructions. Corrects the color of some graphics, but breaks Tri-ace games and others.
            IPUWaitHack : 1,            // FFX FMV, makes GIF flush before doing IPU work. Fixes bad graphics overlay.
            EETimingHack : 1,           // General purpose timing hack.
            SkipMPEGHack : 1,           // Skips MPEG videos (Katamari and other games need this)
            OPHFlagHack : 1,            // Bleach Blade Battlers
            DMABusyHack : 1,            // Denies writes to the DMAC when it's busy. This is correct behaviour but bad timing can cause problems.
            VIFFIFOHack : 1,            // Pretends to fill the non-existant VIF FIFO Buffer.
            VIF1StallHack : 1,          // Like above, processes FIFO data before the stall is allowed (to make sure data goes over).
            GIFFIFOHack : 1,            // Enabled the GIF FIFO (more correct but slower)
            FMVinSoftwareHack : 1,      // Toggle in and out of software rendering when an FMV runs.
            GoemonTlbHack : 1,          // Gomeon tlb miss hack. The game need to access unmapped virtual address. Instead to handle it as exception, tlb are preloaded at startup
            IbitHack : 1,           	// I bit hack. Needed to stop constant VU recompilation
            VUKickstartHack : 1;       // Gives new VU programs a slight head start and runs VU's ahead of EE to avoid VU register reading/writing issues
		BITFIELD_END

		GamefixOptions();
		GamefixOptions& DisableAll();

		void Set( const wxString& list, bool enabled=true );
		void Clear( const wxString& list ) { Set( list, false ); }

		bool Get( GamefixId id ) const;
		void Set( GamefixId id, bool enabled=true );
		void Clear( GamefixId id ) { Set( id, false ); }

		bool operator ==( const GamefixOptions& right ) const
		{
			return OpEqu( bitset );
		}

		bool operator !=( const GamefixOptions& right ) const
		{
			return !OpEqu( bitset );
		}
	};

	// ------------------------------------------------------------------------
	struct SpeedhackOptions
	{
		BITFIELD32()
			bool
				fastCDVD		:1,		// enables fast CDVD access
				IntcStat		:1,		// tells Pcsx2 to fast-forward through intc_stat waits.
				WaitLoop		:1,		// enables constant loop detection and fast-forwarding
				vuFlagHack		:1,		// microVU specific flag hack
				vuThread : 1,		// Enable Threaded VU1
				vu1Instant : 1;		// Enable Instant VU1 (Without MTVU only)
		BITFIELD_END

		s8	EECycleRate;		// EE cycle rate selector (1.0, 1.5, 2.0)
		u8	EECycleSkip;		// EE Cycle skip factor (0, 1, 2, or 3)

		SpeedhackOptions();
		SpeedhackOptions& DisableAll();

		void Set(SpeedhackId id, bool enabled = true);

		bool operator ==( const SpeedhackOptions& right ) const
		{
			return OpEqu( bitset ) && OpEqu( EECycleRate ) && OpEqu( EECycleSkip );
		}

		bool operator !=( const SpeedhackOptions& right ) const
		{
			return !this->operator ==( right );
		}
	};

	BITFIELD32()
		bool
			CdvdShareWrite		:1,		// allows the iso to be modified while it's loaded
			EnablePatches		:1,		// enables patch detection and application
			EnableCheats		:1,		// enables cheat detection and application
			EnableWideScreenPatches		:1,
			EnableNointerlacingPatches	:1,
                        Enable60fpsPatches :1,
		// when enabled uses BOOT2 injection, skipping sony bios splashes
			UseBOOT2Injection	:1,
		// enables simulated ejection of memory cards when loading savestates
			McdEnableEjection	:1,
			McdFolderAutoManage	:1,

			MultitapPort0_Enabled:1,
			MultitapPort1_Enabled:1,

			HostFs				:1;
	BITFIELD_END

	CpuOptions			Cpu;
	GSOptions			GS;
	SpeedhackOptions	Speedhacks;
	GamefixOptions		Gamefixes;

	wxFileName			BiosFilename;

	Pcsx2Config();

	bool MultitapEnabled( uint port ) const;

	bool operator ==( const Pcsx2Config& right ) const
	{
		return
			OpEqu( bitset )		&&
			OpEqu( Cpu )		&&
			OpEqu( GS )			&&
			OpEqu( Speedhacks )	&&
			OpEqu( Gamefixes )	&&
			OpEqu( BiosFilename );
	}

	bool operator !=( const Pcsx2Config& right ) const
	{
		return !this->operator ==( right );
	}
};

extern Pcsx2Config EmuConfig;

/////////////////////////////////////////////////////////////////////////////////////////
// Helper Macros for Reading Emu Configurations.
//

// ------------ CPU / Recompiler Options ---------------

#define THREAD_VU1					(EmuConfig.Cpu.Recompiler.EnableVU1 && EmuConfig.Speedhacks.vuThread)
#define INSTANT_VU1					(EmuConfig.Speedhacks.vu1Instant)
#define CHECK_EEREC					(EmuConfig.Cpu.Recompiler.EnableEE)
#define CHECK_IOPREC					(EmuConfig.Cpu.Recompiler.EnableIOP)

//------------ SPECIAL GAME FIXES!!! ---------------
#define CHECK_VUADDSUBHACK			(EmuConfig.Gamefixes.VuAddSubHack)	 // Special Fix for Tri-ace games, they use an encryption algorithm that requires VU addi opcode to be bit-accurate.
#define CHECK_FPUCOMPAREHACK			(EmuConfig.Gamefixes.FpuCompareHack) // Special Fix for Digimon Rumble Arena 2, fixes spinning/hanging on intro-menu.
#define CHECK_FPUMULHACK			(EmuConfig.Gamefixes.FpuMulHack)	 // Special Fix for Tales of Destiny hangs.
#define CHECK_FPUNEGDIVHACK			(EmuConfig.Gamefixes.FpuNegDivHack)	 // Special Fix for Gundam games messed up camera-view.
#define CHECK_XGKICKHACK			(EmuConfig.Gamefixes.XgKickHack)	 // Special Fix for Erementar Gerad, adds more delay to VU XGkick instructions. Corrects the color of some graphics.
#define CHECK_IPUWAITHACK			(EmuConfig.Gamefixes.IPUWaitHack)	 // Special Fix For FFX
#define CHECK_EETIMINGHACK			(EmuConfig.Gamefixes.EETimingHack)	 // Fix all scheduled events to happen in 1 cycle.
#define CHECK_SKIPMPEGHACK			(EmuConfig.Gamefixes.SkipMPEGHack)	 // Finds sceMpegIsEnd pattern to tell the game the mpeg is finished (Katamari and a lot of games need this)
#define CHECK_OPHFLAGHACK			(EmuConfig.Gamefixes.OPHFlagHack)	 // Bleach Blade Battlers
#define CHECK_DMABUSYHACK			(EmuConfig.Gamefixes.DMABusyHack)    // Denies writes to the DMAC when it's busy. This is correct behaviour but bad timing can cause problems.
#define CHECK_VIFFIFOHACK			(EmuConfig.Gamefixes.VIFFIFOHack)    // Pretends to fill the non-existant VIF FIFO Buffer.
#define CHECK_VIF1STALLHACK			(EmuConfig.Gamefixes.VIF1StallHack)  // Like above, processes FIFO data before the stall is allowed (to make sure data goes over).
#define CHECK_GIFFIFOHACK			(EmuConfig.Gamefixes.GIFFIFOHack)	 // Enabled the GIF FIFO (more correct but slower)
#define CHECK_FMVINSOFTWAREHACK	 		(EmuConfig.Gamefixes.FMVinSoftwareHack) // Toggle in and out of software rendering when an FMV runs.
//------------ Advanced Options!!! ---------------
#define CHECK_VU_OVERFLOW			(EmuConfig.Cpu.Recompiler.vuOverflow)
#define CHECK_VU_EXTRA_OVERFLOW			(EmuConfig.Cpu.Recompiler.vuExtraOverflow) // If enabled, Operands are clamped before being used in the VU recs
#define CHECK_VU_SIGN_OVERFLOW			(EmuConfig.Cpu.Recompiler.vuSignOverflow)
#define CHECK_VU_UNDERFLOW			(EmuConfig.Cpu.Recompiler.vuUnderflow)

#define CHECK_FPU_OVERFLOW			(EmuConfig.Cpu.Recompiler.fpuOverflow)
#define CHECK_FPU_EXTRA_OVERFLOW		(EmuConfig.Cpu.Recompiler.fpuExtraOverflow) // If enabled, Operands are checked for infinities before being used in the FPU recs
#define CHECK_FPU_FULL				(EmuConfig.Cpu.Recompiler.fpuFullMode)

//------------ EE Recompiler defines - Comment to disable a recompiler ---------------

#define SHIFT_RECOMPILE		// Speed majorly reduced if disabled
#define BRANCH_RECOMPILE	// Speed extremely reduced if disabled - more then shift

// Disabling all the recompilers in this block is interesting, as it still runs at a reasonable rate.
// It also adds a few glitches. Really reminds me of the old Linux 64-bit version. --arcum42
#define ARITHMETICIMM_RECOMPILE
#define ARITHMETIC_RECOMPILE
#define MULTDIV_RECOMPILE
#define JUMP_RECOMPILE
#define LOADSTORE_RECOMPILE
#define MOVE_RECOMPILE
#define MMI_RECOMPILE
#define MMI0_RECOMPILE
#define MMI1_RECOMPILE
#define MMI2_RECOMPILE
#define MMI3_RECOMPILE
#define FPU_RECOMPILE
#define CP0_RECOMPILE
#define CP2_RECOMPILE

// You can't recompile ARITHMETICIMM without ARITHMETIC.
#ifndef ARITHMETIC_RECOMPILE
#undef ARITHMETICIMM_RECOMPILE
#endif
