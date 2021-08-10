
namespace PCSX2_vm
{
	// [Emucore]
	bool CdvdShareWrite				= false;
	bool EnablePatches				= true;
	bool EnableCheats				= true;
	bool EnableIPC					= false;
	bool EnableWideScreenPatches	= true;
	bool EnableNointerlacingPatches = true;
	bool HostFs						= false;
	bool McdEnableEjection			= true;
	bool McdFolderAutoManage		= true;
	bool MultitapPort0_Enabled		= false;
	bool MultitapPort1_Enabled		= false;

	// [EmuCore/Speedhacks]
	s8 EECycleRate					= 0;
	u8 EECycleSkip					= 0;
	bool IntcStat					= true;
	bool WaitLoop					= true;
	bool vuFlagHack					= true;
	bool vuThread					= true;
	bool vu1Instant					= true;

	// [EmuCore/CPU]
	u32 FPU_DenormalsAreZero		= 1;
	u32 FPU_FlushToZero				= 1;
	u32 FPU_Roundmode				= 3;
	u32 VU_DenormalsAreZero			= 1;
	u32 VU_FlushToZero				= 1;
	int VU_Roundmode				= 3;

	// [EmuCore/CPU/Recompiler]
	bool EnableEE					= true;
	bool EnableIOP					= true;
	bool EnableVU0					= true;
	bool EnableVU1					= true;
	bool UseMicroVU0				= true;
	bool UseMicroVU1				= true;

	bool vuOverflow					= true;
	bool vuExtraOverflow			= false;
	bool vuSignOverflow				= false;
	bool vuUnderflow				= false;
	bool fpuOverflow				= true;
	bool fpuExtraOverflow			= false;
	bool fpuFullMode				= false;

	// [EmuCore/GS]
	int VsyncQueueSize				= 2;
	int FrameSkipEnable				= false;
	int FramesToDraw				= 1;
	int FramesToSkip				= 1;

	// [EmuCore / Gamefixes]
	bool VuAddSubHack				= false;
	bool FpuCompareHack				= false;
	bool FpuMulHack					= false;
	bool FpuNegDivHack				= false;
	bool XgKickHack					= false;

	bool IPUWaitHack				= false;
	bool EETimingHack				= false;
	bool SkipMPEGHack				= false;
	bool OPHFlagHack				= false;
	bool DMABusyHack				= false;

	bool VIFFIFOHack				= false;
	bool VIF1StallHack				= false;
	bool GIFFIFOHack				= false;
	bool FMVinSoftwareHack			= false;
	bool GoemonTlbHack				= false;

	bool ScarfaceIbit				= false;
	bool CrashTagTeamRacingIbit		= false;
	bool VU0KickstartHack			= false;

}

