/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/utilsexc.cpp
// Purpose:     wxExecute implementation for MSW
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) 1998-2002 wxWidgets dev team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/app.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #if wxUSE_STREAMS
        #include "wx/stream.h"
    #endif
    #include "wx/module.h"
#endif

#include "wx/process.h"
#include "wx/thread.h"
#include "wx/apptrait.h"
#include "wx/evtloop.h"
#include "wx/vector.h"


#include "wx/msw/private.h"

#include <ctype.h>

#if !defined(__GNUWIN32__) && !defined(__WXMICROWIN__) && !defined(__WXWINCE__)
    #include <direct.h>
    #include <dos.h>
#endif

#if defined(__GNUWIN32__)
    #include <sys/stat.h>
#endif

#if !defined(__WXMICROWIN__) && !defined(__WXWINCE__)
    #ifndef __UNIX__
        #include <io.h>
    #endif

    #ifndef __GNUWIN32__
        #include <shellapi.h>
    #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __WATCOMC__
    #if !(defined(_MSC_VER) && (_MSC_VER > 800))
        #include <errno.h>
    #endif
#endif
#include <stdarg.h>

// FIXME-VC6: These are not defined in VC6 SDK headers.
#ifndef BELOW_NORMAL_PRIORITY_CLASS
    #define BELOW_NORMAL_PRIORITY_CLASS 0x4000
#endif

#ifndef ABOVE_NORMAL_PRIORITY_CLASS
    #define ABOVE_NORMAL_PRIORITY_CLASS 0x8000
#endif

// ----------------------------------------------------------------------------
// private types
// ----------------------------------------------------------------------------

#if wxUSE_STREAMS && !defined(__WXWINCE__)

#include "wx/private/pipestream.h"
#include "wx/private/streamtempinput.h"

// ----------------------------------------------------------------------------
// wxPipe represents a Win32 anonymous pipe
// ----------------------------------------------------------------------------

class wxPipe
{
public:
    // the symbolic names for the pipe ends
    enum Direction
    {
        Read,
        Write
    };

    // default ctor doesn't do anything
    wxPipe() { m_handles[Read] = m_handles[Write] = INVALID_HANDLE_VALUE; }

    // create the pipe, return true if ok, false on error
    bool Create()
    {
        // default secutiry attributes
        SECURITY_ATTRIBUTES security;

        security.nLength              = sizeof(security);
        security.lpSecurityDescriptor = NULL;
        security.bInheritHandle       = TRUE; // to pass it to the child

        if ( !::CreatePipe(&m_handles[0], &m_handles[1], &security, 0) )
        {
            wxLogSysError(_("Failed to create an anonymous pipe"));

            return false;
        }

        return true;
    }

    // return true if we were created successfully
    bool IsOk() const { return m_handles[Read] != INVALID_HANDLE_VALUE; }

    // return the descriptor for one of the pipe ends
    HANDLE operator[](Direction which) const { return m_handles[which]; }

    // detach a descriptor, meaning that the pipe dtor won't close it, and
    // return it
    HANDLE Detach(Direction which)
    {
        HANDLE handle = m_handles[which];
        m_handles[which] = INVALID_HANDLE_VALUE;

        return handle;
    }

    // close the pipe descriptors
    void Close()
    {
        for ( size_t n = 0; n < WXSIZEOF(m_handles); n++ )
        {
            if ( m_handles[n] != INVALID_HANDLE_VALUE )
            {
                ::CloseHandle(m_handles[n]);
                m_handles[n] = INVALID_HANDLE_VALUE;
            }
        }
    }

    // dtor closes the pipe descriptors
    ~wxPipe() { Close(); }

private:
    HANDLE m_handles[2];
};

#endif // wxUSE_STREAMS
// ============================================================================
// implementation of IO redirection support classes
// ============================================================================

#if wxUSE_STREAMS && !defined(__WXWINCE__)

// ----------------------------------------------------------------------------
// wxPipeInputStreams
// ----------------------------------------------------------------------------

wxPipeInputStream::wxPipeInputStream(HANDLE hInput)
{
    m_hInput = hInput;
}

wxPipeInputStream::~wxPipeInputStream()
{
    if ( m_hInput != INVALID_HANDLE_VALUE )
        ::CloseHandle(m_hInput);
}

bool wxPipeInputStream::CanRead() const
{
    // we can read if there's something in the put back buffer
    // even pipe is closed
    if ( m_wbacksize > m_wbackcur )
        return true;

    wxPipeInputStream * const self = wxConstCast(this, wxPipeInputStream);

    if ( !IsOpened() )
    {
        // set back to mark Eof as it may have been unset by Ungetch()
        self->m_lasterror = wxSTREAM_EOF;
        return false;
    }

    DWORD nAvailable;

    // function name is misleading, it works with anon pipes as well
    DWORD rc = ::PeekNamedPipe
                    (
                      m_hInput,     // handle
                      NULL, 0,      // ptr to buffer and its size
                      NULL,         // [out] bytes read
                      &nAvailable,  // [out] bytes available
                      NULL          // [out] bytes left
                    );

    if ( !rc )
    {
        // don't try to continue reading from a pipe if an error occurred or if
        // it had been closed
        ::CloseHandle(m_hInput);

        self->m_hInput = INVALID_HANDLE_VALUE;
        self->m_lasterror = wxSTREAM_EOF;

        nAvailable = 0;
    }

    return nAvailable != 0;
}

size_t wxPipeInputStream::OnSysRead(void *buffer, size_t len)
{
    if ( !IsOpened() )
    {
        m_lasterror = wxSTREAM_EOF;

        return 0;
    }

    DWORD bytesRead;
    if ( !::ReadFile(m_hInput, buffer, len, &bytesRead, NULL) )
    {
        m_lasterror = ::GetLastError() == ERROR_BROKEN_PIPE
                        ? wxSTREAM_EOF
                        : wxSTREAM_READ_ERROR;
    }

    // bytesRead is set to 0, as desired, if an error occurred
    return bytesRead;
}

// ----------------------------------------------------------------------------
// wxPipeOutputStream
// ----------------------------------------------------------------------------

wxPipeOutputStream::wxPipeOutputStream(HANDLE hOutput)
{
    m_hOutput = hOutput;

    // unblock the pipe to prevent deadlocks when we're writing to the pipe
    // from which the child process can't read because it is writing in its own
    // end of it
    DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
    if ( !::SetNamedPipeHandleState
            (
                m_hOutput,
                &mode,
                NULL,       // collection count (we don't set it)
                NULL        // timeout (we don't set it neither)
            ) ) { }
}

bool wxPipeOutputStream::Close()
{
   return ::CloseHandle(m_hOutput) != 0;
}


size_t wxPipeOutputStream::OnSysWrite(const void *buffer, size_t len)
{
    m_lasterror = wxSTREAM_NO_ERROR;

    DWORD totalWritten = 0;
    while ( len > 0 )
    {
        DWORD chunkWritten;
        if ( !::WriteFile(m_hOutput, buffer, len, &chunkWritten, NULL) )
        {
            m_lasterror = ::GetLastError() == ERROR_BROKEN_PIPE
                                ? wxSTREAM_EOF
                                : wxSTREAM_WRITE_ERROR;
            break;
        }

        if ( !chunkWritten )
            break;

        buffer = (char *)buffer + chunkWritten;
        totalWritten += chunkWritten;
        len -= chunkWritten;
    }

    return totalWritten;
}

#endif // wxUSE_STREAMS
