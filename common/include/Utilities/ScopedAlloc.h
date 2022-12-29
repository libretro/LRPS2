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

#include "Pcsx2Types.h"
#include <cstddef>

//////////////////////////////////////////////////////////////////////////////////////////
// Safe deallocation macros -- checks pointer validity (non-null) when needed, and sets
// pointer to null after deallocation.

#define safe_delete(ptr) \
    ((void)(delete (ptr)), (ptr) = NULL)

#define safe_delete_array(ptr) \
    ((void)(delete[](ptr)), (ptr) = NULL)

// No checks for NULL -- wxWidgets says it's safe to skip NULL checks and it runs on
// just about every compiler and libc implementation of any recentness.
#define safe_free(ptr) \
    ((void)(free(ptr), !!0), (ptr) = NULL)
//((void) (( ( (ptr) != NULL ) && (free( ptr ), !!0) ), (ptr) = NULL))

// Implementation note: all known implementations of AlignedFree check the pointer for
// NULL status (our implementation under GCC, and microsoft's under MSVC), so no need to
// do it here.
#define safe_aligned_free(ptr) \
    ((void)(AlignedFree(ptr), (ptr) = NULL))

#ifdef _MSC_VER
#define AlignedMalloc(a, b) _aligned_malloc(a, b)
#define AlignedFree(a) _aligned_free(a)
#define pcsx2_aligned_realloc(a, b, c, d) _aligned_realloc(a, b, c)
#else
extern void *AlignedMalloc(size_t size, size_t align);
extern void AlignedFree(void *pmem);
extern void *pcsx2_aligned_realloc(void *handle, size_t new_size, size_t align, size_t old_size);
#endif

// --------------------------------------------------------------------------------------
//  pxDoOutOfMemory
// --------------------------------------------------------------------------------------

typedef void FnType_OutOfMemory(uptr blocksize);
typedef FnType_OutOfMemory *Fnptr_OutOfMemory;

// This method is meant to be assigned by applications that link against pxWex.  It is called
// (invoked) prior to most pxWex built-in memory/array classes throwing exceptions, and can be
// used by an application to remove unneeded memory allocations and/or reduce internal cache
// reserves.
//
// Example: PCSX2 uses several bloated recompiler code caches.  Larger caches improve performance,
// however a rouge cache growth could cause memory constraints in the operating system.  If an out-
// of-memory error occurs, PCSX2's implementation of this function attempts to reset all internal
// recompiler caches.  This can typically free up 100-150 megs of memory, and will allow the app
// to continue running without crashing or hanging the operating system, etc.
//
extern Fnptr_OutOfMemory pxDoOutOfMemory;


// --------------------------------------------------------------------------------------
//  BaseScopedAlloc
// --------------------------------------------------------------------------------------
// Base class that allows various ScopedMalloc types to be passed to functions that act
// on them.
//
// Rationale: This class and the derived varieties are provided as a simple autonomous self-
// destructing container for malloc.  The entire class is almost completely dependency free,
// and thus can be included everywhere and anywhere without dependency hassles.
//
template <typename T>
class BaseScopedAlloc
{
protected:
    T *m_buffer;
    uint m_size;

public:
    BaseScopedAlloc()
    {
        m_buffer = NULL;
        m_size = 0;
    }

    virtual ~BaseScopedAlloc()
    {
    }

public:
    size_t GetSize() const { return m_size; }
    size_t GetLength() const { return m_size; }

    // Allocates the object to the specified size.  If an existing allocation is in
    // place, it is freed and replaced with the new allocation, and all data is lost.
    // Parameter:
    //   newSize - size of the new allocation, in elements (not bytes!).  If the specified
    //     size is 0, the the allocation is freed, same as calling Free().
    virtual void Alloc(size_t newsize) = 0;

    // Re-sizes the allocation to the requested size, without any data loss.
    // Parameter:
    //   newSize - size of the new allocation, in elements (not bytes!).  If the specified
    //     size is 0, the the allocation is freed, same as calling Free().
    virtual void Resize(size_t newsize) = 0;

    void Free()
    {
        Alloc(0);
    }

    // Makes enough room for the requested size.  Existing data in the array is retained.
    void MakeRoomFor(uint size)
    {
        if (size <= m_size)
            return;
        Resize(size);
    }

    T *GetPtr(uint idx = 0) const
    {
        return &m_buffer[idx];
    }

    T &operator[](uint idx)
    {
        return m_buffer[idx];
    }

    const T &operator[](uint idx) const
    {
        return m_buffer[idx];
    }
};

// --------------------------------------------------------------------------------------
//  ScopedAlloc
// --------------------------------------------------------------------------------------
// A simple container class for a standard malloc allocation.  By default, no bounds checking
// is performed, and there is no option for enabling bounds checking.  If bounds checking and
// other features are needed, use the more robust SafeArray<> instead.
//
// See docs for BaseScopedAlloc for details and rationale.
//
template <typename T>
class ScopedAlloc : public BaseScopedAlloc<T>
{
    typedef BaseScopedAlloc<T> _parent;

public:
    ScopedAlloc(size_t size = 0)
        : _parent()
    {
        Alloc(size);
    }

    virtual ~ScopedAlloc()
    {
        safe_free(this->m_buffer);
    }

    virtual void Alloc(size_t newsize)
    {
        safe_free(this->m_buffer);
        this->m_size = newsize;
        if (!this->m_size)
            return;

        this->m_buffer = (T *)malloc(this->m_size * sizeof(T));
    }

    virtual void Resize(size_t newsize)
    {
        this->m_size = newsize;
        this->m_buffer = (T *)realloc(this->m_buffer, this->m_size * sizeof(T));
    }

    using _parent::operator[];
};

// --------------------------------------------------------------------------------------
//  ScopedAlignedAlloc
// --------------------------------------------------------------------------------------
// A simple container class for an aligned allocation.  By default, no bounds checking is
// performed, and there is no option for enabling bounds checking.  If bounds checking and
// other features are needed, use the more robust SafeArray<> instead.
//
// See docs for BaseScopedAlloc for details and rationale.
//
template <typename T, uint align>
class ScopedAlignedAlloc : public BaseScopedAlloc<T>
{
    typedef BaseScopedAlloc<T> _parent;

public:
    ScopedAlignedAlloc(size_t size = 0)
        : _parent()
    {
        Alloc(size);
    }

    virtual ~ScopedAlignedAlloc()
    {
        safe_aligned_free(this->m_buffer);
    }

    virtual void Alloc(size_t newsize)
    {
        safe_aligned_free(this->m_buffer);
        this->m_size = newsize;
        if (!this->m_size)
            return;

        this->m_buffer = (T *)AlignedMalloc(this->m_size * sizeof(T), align);
    }

    virtual void Resize(size_t newsize)
    {
        this->m_buffer = (T *)pcsx2_aligned_realloc(this->m_buffer, newsize * sizeof(T), align, this->m_size * sizeof(T));
        this->m_size = newsize;
    }

    using _parent::operator[];
};
