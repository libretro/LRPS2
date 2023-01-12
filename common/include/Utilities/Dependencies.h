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

// Dependencies.h : Contains classes required by all Utilities headers.
//   This file is included by most .h files provided by the Utilities class.

#include "pxForwardDefs.h"

// This should prove useful....
#define wxsFormat wxString::Format

// --------------------------------------------------------------------------------------
//  ImplementEnumOperators  (macro)
// --------------------------------------------------------------------------------------
// This macro implements ++/-- operators for any conforming enumeration.  In order for an
// enum to conform, it must have _FIRST and _COUNT members defined, and must have a full
// compliment of sequential members (no custom assignments) --- looking like so:
//
// enum Dummy {
//    Dummy_FIRST,
//    Dummy_Item = Dummy_FIRST,
//    Dummy_Crap,
//    Dummy_COUNT
// };
//
#define ImplementEnumOperators(enumName)                                                                             \
    static __fi enumName &operator++(enumName &src)                                                                  \
    {                                                                                                                \
        src = (enumName)((int)src + 1);                                                                              \
        return src;                                                                                                  \
    }                                                                                                                \
    static __fi enumName &operator--(enumName &src)                                                                  \
    {                                                                                                                \
        src = (enumName)((int)src - 1);                                                                              \
        return src;                                                                                                  \
    }                                                                                                                \
    static __fi enumName operator++(enumName &src, int)                                                              \
    {                                                                                                                \
        enumName orig = src;                                                                                         \
        src = (enumName)((int)src + 1);                                                                              \
        return orig;                                                                                                 \
    }                                                                                                                \
    static __fi enumName operator--(enumName &src, int)                                                              \
    {                                                                                                                \
        enumName orig = src;                                                                                         \
        src = (enumName)((int)src - 1);                                                                              \
        return orig;                                                                                                 \
    }                                                                                                                \
                                                                                                                     \
    static __fi bool operator<(const enumName &left, const pxEnumEnd_t &) { return (int)left < enumName##_COUNT; }   \
    static __fi bool operator!=(const enumName &left, const pxEnumEnd_t &) { return (int)left != enumName##_COUNT; } \
    static __fi bool operator==(const enumName &left, const pxEnumEnd_t &) { return (int)left == enumName##_COUNT; }

class pxEnumEnd_t
{
};
static const pxEnumEnd_t pxEnumEnd = {};

// --------------------------------------------------------------------------------------
//  DeclareNoncopyableObject
// --------------------------------------------------------------------------------------
// This macro provides an easy and clean method for ensuring objects are not copyable.
// Simply add the macro to the head or tail of your class declaration, and attempts to
// copy the class will give you a moderately obtuse compiler error that will have you
// scratching your head for 20 minutes.
//
// (... but that's probably better than having a weird invalid object copy having you
//  scratch your head for a day).
//
// Programmer's notes:
//  * We intentionally do NOT provide implementations for these methods, which should
//    never be referenced anyway.

//  * I've opted for macro form over multi-inherited class form (Boost style), because
//    the errors generated by the macro are considerably less voodoo.  The Boost-style
//    The macro reports the exact class that causes the copy failure, while Boost's class
//    approach just reports an error in whatever "NoncopyableObject" is inherited.
//
//  * This macro is the same as wxWidgets' DECLARE_NO_COPY_CLASS macro.  This one is free
//    of wx dependencies though, and has a nicer typeset. :)
//
#ifndef DeclareNoncopyableObject
#define DeclareNoncopyableObject(classname) \
private:                                    \
    explicit classname(const classname &);  \
    classname &operator=(const classname &)
#endif

#include <wx/string.h>
#include <wx/crt.h>

#if defined(_WIN32)
#include <wx/filefn.h>
#endif

#include <stdexcept>
#include <cstring> // string.h under c++
#include <cstdlib>
#include <algorithm>
#include <memory>
#include <atomic>
#include <thread>

#include "Pcsx2Defs.h"

// --------------------------------------------------------------------------------------
//  Handy Human-readable constants for common immediate values (_16kb -> _4gb)

static const sptr _1kb = 1024 * 1;
static const sptr _4kb = _1kb * 4;
static const sptr _16kb = _1kb * 16;
static const sptr _32kb = _1kb * 32;
static const sptr _64kb = _1kb * 64;
static const sptr _128kb = _1kb * 128;
static const sptr _256kb = _1kb * 256;

static const s64 _1mb = 1024 * 1024;
static const s64 _8mb = _1mb * 8;
static const s64 _16mb = _1mb * 16;
static const s64 _32mb = _1mb * 32;
static const s64 _64mb = _1mb * 64;
static const s64 _256mb = _1mb * 256;
static const s64 _1gb = _1mb * 1024;
static const s64 _4gb = _1gb * 4;

extern wxString fromUTF8(const char *src);

#include "Utilities/Exceptions.h"
#include "Utilities/ScopedAlloc.h"
