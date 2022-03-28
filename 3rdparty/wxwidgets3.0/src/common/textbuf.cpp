///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/textbuf.cpp
// Purpose:     implementation of wxTextBuffer class
// Created:     14.11.01
// Author:      Morten Hanssen, Vadim Zeitlin
// Copyright:   (c) 1998-2001 wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// headers
// ============================================================================

#include  "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include  "wx/string.h"
#endif

#include "wx/textbuf.h"

// ============================================================================
// wxTextBuffer class implementation
// ============================================================================

// ----------------------------------------------------------------------------
// static methods (always compiled in)
// ----------------------------------------------------------------------------

// default type is the native one

const wxTextFileType wxTextBuffer::typeDefault =
#if defined(__WINDOWS__)
  wxTextFileType_Dos;
#elif defined(__UNIX__)
  wxTextFileType_Unix;
#else
  wxTextFileType_None;
  #error  "wxTextBuffer: unsupported platform."
#endif

const wxChar *wxTextBuffer::GetEOL(wxTextFileType type)
{
    switch ( type ) {
        default:
            break;
            // fall through nevertheless - we must return something...

        case wxTextFileType_None: return wxEmptyString;
        case wxTextFileType_Unix: return wxT("\n");
        case wxTextFileType_Dos:  return wxT("\r\n");
        case wxTextFileType_Mac:  return wxT("\r");
    }
}

#if wxUSE_TEXTBUFFER

wxString wxTextBuffer::ms_eof;

// ----------------------------------------------------------------------------
// ctors & dtor
// ----------------------------------------------------------------------------

wxTextBuffer::wxTextBuffer(const wxString& strBufferName)
            : m_strBufferName(strBufferName)
{
    m_nCurLine = 0;
    m_isOpened = false;
}

wxTextBuffer::~wxTextBuffer()
{
    // required here for Darwin
}

// ----------------------------------------------------------------------------
// buffer operations
// ----------------------------------------------------------------------------

bool wxTextBuffer::Exists() const
{
    return OnExists();
}

bool wxTextBuffer::Create(const wxString& strBufferName)
{
    m_strBufferName = strBufferName;

    return Create();
}

bool wxTextBuffer::Create()
{
    // if the buffer already exists do nothing
    if ( Exists() ) return false;

    if ( !OnOpen(m_strBufferName, WriteAccess) )
        return false;

    OnClose();
    return true;
}

bool wxTextBuffer::Open(const wxString& strBufferName, const wxMBConv& conv)
{
    m_strBufferName = strBufferName;

    return Open(conv);
}

bool wxTextBuffer::Open(const wxMBConv& conv)
{
    // open buffer in read-only mode
    if ( !OnOpen(m_strBufferName, ReadAccess) )
        return false;

    // read buffer into memory
    m_isOpened = OnRead(conv);

    OnClose();

    return m_isOpened;
}

bool wxTextBuffer::Close()
{
    Clear();
    m_isOpened = false;

    return true;
}

bool wxTextBuffer::Write(wxTextFileType typeNew, const wxMBConv& conv)
{
    return OnWrite(typeNew, conv);
}

#endif // wxUSE_TEXTBUFFER
