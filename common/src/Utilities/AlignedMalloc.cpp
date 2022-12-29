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

#include "Dependencies.h"

#include "MemcpyFast.h"
#include "Exceptions.h"
#include "SafeArray.h"
#include "General.h"

#ifndef _MSC_VER
void *AlignedMalloc(size_t size, size_t alignment)
{
#ifdef __MINGW32__
	return __mingw_aligned_malloc(size, alignment);
#elif defined(_WIN32)
	return _aligned_malloc(size, alignment);
#else
	void *p;
	int ret = posix_memalign(&p, alignment, size);
	return (ret == 0) ? p : 0;
#endif
}

void *pcsx2_aligned_realloc(void *handle, size_t new_size, size_t align, size_t old_size)
{
#ifdef _MSC_VER
    return _aligned_realloc(handle, new_size, align);
#else
    void *newbuf = AlignedMalloc(new_size, align);

    if (newbuf && handle)
    {
        memcpy(newbuf, handle, std::min(old_size, new_size));
        AlignedFree(handle);
    }
    return newbuf;
#endif
}

__fi void AlignedFree(void *p)
{
#ifdef __MINGW32__
	__mingw_aligned_free(p);
#elif defined(_MSC_VER)
	_aligned_free(p);
#else
	free(p);
#endif
}
#endif
