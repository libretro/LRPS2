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

#include "Dependencies.h"

#include "pxStreams.h"

#include <wx/stream.h>

#include <errno.h>

// --------------------------------------------------------------------------------------
//  pxStreamBase  (implementations)
// --------------------------------------------------------------------------------------
pxStreamBase::pxStreamBase(const wxString &filename)
    : m_filename(filename)
{
}

bool pxStreamBase::IsOk() const
{
    wxStreamBase *woot = GetWxStreamBase();
    return woot && woot->IsOk();
}

wxFileOffset pxStreamBase::Length() const
{
    if (!GetWxStreamBase())
        return 0;
    return GetWxStreamBase()->GetLength();
}

// --------------------------------------------------------------------------------------
//  pxInputStream  (implementations)
// --------------------------------------------------------------------------------------
// Interface for reading data from a gzip stream.
//

pxInputStream::pxInputStream(const wxString &filename, std::unique_ptr<wxInputStream> &input)
    : pxStreamBase(filename)
    , m_stream_in(std::move(input))
{
}

pxInputStream::pxInputStream(const wxString &filename, wxInputStream *input)
    : pxStreamBase(filename)
    , m_stream_in(input)
{
}

wxStreamBase *pxInputStream::GetWxStreamBase() const { return m_stream_in.get(); }

wxFileOffset pxInputStream::Tell() const
{
    return m_stream_in->TellI();
}

wxFileOffset pxInputStream::Seek(wxFileOffset ofs, wxSeekMode mode)
{
    return m_stream_in->SeekI(ofs, mode);
}

void pxInputStream::Read(void *dest, size_t size)
{
    m_stream_in->Read(dest, size);
    if (m_stream_in->GetLastError() == wxSTREAM_READ_ERROR)
    {
        int err = errno;
        if (!err)
            throw Exception::BadStream(m_filename).SetDiagMsg(L"Cannot read from file (bad file handle?)");
    }

    // IMPORTANT!  The underlying file/source Eof() stuff is not really reliable, so we
    // must always use the explicit check against the number of bytes read to determine
    // end-of-stream conditions.

    if ((size_t)m_stream_in->LastRead() < size)
        throw Exception::EndOfStream(m_filename);
}
