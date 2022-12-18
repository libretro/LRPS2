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

#if defined(__UNIX__)
#include <signal.h>
#endif

#include "Dependencies.h"

#include "Threading.h"

// for lack of a better place...
Fnptr_OutOfMemory pxDoOutOfMemory = NULL;

// Because wxTrap isn't available on Linux builds of wxWidgets (non-Debug, typically)
void pxTrap(void)
{
#if defined(__WXMSW__)
    __debugbreak();
#elif defined(__WXMAC__) && !defined(__DARWIN__)
#if __powerc
    Debugger();
#else
    SysBreak();
#endif
#elif defined(__UNIX__)
    raise(SIGTRAP);
#else
// TODO
#endif // Win/Unix
}

// --------------------------------------------------------------------------------------
//  BaseException  (implementations)
// --------------------------------------------------------------------------------------

BaseException &BaseException::SetDiagMsg(const wxString &msg_diag)
{
    m_message_diag = msg_diag;
    return *this;
}

BaseException &BaseException::SetUserMsg(const wxString &msg_user)
{
    m_message_user = msg_user;
    return *this;
}

// --------------------------------------------------------------------------------------
//  Exception::RuntimeError   (implementations)
// --------------------------------------------------------------------------------------
Exception::RuntimeError::RuntimeError(const std::runtime_error &ex, const wxString &prefix)
{
    IsSilent = false;

    SetDiagMsg(pxsFmt(L"STL Runtime Error%s: %s",
                      (prefix.IsEmpty() ? L"" : pxsFmt(L" (%s)", WX_STR(prefix)).c_str()),
                      WX_STR(fromUTF8(ex.what()))));
}

Exception::RuntimeError::RuntimeError(const std::exception &ex, const wxString &prefix)
{
    IsSilent = false;

    SetDiagMsg(pxsFmt(L"STL Exception%s: %s",
                      (prefix.IsEmpty() ? L"" : pxsFmt(L" (%s)", WX_STR(prefix)).c_str()),
                      WX_STR(fromUTF8(ex.what()))));
}

// --------------------------------------------------------------------------------------
//  Exception::OutOfMemory   (implementations)
// --------------------------------------------------------------------------------------
Exception::OutOfMemory::OutOfMemory(const wxString &allocdesc)
{
    AllocDescription = allocdesc;
}

// --------------------------------------------------------------------------------------
//  Exception::VirtualMemoryMapConflict   (implementations)
// --------------------------------------------------------------------------------------
Exception::VirtualMemoryMapConflict::VirtualMemoryMapConflict(const wxString &allocdesc)
{
    AllocDescription = allocdesc;
    m_message_user = "Virtual memory mapping failure!  Your system may have conflicting device drivers, services, or may simply have insufficient memory or resources to meet PCSX2's lofty needs.";
}

// --------------------------------------------------------------------------------------
//  Exceptions from Errno (POSIX)
// --------------------------------------------------------------------------------------

// Translates an Errno code into an exception.
// Throws an exception based on the given error code (usually taken from ANSI C's errno)
BaseException *Exception::FromErrno(const wxString &streamname, int errcode)
{
    switch (errcode)
    {
	    case EINVAL:
		    return &(new Exception::BadStream(streamname))->SetDiagMsg(L"Invalid argument? (likely caused by an unforgivable programmer error!)");

	    case EACCES: // Access denied!
		    return new Exception::AccessDenied(streamname);

	    case EMFILE:                                                                                     // Too many open files!
		    return &(new Exception::CannotCreateStream(streamname))->SetDiagMsg(L"Too many open files"); // File handle allocation failure

	    case EEXIST:
		    return &(new Exception::CannotCreateStream(streamname))->SetDiagMsg(L"File already exists");

	    case ENOENT: // File not found!
		    return new Exception::FileNotFound(streamname);

	    case EPIPE:
		    return &(new Exception::BadStream(streamname))->SetDiagMsg(L"Broken pipe");

	    case EBADF:
		    return &(new Exception::BadStream(streamname))->SetDiagMsg(L"Bad file number");

	    default:
		    return &(new Exception::BadStream(streamname))->SetDiagMsg(pxsFmt(L"General file/stream error [errno: %d]", errcode));
    }
}
