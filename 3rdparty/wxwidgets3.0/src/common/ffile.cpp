/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/ffile.cpp
// Purpose:     wxFFile encapsulates "FILE *" IO stream
// Author:      Vadim Zeitlin
// Modified by:
// Created:     14.07.99
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
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

#if wxUSE_FFILE

#ifndef WX_PRECOMP
    #include "wx/crt.h"
#endif

#include "wx/ffile.h"

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// opening the file
// ----------------------------------------------------------------------------

wxFFile::wxFFile(const wxString& filename, const wxString& mode)
{
    m_fp = NULL;

    (void)Open(filename, mode);
}

bool wxFFile::Open(const wxString& filename, const wxString& mode)
{
    FILE* const fp = wxFopen(filename, mode);

    if ( !fp )
        return false;

    Attach(fp, filename);

    return true;
}

bool wxFFile::Close()
{
    if ( IsOpened() )
    {
        if ( fclose(m_fp) != 0 )
            return false;

        m_fp = NULL;
    }

    return true;
}

// ----------------------------------------------------------------------------
// read/write
// ----------------------------------------------------------------------------

size_t wxFFile::Read(void *pBuf, size_t nCount)
{
    return fread(pBuf, 1, nCount, m_fp);
}

size_t wxFFile::Write(const void *pBuf, size_t nCount)
{
    return fwrite(pBuf, 1, nCount, m_fp);
}

bool wxFFile::Write(const wxString& s, const wxMBConv& conv)
{
    // Writing nothing always succeeds -- and simplifies the check for
    // conversion failure below.
    if ( s.empty() )
        return true;

    const wxWX2MBbuf buf = s.mb_str(conv);
#if wxUSE_UNICODE
    const size_t size = buf.length();
    if ( !size ) // conversil failed
        return false;
#else
    const size_t size = s.length();
#endif
    return fwrite(buf, 1, size, m_fp) == size;
}

bool wxFFile::Flush()
{
    if ( IsOpened() )
    {
        if ( fflush(m_fp) != 0 )
            return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
// seeking
// ----------------------------------------------------------------------------

bool wxFFile::Seek(wxFileOffset ofs, wxSeekMode mode)
{
    int origin;
    switch ( mode )
    {
        default:
            // still fall through

        case wxFromStart:
            origin = SEEK_SET;
            break;

        case wxFromCurrent:
            origin = SEEK_CUR;
            break;

        case wxFromEnd:
            origin = SEEK_END;
            break;
    }

#ifndef wxHAS_LARGE_FFILES
    if ((long)ofs != ofs)
        return false;
#endif
    if ( wxFseek(m_fp, (long)ofs, origin) != 0 )
        return false;

    return true;
}

wxFileOffset wxFFile::Tell() const
{
    return wxFtell(m_fp);
}

wxFileOffset wxFFile::Length() const
{
    wxFFile& self = *const_cast<wxFFile *>(this);

    wxFileOffset posOld = Tell();
    if ( posOld != wxInvalidOffset )
    {
        if ( self.SeekEnd() )
        {
            wxFileOffset len = Tell();

            (void)self.Seek(posOld);

            return len;
        }
    }

    return wxInvalidOffset;
}

#endif // wxUSE_FFILE
