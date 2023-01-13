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

#ifndef __PCSX2TYPES_H__
#define __PCSX2TYPES_H__

// --------------------------------------------------------------------------------------
//  Forward declarations
// --------------------------------------------------------------------------------------
// Forward declarations for wxWidgets-supporting features.
// If you aren't linking against wxWidgets libraries, then functions that
// depend on these types will not be usable (they will yield linker errors).

#ifdef __cplusplus
class wxString;
class FastFormatAscii;
class FastFormatUnicode;
#endif

// --------------------------------------------------------------------------------------
//  Basic Atomic Types
// --------------------------------------------------------------------------------------

#include <stdint.h>

// Note: char does not have a default sign, unlike other types. As we actually want
// char and not signed char in pcsx2, we define s8 to char

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uintptr_t uptr;
typedef intptr_t sptr;

typedef unsigned int uint;

// --------------------------------------------------------------------------------------
//  u128 / s128 - A rough-and-ready cross platform 128-bit datatype, Non-SSE style.
// --------------------------------------------------------------------------------------
// Note: These structs don't provide any additional constructors because C++ doesn't allow
// the use of datatypes with constructors in unions (and since unions aren't the primary
// uses of these types, that means we can't have constructors). Embedded functions for
// performing explicit conversion from 64 and 32 bit values are provided instead.
//
#ifdef __cplusplus
union u128
{
    struct
    {
        u64 lo;
        u64 hi;
    };

    u64 _u64[2];
    u32 _u32[4];
    u16 _u16[8];
    u8 _u8[16];

    // Explicit conversion from u32. Zero-extends the source through 128 bits.
    static u128 From32(u32 src)
    {
        u128 retval;
        retval._u32[0] = src;
        retval._u32[1] = 0;
        retval.hi = 0;
        return retval;
    }

    operator u32() const { return _u32[0]; }
    operator u16() const { return _u16[0]; }
    operator u8() const { return _u8[0]; }

    bool operator==(const u128 &right) const
    {
        return (lo == right.lo) && (hi == right.hi);
    }

    bool operator!=(const u128 &right) const
    {
        return (lo != right.lo) || (hi != right.hi);
    }
};

struct s128
{
    s64 lo;
    s64 hi;

    operator u32() const { return (s32)lo; }
    operator u16() const { return (s16)lo; }
    operator u8() const { return (s8)lo; }

    bool operator==(const s128 &right) const
    {
        return (lo == right.lo) && (hi == right.hi);
    }

    bool operator!=(const s128 &right) const
    {
        return (lo != right.lo) || (hi != right.hi);
    }
};

#else

typedef union _u128_t
{
    struct
    {
        u64 lo;
        u64 hi;
    };

    u64 _u64[2];
    u32 _u32[4];
    u16 _u16[8];
    u8 _u8[16];
} u128;

typedef union _s128_t
{
    u64 lo;
    s64 hi;
} s128;

#endif

typedef void FnType_Void(void);
typedef FnType_Void* Fnptr_Void;

// freeze modes:
#define FREEZE_LOAD 0
#define FREEZE_SAVE 1
#define FREEZE_SIZE 2

typedef struct
{
    int size;
    s8 *data;
} freezeData;

#endif
