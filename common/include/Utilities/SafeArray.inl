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
// Throws:
//   Exception::OutOfMemory if the allocated_mem pointer is NULL.
template <typename T>
SafeArray<T>::SafeArray(const wxChar *name, T *allocated_mem, int initSize)
    : Name(name)
{
    ChunkSize = DefaultChunkSize;
    m_ptr = allocated_mem;
    m_size = initSize;

    if (m_ptr == NULL)
        throw Exception::OutOfMemory(name)
            .SetDiagMsg(wxsFormat(L"Called from 'SafeArray::ctor' [size=%d]", initSize));
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
SafeArray<T>::SafeArray(const wxChar *name)
    : Name(name)
{
    ChunkSize = DefaultChunkSize;
    m_ptr = NULL;
    m_size = 0;
}

template <typename T>
SafeArray<T>::SafeArray(int initialSize, const wxChar *name)
    : Name(name)
{
    ChunkSize = DefaultChunkSize;
    m_ptr = (initialSize == 0) ? NULL : (T *)malloc(initialSize * sizeof(T));
    m_size = initialSize;

    if ((initialSize != 0) && (m_ptr == NULL))
        throw Exception::OutOfMemory(name)
            .SetDiagMsg(wxsFormat(L"Called from 'SafeArray::ctor' [size=%d]", initialSize));
}

// Clears the contents of the array to zero, and frees all memory allocations.
template <typename T>
void SafeArray<T>::Dispose()
{
    m_size = 0;
    safe_free(m_ptr);
}

template <typename T>
T *SafeArray<T>::_getPtr(uint i) const
{
    IndexBoundsAssumeDev(WX_STR(Name), i, m_size);
    return &m_ptr[i];
}

// reallocates the array to the explicit size.  Can be used to shrink or grow an
// array, and bypasses the internal threshold growth indicators.
template <typename T>
void SafeArray<T>::ExactAlloc(int newsize)
{
    if (newsize == m_size)
        return;

    m_ptr = _virtual_realloc(newsize);
    if (m_ptr == NULL)
        throw Exception::OutOfMemory(Name)
            .SetDiagMsg(wxsFormat(L"Called from 'SafeArray::ExactAlloc' [oldsize=%d] [newsize=%d]", m_size, newsize));

    m_size = newsize;
}

template <typename T>
SafeArray<T> *SafeArray<T>::Clone() const
{
    SafeArray<T> *retval = new SafeArray<T>(m_size);
    memcpy(retval->GetPtr(), m_ptr, sizeof(T) * m_size);
    return retval;
}


// --------------------------------------------------------------------------------------
//  SafeAlignedArray<T>  (implementations)
// --------------------------------------------------------------------------------------

template <typename T, uint Alignment>
T *SafeAlignedArray<T, Alignment>::_virtual_realloc(int newsize)
{
    return (T *)((this->m_ptr == NULL) ?
                     _aligned_malloc(newsize * sizeof(T), Alignment) :
                     pcsx2_aligned_realloc(this->m_ptr, newsize * sizeof(T), Alignment, this->m_size * sizeof(T)));
}

// Appends "(align: xx)" to the name of the allocation in devel builds.
// Maybe useful,maybe not... no harm in attaching it. :D

template <typename T, uint Alignment>
SafeAlignedArray<T, Alignment>::~SafeAlignedArray()
{
    safe_aligned_free(this->m_ptr);
    // mptr is set to null, so the parent class's destructor won't re-free it.
}

template <typename T, uint Alignment>
SafeAlignedArray<T, Alignment>::SafeAlignedArray(int initialSize, const wxChar *name)
    : SafeArray<T>::SafeArray(
          name,
          (T *)_aligned_malloc(initialSize * sizeof(T), Alignment),
          initialSize)
{
}

template <typename T, uint Alignment>
SafeAlignedArray<T, Alignment> *SafeAlignedArray<T, Alignment>::Clone() const
{
    SafeAlignedArray<T, Alignment> *retval = new SafeAlignedArray<T, Alignment>(this->m_size);
    memcpy(retval->GetPtr(), this->m_ptr, sizeof(T) * this->m_size);
    return retval;
}
