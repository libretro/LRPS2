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

#include "App.h"
#include <array>
#include <map>
#include <memory>

static const bool EnableThreadedLoggingTest = false; //true;

class LogWriteEvent;

// --------------------------------------------------------------------------------------
//  PipeRedirectionBase
// --------------------------------------------------------------------------------------
// Implementations for this class are found in Win/Lnx specific modules.  Class creation
// should be done using NewPipeRedir() only (hence the protected constructor in this class).
//
class PipeRedirectionBase
{
	DeclareNoncopyableObject( PipeRedirectionBase );

public:
	virtual ~PipeRedirectionBase() =0;	// abstract destructor, forces abstract class behavior

protected:
	PipeRedirectionBase() {}
};

extern PipeRedirectionBase* NewPipeRedir( FILE* stdstream );

void OSDlog(ConsoleColors color, bool console, const std::string& str);

template<typename ... Args>
void OSDlog(ConsoleColors color, bool console, const std::string& format, Args ... args) {
#ifdef BUILTIN_GS_PLUGIN
	if (!console) return;
#else
	if (!GSosdLog && !console) return;
#endif

	size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
	std::vector<char> buf(size);
	snprintf( buf.data(), size, format.c_str(), args ... );

	OSDlog(color, console, buf.data());
}

void OSDmonitor(ConsoleColors color, const std::string key, const std::string value);

