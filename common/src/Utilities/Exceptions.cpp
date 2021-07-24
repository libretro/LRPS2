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

#include <wx/app.h>
#include "Threading.h"

#if defined(__UNIX__)
#include <signal.h>
#endif

// for lack of a better place...
Fnptr_OutOfMemory pxDoOutOfMemory = NULL;

// ------------------------------------------------------------------------
// Force DevAssert to *not* inline for devel builds (allows using breakpoints to trap assertions,
// and force it to inline for release builds (optimizes it out completely since IsDevBuild is a
// const false).
//
#ifdef PCSX2_DEVBUILD
#define DEVASSERT_INLINE __noinline
#else
#define DEVASSERT_INLINE __fi
#endif

// Using a threadlocal assertion guard.  Separate threads can assert at the same time.
// That's ok.  What we don't want is the *same* thread recurse-asserting.
static DeclareTls(int) s_assert_guard(0);


// Because wxTrap isn't available on Linux builds of wxWidgets (non-Debug, typically)
void pxTrap()
{
#if defined(__WXMSW__) && !defined(__WXMICROWIN__)
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

DEVASSERT_INLINE void pxOnAssert(const DiagnosticOrigin &origin, const wxString &msg)
{
}

// --------------------------------------------------------------------------------------
//  BaseException  (implementations)
// --------------------------------------------------------------------------------------

BaseException &BaseException::SetBothMsgs(const wxChar *msg_diag)
{
    m_message_user = msg_diag ? wxString(wxGetTranslation(msg_diag)) : wxString("");
    return SetDiagMsg(msg_diag);
}

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

wxString BaseException::FormatDiagnosticMessage() const
{
    return m_message_diag;
}

wxString BaseException::FormatDisplayMessage() const
{
    return m_message_user.IsEmpty() ? m_message_diag : m_message_user;
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

wxString Exception::OutOfMemory::FormatDiagnosticMessage() const
{
    FastFormatUnicode retmsg;
    retmsg.Write(L"Out of memory");
    if (!AllocDescription.IsEmpty())
        retmsg.Write(L" while allocating '%s'", WX_STR(AllocDescription));

    if (!m_message_diag.IsEmpty())
        retmsg.Write(L":\n%s", WX_STR(m_message_diag));

    return retmsg;
}

wxString Exception::OutOfMemory::FormatDisplayMessage() const
{
    FastFormatUnicode retmsg;
    retmsg.Write(L"%s", "Out of memory!");

    if (!m_message_user.IsEmpty())
        retmsg.Write(L"\n\n%s", WX_STR(m_message_user));

    return retmsg;
}


// --------------------------------------------------------------------------------------
//  Exception::VirtualMemoryMapConflict   (implementations)
// --------------------------------------------------------------------------------------
Exception::VirtualMemoryMapConflict::VirtualMemoryMapConflict(const wxString &allocdesc)
{
    AllocDescription = allocdesc;
    m_message_user = "Virtual memory mapping failure!  Your system may have conflicting device drivers, services, or may simply have insufficient memory or resources to meet PCSX2's lofty needs.";
}

wxString Exception::VirtualMemoryMapConflict::FormatDiagnosticMessage() const
{
    FastFormatUnicode retmsg;
    retmsg.Write(L"Virtual memory map failed");
    if (!AllocDescription.IsEmpty())
        retmsg.Write(L" while reserving '%s'", WX_STR(AllocDescription));

    if (!m_message_diag.IsEmpty())
        retmsg.Write(L":\n%s", WX_STR(m_message_diag));

    return retmsg;
}

wxString Exception::VirtualMemoryMapConflict::FormatDisplayMessage() const
{
    FastFormatUnicode retmsg;
    retmsg.Write(L"%s",
                 L"There is not enough virtual memory available, or necessary virtual memory mappings have already been reserved by other processes, services, or DLLs.");

    if (!m_message_diag.IsEmpty())
        retmsg.Write(L"\n\n%s", WX_STR(m_message_diag));

    return retmsg;
}


// ------------------------------------------------------------------------
wxString Exception::CancelEvent::FormatDiagnosticMessage() const
{
    return L"Action canceled: " + m_message_diag;
}

wxString Exception::CancelEvent::FormatDisplayMessage() const
{
    return L"Action canceled: " + m_message_diag;
}

// --------------------------------------------------------------------------------------
//  Exception::BadStream  (implementations)
// --------------------------------------------------------------------------------------
wxString Exception::BadStream::FormatDiagnosticMessage() const
{
    FastFormatUnicode retval;
    _formatDiagMsg(retval);
    return retval;
}

wxString Exception::BadStream::FormatDisplayMessage() const
{
    FastFormatUnicode retval;
    _formatUserMsg(retval);
    return retval;
}

void Exception::BadStream::_formatDiagMsg(FastFormatUnicode &dest) const
{
    dest.Write(L"Path: ");
    if (!StreamName.IsEmpty())
        dest.Write(L"%s", WX_STR(StreamName));
    else
        dest.Write(L"[Unnamed or unknown]");

    if (!m_message_diag.IsEmpty())
        dest.Write(L"\n%s", WX_STR(m_message_diag));
}

void Exception::BadStream::_formatUserMsg(FastFormatUnicode &dest) const
{
    dest.Write("Path: ");
    if (!StreamName.IsEmpty())
        dest.Write(L"%s", WX_STR(StreamName));
    else
        dest.Write("[Unnamed or unknown]");

    if (!m_message_user.IsEmpty())
        dest.Write(L"\n%s", WX_STR(m_message_user));
}

// --------------------------------------------------------------------------------------
//  Exception::CannotCreateStream  (implementations)
// --------------------------------------------------------------------------------------
wxString Exception::CannotCreateStream::FormatDiagnosticMessage() const
{
    FastFormatUnicode retval;
    retval.Write("File could not be created.");
    _formatDiagMsg(retval);
    return retval;
}

wxString Exception::CannotCreateStream::FormatDisplayMessage() const
{
    FastFormatUnicode retval;
    retval.Write("A file could not be created.");
    retval.Write("\n");
    _formatUserMsg(retval);
    return retval;
}

// --------------------------------------------------------------------------------------
//  Exception::FileNotFound  (implementations)
// --------------------------------------------------------------------------------------
wxString Exception::FileNotFound::FormatDiagnosticMessage() const
{
    FastFormatUnicode retval;
    retval.Write("File not found.\n");
    _formatDiagMsg(retval);
    return retval;
}

wxString Exception::FileNotFound::FormatDisplayMessage() const
{
    FastFormatUnicode retval;
    retval.Write("File not found.");
    retval.Write("\n");
    _formatUserMsg(retval);
    return retval;
}

// --------------------------------------------------------------------------------------
//  Exception::AccessDenied  (implementations)
// --------------------------------------------------------------------------------------
wxString Exception::AccessDenied::FormatDiagnosticMessage() const
{
    FastFormatUnicode retval;
    retval.Write("Permission denied to file.\n");
    _formatDiagMsg(retval);
    return retval;
}

wxString Exception::AccessDenied::FormatDisplayMessage() const
{
    FastFormatUnicode retval;
    retval.Write("Permission denied while trying to open file, likely due to insufficient user account rights.");
    retval.Write("\n");
    _formatUserMsg(retval);
    return retval;
}

// --------------------------------------------------------------------------------------
//  Exception::EndOfStream  (implementations)
// --------------------------------------------------------------------------------------
wxString Exception::EndOfStream::FormatDiagnosticMessage() const
{
    FastFormatUnicode retval;
    retval.Write("Unexpected end of file or stream.\n");
    _formatDiagMsg(retval);
    return retval;
}

wxString Exception::EndOfStream::FormatDisplayMessage() const
{
    FastFormatUnicode retval;
    retval.Write("Unexpected end of file or stream encountered.  File is probably truncated or corrupted.");
    retval.Write("\n");
    _formatUserMsg(retval);
    return retval;
}

// --------------------------------------------------------------------------------------
//  Exceptions from Errno (POSIX)
// --------------------------------------------------------------------------------------

// Translates an Errno code into an exception.
// Throws an exception based on the given error code (usually taken from ANSI C's errno)
BaseException *Exception::FromErrno(const wxString &streamname, int errcode)
{
    pxAssumeDev(errcode != 0, "Invalid NULL error code?  (errno)");

    switch (errcode) {
        case EINVAL:
            pxFailDev(L"Invalid argument");
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
