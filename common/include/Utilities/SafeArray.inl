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

#include "SafeArray.h"

// Internal constructor for use by derived classes.  This allows a derived class to
// use its own memory allocation (with an aligned memory, for example).
template <typename T>
SafeArray<T>::SafeArray(T *allocated_mem, int initSize)
{
    ChunkSize = DefaultChunkSize;
    m_ptr     = allocated_mem;
    m_size    = initSize;
}

template <typename T>
T *SafeArray<T>::_virtual_realloc(int newsize)
{
    return (T *)((m_ptr == NULL) ?
                          malloc(newsize * sizeof(T)) :
                          realloc(m_ptr, newsize * sizeof(T)));
}

template <typename T>
SafeArray<T>::~SafeArray()
{
    safe_free(m_ptr);
}

template <typename T>
SafeArray<T>::SafeArray(void)
{
    ChunkSize = DefaultChunkSize;
    m_ptr     = NULL;
    m_size    = 0;
}

template <typename T>
SafeArray<T>::SafeArray(int initialSize)
{
    ChunkSize = DefaultChunkSize;
    m_ptr     = (initialSize == 0) ? NULL : (T *)malloc(initialSize * sizeof(T));
    m_size    = initialSize;
}

template <typename T>
T *SafeArray<T>::_getPtr(uint i) const
{
    return &m_ptr[i];
}

// reallocates the array to the explicit size.  Can be used to shrink or grow an
// array, and bypasses the internal threshold growth indicators.
template <typename T>
void SafeArray<T>::ExactAlloc(int newsize)
{
    if (newsize == m_size)
        return;

    /* TODO/FIXME - do a safer way to handle failure instead of 
     * throwing exceptions */
    m_ptr  = _virtual_realloc(newsize);
    m_size = newsize;
}

template <typename T>
SafeArray<T> *SafeArray<T>::Clone() const
{
    SafeArray<T> *retval = new SafeArray<T>(m_size);
    memcpy(retval->GetPtr(), m_ptr, sizeof(T) * m_size);
    return retval;
}
