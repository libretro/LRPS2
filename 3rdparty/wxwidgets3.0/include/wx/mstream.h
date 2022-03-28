/////////////////////////////////////////////////////////////////////////////
// Name:        wx/mstream.h
// Purpose:     Memory stream classes
// Author:      Guilhem Lavaux
// Modified by:
// Created:     11/07/98
// Copyright:   (c) Guilhem Lavaux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WXMMSTREAM_H__
#define _WX_WXMMSTREAM_H__

#include "wx/defs.h"

#if wxUSE_STREAMS

#include "wx/stream.h"

class WXDLLIMPEXP_BASE wxMemoryInputStream : public wxInputStream
{
public:
    wxMemoryInputStream(const void *data, size_t length);

    virtual ~wxMemoryInputStream();
    virtual wxFileOffset GetLength() const { return m_length; }
    virtual bool IsSeekable() const { return true; }

    virtual bool CanRead() const;

    wxStreamBuffer *GetInputStreamBuffer() const { return m_i_streambuf; }

protected:
    wxStreamBuffer *m_i_streambuf;

    size_t OnSysRead(void *buffer, size_t nbytes);
    wxFileOffset OnSysSeek(wxFileOffset pos, wxSeekMode mode);
    wxFileOffset OnSysTell() const;

private:
    size_t m_length;

    // copy ctor is implemented above: it copies the other stream in this one
    DECLARE_ABSTRACT_CLASS(wxMemoryInputStream)
    wxDECLARE_NO_ASSIGN_CLASS(wxMemoryInputStream);
};

#endif
  // wxUSE_STREAMS

#endif
  // _WX_WXMMSTREAM_H__
