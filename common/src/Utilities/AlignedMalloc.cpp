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

// This module contains implementations of _aligned_malloc for platforms that don't have
// it built into their CRT/libc.

#include "PrecompiledHeader.h"

void *__fastcall pcsx2_aligned_realloc(void *handle, size_t new_size, size_t align, size_t old_size)
{
    void *newbuf = _aligned_malloc(new_size, align);
    if (newbuf != NULL && handle != NULL)
    {
        memcpy(newbuf, handle, std::min(old_size, new_size));
        _aligned_free(handle);
    }
    return newbuf;
}
