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

#include "../Common.h"
#include "App.h"
#include "MemoryCardFile.h"
#include "../IopBios.h"
#include "../IopDma.h"
#include "../R5900.h"

#include "../Counters.h"
#include "../GS.h"
#include "../Elfheader.h"
#include "../Patch.h"
#include "SysThreads.h"
#include "../MTVU.h"
#include "../FW.h"
#include "../GS/GSFuncs.h"
#include "../PAD/PAD.h"
#include "../SPU2/spu2.h"

#include "../DebugTools/MIPSAnalyst.h"
#include "../DebugTools/SymbolMap.h"

DEV9handler dev9Handler;
USBhandler usbHandler;

static void modules_close(void)
{
	DoCDVDclose();
	FWclose();
	SPU2close();
	/* DEV9 */
	DEV9close();
	dev9Handler = NULL;

	/* USB */
	USBclose();
	usbHandler = NULL;

	FileMcd_EmuClose();
	if( GetMTGS().IsSelf() )
		GSclose();
	else
		GetMTGS().Suspend();
}

static void modules_open(bool isSuspended)
{
	GetMTGS().WaitForOpen();
	if (isSuspended || !g_GameStarted)
		DoCDVDopen();
	FWopen();
	SPU2open();

	/* DEV9 */
	dev9Handler = NULL;
	DEV9open();
	DEV9irqCallback( dev9Irq );
	dev9Handler = DEV9irqHandler();

	/* USB */
	usbHandler = NULL;

	USBopen();
	USBirqCallback( usbIrq );
	usbHandler = USBirqHandler();

	FileMcd_EmuOpen();
}

static void modules_shutdown(void)
{
	GetMTGS().Cancel();	// cancel it for speedier shutdown!
	GSshutdown();
	SPU2shutdown();
	PADshutdown();
	USBshutdown();
	DEV9shutdown();
}

// --------------------------------------------------------------------------------------
//  SysCoreThread *External Thread* Implementations
//    (Called from outside the context of this thread)
// --------------------------------------------------------------------------------------

SysCoreThread::SysCoreThread()
{
	m_name = L"EE Core";
	m_resetRecompilers = true;
	m_resetVsyncTimers = true;
	m_resetVirtualMachine = true;

	m_hasActiveMachine = false;
}

SysCoreThread::~SysCoreThread()
{
	SysCoreThread::Cancel(wxTimeSpan(0, 0, 4, 0));
}

void SysCoreThread::Cancel()
{
	m_hasActiveMachine = false;
	R3000A::ioman::reset();
	_parent::Cancel(wxTimeSpan(0, 0, 4, 0));
}

bool SysCoreThread::Cancel(const wxTimeSpan& span)
{
	m_hasActiveMachine = false;
	R3000A::ioman::reset();
	return _parent::Cancel(span);
}

void SysCoreThread::OnStart()
{
	_parent::OnStart();
}

void SysCoreThread::Start()
{
	GSinit();
	SPU2init();
	PADinit(0);
	USBinit();
	DEV9init();
	_parent::Start();
}

// Resumes the core execution state, or does nothing is the core is already running.  If
// settings were changed, resets will be performed as needed and emulation state resumed from
// memory savestates.
//
// Exceptions (can occur on first call only):
//   ThreadCreationError - Insufficient system resources to create thread.
//
void SysCoreThread::OnResumeReady()
{
	if (m_resetVirtualMachine)
		m_hasActiveMachine = false;

	if (!m_hasActiveMachine)
		m_resetRecompilers = true;
}

// This function *will* reset the emulator in order to allow the specified elf file to
// take effect.  This is because it really doesn't make sense to change the elf file outside
// the context of a reset/restart.
void SysCoreThread::SetElfOverride(const wxString& elf)
{
	m_elf_override = elf;


	Hle_SetElfPath(elf.ToUTF8());
}

// Performs a quicker reset that does not deallocate memory associated with PS2 virtual machines
// or cpu providers (recompilers).
void SysCoreThread::ResetQuick()
{
	Suspend();

	m_resetVirtualMachine = true;
	m_hasActiveMachine = false;
	R3000A::ioman::reset();
}

void SysCoreThread::Reset()
{
	ResetQuick();
	GetVmMemory().DecommitAll();
	SysClearExecutionCache();
}


// Applies a full suite of new settings, which will automatically facilitate the necessary
// resets of the core and components.  The scope of resetting
// is determined by comparing the current settings against the new settings, so that only
// real differences are applied.
void SysCoreThread::ApplySettings(const Pcsx2Config& src)
{
	if (src == EmuConfig)
		return;

	if (!(IsPaused() | IsSelf()))
		return;

	m_resetRecompilers = (src.Cpu != EmuConfig.Cpu) || (src.Gamefixes != EmuConfig.Gamefixes) || (src.Speedhacks != EmuConfig.Speedhacks);
	m_resetVsyncTimers = (src.GS != EmuConfig.GS);

	const_cast<Pcsx2Config&>(EmuConfig) = src;
}

// --------------------------------------------------------------------------------------
//  SysCoreThread *Worker* Implementations
//    (Called from the context of this thread only)
// --------------------------------------------------------------------------------------
bool SysCoreThread::HasPendingStateChangeRequest() const
{
	return !m_hasActiveMachine || _parent::HasPendingStateChangeRequest();
}

void SysCoreThread::_reset_stuff_as_needed()
{
	// Note that resetting recompilers along with the virtual machine is only really needed
	// because of changes to the TLB.  We don't actually support the TLB, however, so rec
	// resets aren't in fact *needed* ... yet.  But might as well, no harm.  --air

	GetVmMemory().CommitAll();

	if (m_resetVirtualMachine || m_resetRecompilers)
	{
		SysClearExecutionCache();
		memBindConditionalHandlers();
		SetCPUState(EmuConfig.Cpu.sseMXCSR, EmuConfig.Cpu.sseVUMXCSR);

		m_resetRecompilers = false;
	}

	if (m_resetVirtualMachine)
	{
		DoCpuReset();

		m_resetVirtualMachine = false;
		m_resetVsyncTimers = false;

		ForgetLoadedPatches();
	}

	if (m_resetVsyncTimers)
	{
		UpdateVSyncRate();

		m_resetVsyncTimers = false;
	}
}

void SysCoreThread::DoCpuReset()
{
	cpuReset();
}

void SysCoreThread::GameStartingInThread()
{
	GetMTGS().SendGameCRC(ElfCRC);

	MIPSAnalyst::ScanForFunctions(ElfTextRange.first, ElfTextRange.first + ElfTextRange.second, true);
	symbolMap.UpdateActiveSymbols();

	ApplyLoadedPatches(PPT_ONCE_ON_LOAD);
}

bool SysCoreThread::StateCheckInThread()
{
	return _parent::StateCheckInThread() && (_reset_stuff_as_needed(), true);
}

// Runs CPU cycles indefinitely, until the user or another thread requests execution to break.
// Rationale: This very short function allows an override point and solves an SEH
// "exception-type boundary" problem (can't mix SEH and C++ exceptions in the same function).
void SysCoreThread::DoCpuExecute()
{
	m_hasActiveMachine = true;
	Cpu->Execute();
}

void SysCoreThread::ExecuteTaskInThread()
{
	m_sem_event.Wait();

	m_mxcsr_saved.bitmask = _mm_getcsr();

	PCSX2_PAGEFAULT_PROTECT
	{
		for (;;)
		{
			StateCheckInThread();
			DoCpuExecute();
		}
	}
	PCSX2_PAGEFAULT_EXCEPT;
}

void SysCoreThread::OnSuspendInThread()
{
	modules_close();
}

void SysCoreThread::OnResumeInThread(bool isSuspended)
{
	modules_open(isSuspended);
}

// Invoked by the pthread_exit or pthread_cancel.
void SysCoreThread::OnCleanupInThread()
{
	m_ExecMode = ExecMode_Closing;

	m_hasActiveMachine = false;
	m_resetVirtualMachine = true;

	R3000A::ioman::reset();
	// FIXME: temporary workaround for deadlock on exit, which actually should be a crash
	vu1Thread.WaitVU();
	modules_close();
	modules_shutdown();

	_mm_setcsr(m_mxcsr_saved.bitmask);
	_parent::OnCleanupInThread();

	m_ExecMode = ExecMode_NoThreadYet;
}
