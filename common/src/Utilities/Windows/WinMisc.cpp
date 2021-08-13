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

#pragma comment(lib, "User32.lib")

#include "PrecompiledHeader.h"
#include "RedtapeWindows.h"
#include <VersionHelpers.h>

// --------------------------------------------------------------------------------------
//  Exception::WinApiError   (implementations)
// --------------------------------------------------------------------------------------
Exception::WinApiError::WinApiError()
{
    ErrorId = GetLastError();
    m_message_diag = L"Unspecified Windows API error.";
}

wxString Exception::WinApiError::GetMsgFromWindows() const
{
    if (!ErrorId)
        return L"No valid error number was assigned to this exception!";

    const DWORD BUF_LEN = 2048;
    TCHAR t_Msg[BUF_LEN];
    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, ErrorId, 0, t_Msg, BUF_LEN, 0))
        return wxsFormat(L"Win32 Error #%d: %s", ErrorId, t_Msg);

    return wxsFormat(L"Win32 Error #%d (no text msg available)", ErrorId);
}

wxString Exception::WinApiError::FormatDiagnosticMessage() const
{
    return m_message_diag + L"\n\t" + GetMsgFromWindows();
}
