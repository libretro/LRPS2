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

#ifdef _WIN32

#define _WIN32_WINNT 0x0600

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

#include <windows.h>
#endif

#include <string>
#include <stdint.h>

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

    #define EXPORT_C_(type) extern "C" type __stdcall
    #define EXPORT_C EXPORT_C_(void)

    #define ALIGN_STACK(n) alignas(n) int dummy__;

#else

    #ifndef __fastcall
        #define __fastcall __attribute__((fastcall))
    #endif

    #define EXPORT_C_(type) extern "C" __attribute__((stdcall,externally_visible,visibility("default"))) type
    #define EXPORT_C EXPORT_C_(void)

    #ifdef __GNUC__
        #define __forceinline __inline__ __attribute__((always_inline,unused))
        // GCC removes the variable as dead code and generates some warnings.
        // Stack is automatically realigned due to SSE/AVX operations
        #define ALIGN_STACK(n) (void)0;
    #else
        // TODO Check clang behavior
        #define ALIGN_STACK(n) alignas(n) int dummy__;
    #endif


#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
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
// Convert GCC see define into GSdx define
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
	#if defined(__USE_ISOC11) && !defined(ASAN_WORKAROUND) // not supported yet on gcc 4.9

	#define _aligned_malloc(size, a) aligned_alloc(a, size)

	#else

	extern void* _aligned_malloc(size_t size, size_t alignment);

	#endif

	static inline void _aligned_free(void* p) { free(p); }

	// http://svn.reactos.org/svn/reactos/trunk/reactos/include/crt/mingw32/intrin_x86.h?view=markup

	__forceinline int _BitScanForward(unsigned long* const Index, const unsigned long Mask)
	{
#if defined(__GCC_ASM_FLAG_OUTPUTS__) && 0
		// Need GCC6 to test the code validity
		int flag;
		__asm__("bsfl %k[Mask], %k[Index]" : [Index] "=r" (*Index), "=@ccz" (flag) : [Mask] "mr" (Mask));
		return flag;
#else
		__asm__("bsfl %k[Mask], %k[Index]" : [Index] "=r" (*Index) : [Mask] "mr" (Mask) : "cc");
		return Mask ? 1 : 0;
#endif
	}
#endif

extern std::string format(const char* fmt, ...);

extern void* vmalloc(size_t size, bool code);
extern void vmfree(void* ptr, size_t size);

extern void* fifo_alloc(size_t size, size_t repeat);
extern void fifo_free(void* ptr, size_t size, size_t repeat);

#include <libretro.h>
extern retro_hw_render_callback hw_render;
#define GL_DEFAULT_FRAMEBUFFER hw_render.get_current_framebuffer()
