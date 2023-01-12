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

#ifndef __PCSX2DEFS_H__
#define __PCSX2DEFS_H__

#ifdef __CYGWIN__
#define __linux__
#endif

// make sure __POSIX__ is defined for all systems where we assume POSIX
// compliance
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__) || defined(__CYGWIN__) || defined(__LINUX__)
#if !defined(__POSIX__)
#define __POSIX__ 1
#endif
#endif

#include "Pcsx2Types.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

// --------------------------------------------------------------------------------------
// __aligned / __aligned16 / __pagealigned
// --------------------------------------------------------------------------------------
// GCC Warning!  The GCC linker (LD) typically fails to assure alignment of class members.
// If you want alignment to be assured, the variable must either be a member of a struct
// or a static global.
//
// __pagealigned is equivalent to __aligned(0x1000), and is used to align a dynarec code
// buffer to a page boundary (allows the use of execution-enabled mprotect).
//
// General Performance Warning: Any function that specifies alignment on a local (stack)
// variable will have to align the stack frame on enter, and restore it on exit (adds
// overhead).  Furthermore, compilers cannot inline functions that have aligned local
// vars.  So use local var alignment with much caution.
//

// Defines the memory page size for the target platform at compilation.  All supported platforms
// (which means Intel only right now) have a 4k granularity.
#define PCSX2_PAGESIZE 0x1000

// --------------------------------------------------------------------------------------
//  Microsoft Visual Studio
// --------------------------------------------------------------------------------------
#ifdef _MSC_VER

// Using these breaks compat with VC2005; so we're not using it yet.
//#	define __pack_begin		__pragma(pack(1))
//#	define __pack_end		__pragma(pack())

// This is the 2005/earlier compatible packing define, which must be used in conjunction
// with #ifdef _MSC_VER/#pragma pack() directives (ugly).
#define __packed

#define __aligned(alig) __declspec(align(alig))
#define __aligned16 __declspec(align(16))
#define __aligned32 __declspec(align(32))
#define __pagealigned __declspec(align(PCSX2_PAGESIZE))

#define __noinline __declspec(noinline)
#define __noreturn __declspec(noreturn)

// Don't know if there are Visual C++ equivalents of these.
#define likely(x) (!!(x))
#define unlikely(x) (!!(x))

#define CALLBACK __stdcall

#else

// --------------------------------------------------------------------------------------
//  GCC / Intel Compilers Section
// --------------------------------------------------------------------------------------

#ifndef __packed
#define __packed __attribute__((packed))
#endif
#ifndef __aligned
#define __aligned(alig) __attribute__((aligned(alig)))
#endif
#define __aligned16 __attribute__((aligned(16)))
#define __aligned32 __attribute__((aligned(32)))
#define __pagealigned __attribute__((aligned(PCSX2_PAGESIZE)))

#define __assume(cond) ((void)0) // GCC has no equivalent for __assume
#define CALLBACK __attribute__((stdcall))

// Inlining note: GCC needs ((unused)) attributes defined on inlined functions to suppress
// warnings when a static inlined function isn't used in the scope of a single file (which
// happens *by design* like all the friggen time >_<)

#define _inline __inline__ __attribute__((unused))
#ifdef NDEBUG
#define __forceinline __attribute__((always_inline, unused))
#else
#define __forceinline __attribute__((unused))
#endif
#ifndef __noinline
#define __noinline __attribute__((noinline))
#endif
#ifndef __noreturn
#define __noreturn __attribute__((noreturn))
#endif
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// --------------------------------------------------------------------------------------
// __releaseinline / __ri -- a forceinline macro that is enabled for RELEASE/PUBLIC builds ONLY.
// --------------------------------------------------------------------------------------
// This is useful because forceinline can make certain types of debugging problematic since
// functions that look like they should be called won't breakpoint since their code is
// inlined, and it can make stack traces confusing or near useless.
//
// Use __releaseinline for things which are generally large functions where trace debugging
// from Devel builds is likely useful; but which should be inlined in an optimized Release
// environment.
//
#define __releaseinline __forceinline

#define __ri __releaseinline
#define __fi __forceinline

#endif
