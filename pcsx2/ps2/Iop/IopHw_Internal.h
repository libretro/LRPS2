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

#include "Common.h"
#include "IopHw.h"

namespace IopMemory {
namespace Internal {

//////////////////////////////////////////////////////////////////////////////////////////
// Masking helper so that I can use the fully qualified address for case statements.
// Switches are based on the bottom 12 bits only, since MSVC tends to optimize switches
// better when it has a limited width operand to work with. :)
//
#define pgmsk( src ) ( (src) & 0x0fff )
#define mcase( src ) case pgmsk(src)

} };
