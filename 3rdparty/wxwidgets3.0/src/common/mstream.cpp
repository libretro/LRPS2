/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/mstream.cpp
// Purpose:     "Memory stream" classes
// Author:      Guilhem Lavaux
// Modified by: VZ (23.11.00): general code review
// Created:     04/01/98
// Copyright:   (c) Guilhem Lavaux
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

#if wxUSE_STREAMS

#include "wx/mstream.h"

#ifndef   WX_PRECOMP
    #include  "wx/stream.h"
#endif  //WX_PRECOMP

#include <stdlib.h>

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxMemoryInputStream
// ----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxMemoryInputStream, wxInputStream)

wxMemoryInputStream::wxMemoryInputStream(const void *data, size_t len)
{
    m_i_streambuf = new wxStreamBuffer(wxStreamBuffer::read);
    m_i_streambuf->SetBufferIO(const_cast<void *>(data), len);
    m_i_streambuf->SetIntPosition(0); // seek to start pos
    m_i_streambuf->Fixed(true);

    m_length = len;
}

bool wxMemoryInputStream::CanRead() const
{
    return m_i_streambuf->GetIntPosition() != m_length;
}

wxMemoryInputStream::~wxMemoryInputStream()
{
    delete m_i_streambuf;
}

size_t wxMemoryInputStream::OnSysRead(void *buffer, size_t nbytes)
{
    size_t pos = m_i_streambuf->GetIntPosition();
    if ( pos == m_length )
    {
        m_lasterror = wxSTREAM_EOF;

        return 0;
    }

    m_i_streambuf->Read(buffer, nbytes);
    m_lasterror = wxSTREAM_NO_ERROR;

    return m_i_streambuf->GetIntPosition() - pos;
}

wxFileOffset wxMemoryInputStream::OnSysSeek(wxFileOffset pos, wxSeekMode mode)
{
    return m_i_streambuf->Seek(pos, mode);
}

wxFileOffset wxMemoryInputStream::OnSysTell() const
{
    return m_i_streambuf->Tell();
}

#endif // wxUSE_STREAMS
