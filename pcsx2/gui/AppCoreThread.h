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

#include "System/SysThreads.h"

#include "AppEventListeners.h"
#include "SaveState.h"

// --------------------------------------------------------------------------------------
//  AppCoreThread class
// --------------------------------------------------------------------------------------
class AppCoreThread : public SysCoreThread
{
	typedef SysCoreThread _parent;

protected:
	std::atomic<bool> m_resetCdvd;

public:
	AppCoreThread();
	virtual ~AppCoreThread();

	void ResetCdvd() { m_resetCdvd = true; }

	virtual void Suspend();
	virtual void Resume();
	virtual void Reset();
	virtual void ResetQuick();
	virtual void Cancel();
	virtual bool StateCheckInThread();
	virtual void ChangeCdvdSource();

	virtual void ApplySettings( const Pcsx2Config& src );
	virtual void UploadStateCopy( const VmStateBuffer& copy );

protected:
	virtual void DoCpuExecute();

	virtual void OnResumeReady();
	virtual void OnPause();
	virtual void OnResumeInThread( bool IsSuspended );
	virtual void OnSuspendInThread();
	virtual void OnCleanupInThread();
	virtual void GameStartingInThread();
	virtual void ExecuteTaskInThread();
	virtual void DoCpuReset();
};
