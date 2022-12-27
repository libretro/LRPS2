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
