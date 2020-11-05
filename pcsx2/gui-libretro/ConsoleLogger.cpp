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
#include "App.h"
#include "ConsoleLogger.h"
#include "MSWstuff.h"

#include "Utilities/Console.h"
#include "Utilities/IniInterface.h"
#include "Utilities/SafeArray.inl"

#include <wx/textfile.h>

wxDECLARE_EVENT(pxEvt_SetTitleText, wxCommandEvent);
wxDECLARE_EVENT(pxEvt_FlushQueue, wxCommandEvent);

wxDEFINE_EVENT(pxEvt_SetTitleText, wxCommandEvent);
wxDEFINE_EVENT(pxEvt_FlushQueue, wxCommandEvent);

// C++ requires abstract destructors to exist, even though they're abstract.
PipeRedirectionBase::~PipeRedirectionBase() = default;

// ----------------------------------------------------------------------------
//
void pxLogConsole::DoLogRecord(wxLogLevel level, const wxString &message, const wxLogRecordInfo &info)
{
	switch ( level )
	{
		case wxLOG_Trace:
		case wxLOG_Debug:
		break;

		case wxLOG_FatalError:
			// This one is unused by wx, and unused by PCSX2 (we prefer exceptions, thanks).
			pxFailDev( "Stop using FatalError and use assertions or exceptions instead." );
		break;

		case wxLOG_Status:
			// Also unsed by wx, and unused by PCSX2 also (we prefer direct API calls to our main window!)
			pxFailDev( "Stop using wxLogStatus just access the Pcsx2App functions directly instead." );
		break;

		case wxLOG_Info:
			if ( !GetVerbose() ) return;
			// fallthrough!

		case wxLOG_Message:
			Console.WriteLn( L"[wx] %s", WX_STR(message));
		break;

		case wxLOG_Error:
			Console.Error(L"[wx] %s", WX_STR(message));
		break;

		case wxLOG_Warning:
			Console.Warning(L"[wx] %s", WX_STR(message));
		break;
	}
}

#ifndef __LIBRETRO__
void OSDlog(ConsoleColors color, bool console, const std::string& str)
{
	if (GSosdLog)
		GSosdLog(str.c_str(), wxGetApp().GetProgramLog()->GetRGBA(color));
	if (console)
		Console.WriteLn(color, str.c_str());
}

void OSDmonitor(ConsoleColors color, const std::string key, const std::string value) {
	if(!GSosdMonitor) return;

	GSosdMonitor(wxString(key).utf8_str(), wxString(value).utf8_str(), wxGetApp().GetProgramLog()->GetRGBA(color));
}
#else
void OSDlog(ConsoleColors color, bool console, const std::string& str)
{
	if (console)
		Console.WriteLn(color, str.c_str());
}
#endif

