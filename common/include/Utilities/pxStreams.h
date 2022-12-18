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

#include <wx/filefn.h>
#include <memory>

#include "Dependencies.h"

// --------------------------------------------------------------------------------------
//  pxStreamBase
// --------------------------------------------------------------------------------------
class pxStreamBase
{
    DeclareNoncopyableObject(pxStreamBase);

protected:
    // Filename of the stream, provided by the creator/caller.  This is typically used *only*
    // for generating comprehensive error messages when an error occurs (the stream name is
    // passed to the exception handlers).
    wxString m_filename;

public:
    pxStreamBase(const wxString &filename);
    virtual ~pxStreamBase() = default;

    // Implementing classes should return the base wxStream object (usually either a wxInputStream
    // or wxOputStream derivative).
    virtual wxStreamBase *GetWxStreamBase() const = 0;
    virtual void Close() = 0;
    virtual wxFileOffset Tell() const = 0;
    virtual wxFileOffset Seek(wxFileOffset ofs, wxSeekMode mode = wxFromStart) = 0;

    virtual wxFileOffset Length() const;
    bool IsOk() const;
    wxString GetStreamName() const { return m_filename; }
};

// --------------------------------------------------------------------------------------
//  pxInputStream
// --------------------------------------------------------------------------------------
class pxInputStream : public pxStreamBase
{
    DeclareNoncopyableObject(pxInputStream);

protected:
    std::unique_ptr<wxInputStream> m_stream_in;

public:
    pxInputStream(const wxString &filename, std::unique_ptr<wxInputStream> &input);
    pxInputStream(const wxString &filename, wxInputStream *input);

    virtual ~pxInputStream() = default;
    virtual void Read(void *dest, size_t size);

    void Close() { m_stream_in = nullptr; }

    virtual wxStreamBase *GetWxStreamBase() const;

    template <typename T>
    void Read(T &dest)
    {
        Read(&dest, sizeof(dest));
    }

    wxFileOffset Tell() const;
    wxFileOffset Seek(wxFileOffset ofs, wxSeekMode mode = wxFromStart);
};
