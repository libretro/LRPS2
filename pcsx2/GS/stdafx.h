/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently

#pragma once

#include "Pcsx2Defs.h"

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#endif

#include <string>

#ifdef __x86_64__
#define _M_AMD64
#endif

// stdc

#include <cstddef>
#include <cstdarg>
#include <cstdlib>
#include <cfloat>
#include <cstring>

#include <memory>

#ifdef _MSC_VER
    #define GS_FORCEINLINE __forceinline

    #define ALIGN_STACK(n) alignas(n) int dummy__;

#else
#ifdef NDEBUG
    #define GS_FORCEINLINE __inline__ __attribute__((always_inline,unused))
#else
    #define GS_FORCEINLINE __attribute__((unused))
#endif

    #ifdef __GNUC__
        // GCC removes the variable as dead code and generates some warnings.
        // Stack is automatically realigned due to SSE/AVX operations
        #define ALIGN_STACK(n) (void)0;
    #else
        // TODO Check clang behavior
        #define ALIGN_STACK(n) alignas(n) int dummy__;
    #endif
#endif

#ifndef RESTRICT
    #if defined(_MSC_VER)
        #define RESTRICT __restrict
    #elif defined(__GNUC__)
        #define RESTRICT __restrict__
    #else
        #define RESTRICT
    #endif
#endif

#ifdef _M_AMD64
	// Yeah let use mips naming ;)
	#ifdef _WIN64
	#define a0 rcx
	#define a1 rdx
	#define a2 r8
	#define a3 r9
	#define t0 rdi
	#define t1 rsi
	#else
	#define a0 rdi
	#define a1 rsi
	#define a2 rdx
	#define a3 rcx
	#define t0 r8
	#define t1 r9
	#endif
#endif

// SSE
#if defined(__GNUC__)
// Convert GCC see define into GS define
#if defined(__AVX2__)
	#if defined(__x86_64__)
		#define _M_SSE 0x500 // TODO
	#else
		#define _M_SSE 0x501
	#endif
#elif defined(__AVX__)
	#define _M_SSE 0x500
#elif defined(__SSE4_1__)
	#define _M_SSE 0x401
#elif defined(__SSSE3__)
	#define _M_SSE 0x301
#elif defined(__SSE2__)
	#define _M_SSE 0x200
#endif
#endif

#if !defined(_M_SSE) && (!defined(_WIN32) || defined(_M_AMD64) || defined(_M_IX86_FP) && _M_IX86_FP >= 2)
	#define _M_SSE 0x200
#endif

#if _M_SSE >= 0x301
	#include <tmmintrin.h>
#endif

#if _M_SSE >= 0x401
	#include <smmintrin.h>
#endif

#if _M_SSE >= 0x500
	#include <immintrin.h>
#endif

#undef min
#undef max
#undef abs

#if !defined(_MSC_VER)
	// http://svn.reactos.org/svn/reactos/trunk/reactos/include/crt/mingw32/intrin_x86.h?view=markup
	GS_FORCEINLINE int _BitScanForward(unsigned long* const Index, const unsigned long Mask)
	{
		__asm__("bsfl %k[Mask], %k[Index]" : [Index] "=r" (*Index) : [Mask] "mr" (Mask) : "cc");
		return Mask ? 1 : 0;
	}
#endif

extern std::string format(const char* fmt, ...);

extern void* vmalloc(size_t size, bool code);
extern void vmfree(void* ptr, size_t size);

#include <libretro.h>
extern retro_hw_render_callback hw_render;
#define GL_DEFAULT_FRAMEBUFFER hw_render.get_current_framebuffer()
