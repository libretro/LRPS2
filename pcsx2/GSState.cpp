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

#include "PrecompiledHeader.h"

#ifdef PCSX2_DEVBUILD

#include <list>
#include <memory>

#include "GS.h"

// fixme - need to take this concept and make it MTGS friendly.
#ifdef _STGS_GSSTATE_CODE
void GSGIFTRANSFER1(u32 *pMem, u32 addr) {
	GSgifTransfer1(pMem, addr);
}

void GSGIFTRANSFER2(u32 *pMem, u32 size) {
	GSgifTransfer2(pMem, size);
}

void GSGIFTRANSFER3(u32 *pMem, u32 size) {
	GSgifTransfer3(pMem, size);
}

__fi void GSVSYNC(void) {
}
#endif

#endif
