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
#include "Utilities/FixedPointTypes.inl"

#include "Config.h"
#include "GS.h"
#include "EmuOptionsVM.h"

const wxChar* const tbl_SpeedhackNames[] =
{
	L"mvuFlag",
	L"InstantVU1" };

const __fi wxChar* EnumToString(SpeedhackId id)
{
	return tbl_SpeedhackNames[id];
}

void Pcsx2Config::SpeedhackOptions::Set(SpeedhackId id, bool enabled)
{
	EnumAssert(id);
	switch (id)
	{
	case Speedhack_mvuFlag:
		vuFlagHack = enabled;
		break;
	case Speedhack_InstantVU1:
		vu1Instant = enabled;
		break;
		jNO_DEFAULT;
	}
}

Pcsx2Config::SpeedhackOptions::SpeedhackOptions()
{
	DisableAll();
	
	// Set recommended speedhacks to enabled by default. They'll still be off globally on resets.
	WaitLoop = true;
	IntcStat = true;
	vuFlagHack = true;
	vu1Instant = true;
}

Pcsx2Config::SpeedhackOptions& Pcsx2Config::SpeedhackOptions::DisableAll()
{
	bitset			= 0;
	EECycleRate		= 0;
	EECycleSkip		= 0;
	
	return *this;
}

// NEW MEM OPTIONS
void Pcsx2Config::SpeedhackOptions::LoadSave()
{
	EECycleRate = PCSX2_vm::EECycleRate;
	EECycleSkip = PCSX2_vm::EECycleSkip;
	IntcStat = PCSX2_vm::IntcStat;
	WaitLoop = PCSX2_vm::WaitLoop;
	vuFlagHack = PCSX2_vm::vuFlagHack;
	vuThread = PCSX2_vm::vuThread;
	vu1Instant = PCSX2_vm::vu1Instant;
}

Pcsx2Config::RecompilerOptions::RecompilerOptions()
{
	bitset		= 0;

	// All recs are enabled by default.

	EnableEE	= true;
	EnableIOP	= true;
	EnableVU0	= true;
	EnableVU1	= true;

	UseMicroVU0	= true;
	UseMicroVU1	= true;

	// vu and fpu clamping default to standard overflow.
	vuOverflow	= true;
	//vuExtraOverflow = false;
	//vuSignOverflow = false;
	//vuUnderflow = false;

	fpuOverflow	= true;
	//fpuExtraOverflow = false;
	//fpuFullMode = false;
}

void Pcsx2Config::RecompilerOptions::ApplySanityCheck()
{
	bool fpuIsRight = true;

	if( fpuExtraOverflow )
		fpuIsRight = fpuOverflow;

	if( fpuFullMode )
		fpuIsRight = fpuOverflow && fpuExtraOverflow;

	if( !fpuIsRight )
	{
		// Values are wonky; assume the defaults.
		fpuOverflow		= RecompilerOptions().fpuOverflow;
		fpuExtraOverflow= RecompilerOptions().fpuExtraOverflow;
		fpuFullMode		= RecompilerOptions().fpuFullMode;
	}

	bool vuIsOk = true;

	if( vuExtraOverflow ) vuIsOk = vuIsOk && vuOverflow;
	if( vuSignOverflow ) vuIsOk = vuIsOk && vuExtraOverflow;

	if( !vuIsOk )
	{
		// Values are wonky; assume the defaults.
		vuOverflow		= RecompilerOptions().vuOverflow;
		vuExtraOverflow	= RecompilerOptions().vuExtraOverflow;
		vuSignOverflow	= RecompilerOptions().vuSignOverflow;
		vuUnderflow		= RecompilerOptions().vuUnderflow;
	}
}

void Pcsx2Config::RecompilerOptions::LoadSave()
{
	EnableEE = PCSX2_vm::EnableEE;
	EnableIOP = PCSX2_vm::EnableIOP;
	EnableVU0 = PCSX2_vm::EnableVU0;
	EnableVU1 = PCSX2_vm::EnableVU1;

	UseMicroVU0 = PCSX2_vm::UseMicroVU0;
	UseMicroVU1 = PCSX2_vm::UseMicroVU1;

	vuOverflow = PCSX2_vm::vuOverflow;
	vuExtraOverflow = PCSX2_vm::vuExtraOverflow;
	vuSignOverflow = PCSX2_vm::vuSignOverflow;
	vuUnderflow = PCSX2_vm::vuUnderflow;

	fpuOverflow = PCSX2_vm::fpuOverflow;
	fpuExtraOverflow = PCSX2_vm::fpuExtraOverflow;
	fpuFullMode = PCSX2_vm::fpuFullMode;
}

Pcsx2Config::CpuOptions::CpuOptions()
{
	sseMXCSR.bitmask	= DEFAULT_sseMXCSR;
	sseVUMXCSR.bitmask	= DEFAULT_sseVUMXCSR;
}

void Pcsx2Config::CpuOptions::ApplySanityCheck()
{
	sseMXCSR.ClearExceptionFlags().DisableExceptions();
	sseVUMXCSR.ClearExceptionFlags().DisableExceptions();

	Recompiler.ApplySanityCheck();
}


// NEW MEM OPTIONS - not sure about types, here....

void Pcsx2Config::CpuOptions::LoadSave()
{
/*
	IniBitBoolEx( sseMXCSR.DenormalsAreZero,	"FPU.DenormalsAreZero" );
	IniBitBoolEx( sseMXCSR.FlushToZero,			"FPU.FlushToZero" );
	IniBitfieldEx( sseMXCSR.RoundingControl,	"FPU.Roundmode" );

	IniBitBoolEx( sseVUMXCSR.DenormalsAreZero,	"VU.DenormalsAreZero" );
	IniBitBoolEx( sseVUMXCSR.FlushToZero,		"VU.FlushToZero" );
	IniBitfieldEx( sseVUMXCSR.RoundingControl,	"VU.Roundmode" );
	*/

	sseMXCSR.DenormalsAreZero = PCSX2_vm::FPU_DenormalsAreZero;
	sseMXCSR.FlushToZero = PCSX2_vm::FPU_FlushToZero;
	sseMXCSR.RoundingControl = PCSX2_vm::FPU_Roundmode;

	sseVUMXCSR.DenormalsAreZero = PCSX2_vm::VU_DenormalsAreZero;
	sseVUMXCSR.FlushToZero = PCSX2_vm::VU_FlushToZero;
	sseVUMXCSR.RoundingControl = PCSX2_vm::VU_Roundmode;


	Recompiler.LoadSave();
}

// Default GSOptions
Pcsx2Config::GSOptions::GSOptions()
{
	FrameSkipEnable			= false;

	VsyncQueueSize			= 2;

	FramesToDraw			= 2;
	FramesToSkip			= 2;

	FramerateNTSC			= 59.94;
	FrameratePAL			= 50.0;
}


// NEW MEM OPTIONS
void Pcsx2Config::GSOptions::LoadSave()
{
	VsyncQueueSize = PCSX2_vm::VsyncQueueSize;
	FrameSkipEnable = PCSX2_vm::FrameSkipEnable;
	FramesToDraw = PCSX2_vm::FramesToDraw;
	FramesToSkip = PCSX2_vm::FramesToSkip;
}

const wxChar *const tbl_GamefixNames[] =
{
	L"VuAddSub",
	L"FpuCompare",
	L"FpuMul",
	L"FpuNegDiv",
	L"XGKick",
	L"IPUWait",
	L"EETiming",
	L"SkipMPEG",
	L"OPHFlag",
	L"DMABusy",
	L"VIFFIFO",
	L"VIF1Stall",
	L"GIFFIFO",
	L"FMVinSoftware",
	L"GoemonTlb",
	L"ScarfaceIbit",
	L"CrashTagTeamRacingIbit",
	L"VU0Kickstart"
};

const __fi wxChar* EnumToString( GamefixId id )
{
	return tbl_GamefixNames[id];
}

// all gamefixes are disabled by default.
Pcsx2Config::GamefixOptions::GamefixOptions()
{
	DisableAll();
}

Pcsx2Config::GamefixOptions& Pcsx2Config::GamefixOptions::DisableAll()
{
	bitset = 0;
	return *this;
}

// Enables a full list of gamefixes.  The list can be either comma or pipe-delimited.
//   Example:  "XGKick,IpuWait"  or  "EEtiming,FpuCompare"
// If an unrecognized tag is encountered, a warning is printed to the console, but no error
// is generated.  This allows the system to function in the event that future versions of
// PCSX2 remove old hacks once they become obsolete.
void Pcsx2Config::GamefixOptions::Set( const wxString& list, bool enabled )
{
	wxStringTokenizer izer( list, L",|", wxTOKEN_STRTOK );
	
	while( izer.HasMoreTokens() )
	{
		wxString token( izer.GetNextToken() );

		GamefixId i;
		for (i=GamefixId_FIRST; i < pxEnumEnd; ++i)
		{
			if( token.CmpNoCase( EnumToString(i) ) == 0 ) break;
		}
		if( i < pxEnumEnd ) Set( i );
	}
}

void Pcsx2Config::GamefixOptions::Set( GamefixId id, bool enabled )
{
	EnumAssert( id );
	switch(id)
	{
		case Fix_VuAddSub:		VuAddSubHack		= enabled;	break;
		case Fix_FpuCompare:	FpuCompareHack		= enabled;	break;
		case Fix_FpuMultiply:	FpuMulHack			= enabled;	break;
		case Fix_FpuNegDiv:		FpuNegDivHack		= enabled;	break;
		case Fix_XGKick:		XgKickHack			= enabled;	break;
		case Fix_IpuWait:		IPUWaitHack			= enabled;	break;
		case Fix_EETiming:		EETimingHack		= enabled;	break;
		case Fix_SkipMpeg:		SkipMPEGHack		= enabled;	break;
		case Fix_OPHFlag:		OPHFlagHack			= enabled;  break;
		case Fix_DMABusy:		DMABusyHack			= enabled;  break;
		case Fix_VIFFIFO:		VIFFIFOHack			= enabled;  break;
		case Fix_VIF1Stall:		VIF1StallHack		= enabled;  break;
		case Fix_GIFFIFO:		GIFFIFOHack			= enabled;  break;
		case Fix_FMVinSoftware:	FMVinSoftwareHack	= enabled;  break;
		case Fix_GoemonTlbMiss: GoemonTlbHack		= enabled;  break;
		case Fix_ScarfaceIbit:  ScarfaceIbit        = enabled;  break;
		case Fix_CrashTagTeamIbit: CrashTagTeamRacingIbit = enabled; break;
		case Fix_VU0Kickstart:	VU0KickstartHack	= enabled; break;
		jNO_DEFAULT;
	}
}

bool Pcsx2Config::GamefixOptions::Get( GamefixId id ) const
{
	EnumAssert( id );
	switch(id)
	{
		case Fix_VuAddSub:		return VuAddSubHack;
		case Fix_FpuCompare:	return FpuCompareHack;
		case Fix_FpuMultiply:	return FpuMulHack;
		case Fix_FpuNegDiv:		return FpuNegDivHack;
		case Fix_XGKick:		return XgKickHack;
		case Fix_IpuWait:		return IPUWaitHack;
		case Fix_EETiming:		return EETimingHack;
		case Fix_SkipMpeg:		return SkipMPEGHack;
		case Fix_OPHFlag:		return OPHFlagHack;
		case Fix_DMABusy:		return DMABusyHack;
		case Fix_VIFFIFO:		return VIFFIFOHack;
		case Fix_VIF1Stall:		return VIF1StallHack;
		case Fix_GIFFIFO:		return GIFFIFOHack;
		case Fix_FMVinSoftware:	return FMVinSoftwareHack;
		case Fix_GoemonTlbMiss: return GoemonTlbHack;
		case Fix_ScarfaceIbit:  return ScarfaceIbit;
		case Fix_CrashTagTeamIbit: return CrashTagTeamRacingIbit;
		case Fix_VU0Kickstart:	return VU0KickstartHack;
		jNO_DEFAULT;
	}
	return false;		// unreachable, but we still need to suppress warnings >_<
}

// NEW MEM OPTIONS
void Pcsx2Config::GamefixOptions::LoadSave()
{
	VuAddSubHack = PCSX2_vm::VuAddSubHack;
	FpuCompareHack = PCSX2_vm::FpuCompareHack;
	FpuMulHack = PCSX2_vm::FpuMulHack;
	FpuNegDivHack = PCSX2_vm::FpuNegDivHack;
	XgKickHack = PCSX2_vm::XgKickHack;
	IPUWaitHack = PCSX2_vm::IPUWaitHack;
	EETimingHack = PCSX2_vm::EETimingHack;
	SkipMPEGHack = PCSX2_vm::SkipMPEGHack;
	OPHFlagHack = PCSX2_vm::OPHFlagHack;
	DMABusyHack = PCSX2_vm::DMABusyHack;
	VIFFIFOHack = PCSX2_vm::VIFFIFOHack;
	VIF1StallHack = PCSX2_vm::VIF1StallHack;
	GIFFIFOHack = PCSX2_vm::GIFFIFOHack;
	FMVinSoftwareHack = PCSX2_vm::FMVinSoftwareHack;
	GoemonTlbHack = PCSX2_vm::GoemonTlbHack;
	ScarfaceIbit = PCSX2_vm::ScarfaceIbit;
	CrashTagTeamRacingIbit = PCSX2_vm::CrashTagTeamRacingIbit;
	VU0KickstartHack = PCSX2_vm::VU0KickstartHack;
}

Pcsx2Config::Pcsx2Config()
{
	bitset = 0;
	// Set defaults for fresh installs / reset settings
	McdEnableEjection = true;
	McdFolderAutoManage = true;
	EnablePatches = true;
}

void Pcsx2Config::LoadSave()
{
	CdvdShareWrite = PCSX2_vm::CdvdShareWrite;
	EnablePatches = PCSX2_vm::EnablePatches;
	EnableCheats = PCSX2_vm::EnableCheats;
	EnableIPC = PCSX2_vm::EnableIPC;
	EnableWideScreenPatches = PCSX2_vm::EnableWideScreenPatches;
	EnableNointerlacingPatches = PCSX2_vm::EnableNointerlacingPatches;
	HostFs = PCSX2_vm::HostFs;

	McdEnableEjection = PCSX2_vm::McdEnableEjection;
	McdFolderAutoManage = PCSX2_vm::McdFolderAutoManage;
	MultitapPort0_Enabled = PCSX2_vm::MultitapPort0_Enabled;
	MultitapPort1_Enabled = PCSX2_vm::MultitapPort1_Enabled;

	// Process various sub-components:

	Speedhacks		.LoadSave();
	Cpu				.LoadSave();
	GS				.LoadSave();
	Gamefixes		.LoadSave();
}

bool Pcsx2Config::MultitapEnabled( uint port ) const
{
	pxAssert( port < 2 );
	return (port==0) ? MultitapPort0_Enabled : MultitapPort1_Enabled;
}

void Pcsx2Config::Load( const wxString& srcfile )
{
	LoadSave();
}

void Pcsx2Config::Save( const wxString& dstfile )
{
	LoadSave();
}
