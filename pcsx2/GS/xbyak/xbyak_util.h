/* Copyright (c) 2007 MITSUNARI Shigeo
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
* Neither the name of the copyright owner nor the names of its contributors may
* be used to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef XBYAK_XBYAK_UTIL_H_
#define XBYAK_XBYAK_UTIL_H_

#include "xbyak.h"

#ifdef _MSC_VER
	#if (_MSC_VER < 1400) && defined(XBYAK32)
		static inline __declspec(naked) void __cpuid(int[4], int)
		{
			__asm {
				push	ebx
				push	esi
				mov		eax, dword ptr [esp + 4 * 2 + 8] // eaxIn
				cpuid
				mov		esi, dword ptr [esp + 4 * 2 + 4] // data
				mov		dword ptr [esi], eax
				mov		dword ptr [esi + 4], ebx
				mov		dword ptr [esi + 8], ecx
				mov		dword ptr [esi + 12], edx
				pop		esi
				pop		ebx
				ret
			}
		}
	#else
		#include <intrin.h> // for __cpuid
	#endif
#else
	#ifndef __GNUC_PREREQ
    	#define __GNUC_PREREQ(major, minor) ((((__GNUC__) << 16) + (__GNUC_MINOR__)) >= (((major) << 16) + (minor)))
	#endif
	#if __GNUC_PREREQ(4, 3) && !defined(__APPLE__)
		#include <cpuid.h>
	#else
		#if defined(__APPLE__) && defined(XBYAK32) // avoid err : can't find a register in class `BREG' while reloading `asm'
			#define __cpuid(eaxIn, a, b, c, d) __asm__ __volatile__("pushl %%ebx\ncpuid\nmovl %%ebp, %%esi\npopl %%ebx" : "=a"(a), "=S"(b), "=c"(c), "=d"(d) : "0"(eaxIn))
			#define __cpuid_count(eaxIn, ecxIn, a, b, c, d) __asm__ __volatile__("pushl %%ebx\ncpuid\nmovl %%ebp, %%esi\npopl %%ebx" : "=a"(a), "=S"(b), "=c"(c), "=d"(d) : "0"(eaxIn), "2"(ecxIn))
		#else
			#define __cpuid(eaxIn, a, b, c, d) __asm__ __volatile__("cpuid\n" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(eaxIn))
			#define __cpuid_count(eaxIn, ecxIn, a, b, c, d) __asm__ __volatile__("cpuid\n" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(eaxIn), "2"(ecxIn))
		#endif
	#endif
#endif

#ifdef _MSC_VER
extern "C" unsigned __int64 __xgetbv(int);
#endif

namespace Xbyak { namespace util {

/* GCC uses AVX/SSE4 operation to handle the uint64_t type.
 *
 * It is quite annoying because the purpose of the code is to test the support
 * of AVX/SSEn
 *
 * So far, we don't need other ISA on i386 so I hacked the code to limit the
 * type to 32 bits.
 */

/**
	CPU detection class
*/
class Cpu {
#ifdef XBYAK64
	uint64_t type_;
#else
	uint32_t type_;
#endif
	unsigned int get32bitAsBE(const char *x) const
	{
		return x[0] | (x[1] << 8) | (x[2] << 16) | (x[3] << 24);
	}
	void setFamily()
	{
		unsigned int data[4];
		getCpuid(1, data);
		stepping = data[0]          & ((1U << 4) - 1);
		model = (data[0] >> 4)      & ((1U << 4) - 1);
		family = (data[0] >> 8)     & ((1U << 4) - 1);
		extModel = (data[0] >> 16)  & ((1U << 4) - 1);
		extFamily = (data[0] >> 20) & ((1U << 8) - 1);
		if (family == 0x0f) {
			displayFamily = family + extFamily;
		} else {
			displayFamily = family;
		}
		if (family == 6 || family == 0x0f) {
			displayModel = (extModel << 4) + model;
		} else {
			displayModel = model;
		}
	}
public:
	int model;
	int family;
	int stepping;
	int extModel;
	int extFamily;
	int displayFamily; // family + extFamily
	int displayModel; // model + extModel
	/*
		data[] = { eax, ebx, ecx, edx }
	*/
	static inline void getCpuid(unsigned int eaxIn, unsigned int data[4])
	{
#ifdef _MSC_VER
		__cpuid(reinterpret_cast<int*>(data), eaxIn);
#else
		__cpuid(eaxIn, data[0], data[1], data[2], data[3]);
#endif
	}
	static inline void getCpuidEx(unsigned int eaxIn, unsigned int ecxIn, unsigned int data[4])
	{
#ifdef _MSC_VER
		__cpuidex(reinterpret_cast<int*>(data), eaxIn, ecxIn);
#else
		__cpuid_count(eaxIn, ecxIn, data[0], data[1], data[2], data[3]);
#endif
	}
	static inline uint64_t getXfeature()
	{
#ifdef _MSC_VER
		return __xgetbv(0);
#else
		unsigned int eax, edx;
		// xgetvb is not support on gcc 4.2
		__asm__ volatile(".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c"(0));
		return ((uint64_t)edx << 32) | eax;
#endif
	}
#ifdef XBYAK64
	typedef uint64_t Type;
#else
	typedef uint32_t Type;
#endif
	static const Type NONE = 0;
	static const Type tMMX = 1 << 0;
	static const Type tMMX2 = 1 << 1;
	static const Type tCMOV = 1 << 2;
	static const Type tSSE = 1 << 3;
	static const Type tSSE2 = 1 << 4;
	static const Type tSSE3 = 1 << 5;
	static const Type tSSSE3 = 1 << 6;
	static const Type tSSE41 = 1 << 7;
	static const Type tSSE42 = 1 << 8;
	static const Type tPOPCNT = 1 << 9;
	static const Type tAESNI = 1 << 10;
	static const Type tSSE5 = 1 << 11;
	static const Type tOSXSAVE = 1 << 12;
	static const Type tPCLMULQDQ = 1 << 13;
	static const Type tAVX = 1 << 14;
	static const Type tFMA = 1 << 15;

	static const Type t3DN = 1 << 16;
	static const Type tE3DN = 1 << 17;
	static const Type tSSE4a = 1 << 18;
	static const Type tRDTSCP = 1 << 19;
	static const Type tAVX2 = 1 << 20;
	static const Type tBMI1 = 1 << 21; // andn, bextr, blsi, blsmsk, blsr, tzcnt
	static const Type tBMI2 = 1 << 22; // bzhi, mulx, pdep, pext, rorx, sarx, shlx, shrx
	static const Type tLZCNT = 1 << 23;

	static const Type tINTEL = 1 << 24;
	static const Type tAMD = 1 << 25;

	static const Type tENHANCED_REP = 1 << 26; // enhanced rep movsb/stosb
	static const Type tRDRAND = 1 << 27;
	static const Type tADX = 1 << 28; // adcx, adox
	static const Type tRDSEED = 1 << 29; // rdseed
	static const Type tSMAP = 1 << 30; // stac
#ifdef XBYAK64
	static const Type tHLE = uint64_t(1) << 31; // xacquire, xrelease, xtest
	static const Type tRTM = uint64_t(1) << 32; // xbegin, xend, xabort
	static const Type tF16C = uint64_t(1) << 33; // vcvtph2ps, vcvtps2ph
	static const Type tMOVBE = uint64_t(1) << 34; // mobve
#endif

	Cpu()
		: type_(NONE)
	{
		unsigned int data[4];
		getCpuid(0, data);
		const unsigned int maxNum = data[0];
		static const char intel[] = "ntel";
		static const char amd[] = "cAMD";
		if (data[2] == get32bitAsBE(amd)) {
			type_ |= tAMD;
			getCpuid(0x80000001, data);
			if (data[3] & (1U << 31)) type_ |= t3DN;
			if (data[3] & (1U << 15)) type_ |= tCMOV;
			if (data[3] & (1U << 30)) type_ |= tE3DN;
			if (data[3] & (1U << 22)) type_ |= tMMX2;
			if (data[3] & (1U << 27)) type_ |= tRDTSCP;
		}
		if (data[2] == get32bitAsBE(intel)) {
			type_ |= tINTEL;
			getCpuid(0x80000001, data);
			if (data[3] & (1U << 27)) type_ |= tRDTSCP;
			if (data[2] & (1U << 5)) type_ |= tLZCNT;
		}
		getCpuid(1, data);
		if (data[2] & (1U << 0)) type_ |= tSSE3;
		if (data[2] & (1U << 9)) type_ |= tSSSE3;
		if (data[2] & (1U << 19)) type_ |= tSSE41;
		if (data[2] & (1U << 20)) type_ |= tSSE42;
#ifdef XBYAK64
		if (data[2] & (1U << 22)) type_ |= tMOVBE;
		if (data[2] & (1U << 29)) type_ |= tF16C;
#endif
		if (data[2] & (1U << 23)) type_ |= tPOPCNT;
		if (data[2] & (1U << 25)) type_ |= tAESNI;
		if (data[2] & (1U << 1)) type_ |= tPCLMULQDQ;
		if (data[2] & (1U << 27)) type_ |= tOSXSAVE;
		if (data[2] & (1U << 30)) type_ |= tRDRAND;

		if (data[3] & (1U << 15)) type_ |= tCMOV;
		if (data[3] & (1U << 23)) type_ |= tMMX;
		if (data[3] & (1U << 25)) type_ |= tMMX2 | tSSE;
		if (data[3] & (1U << 26)) type_ |= tSSE2;

		if (type_ & tOSXSAVE) {
			// check XFEATURE_ENABLED_MASK[2:1] = '11b'
			uint64_t bv = getXfeature();
			if ((bv & 6) == 6) {
				if (data[2] & (1U << 28)) type_ |= tAVX;
				if (data[2] & (1U << 12)) type_ |= tFMA;
			}
		}
		if (maxNum >= 7) {
			getCpuidEx(7, 0, data);
			if (type_ & tAVX && data[1] & 0x20) type_ |= tAVX2;
			if (data[1] & (1U << 3)) type_ |= tBMI1;
			if (data[1] & (1U << 8)) type_ |= tBMI2;
			if (data[1] & (1U << 9)) type_ |= tENHANCED_REP;
			if (data[1] & (1U << 18)) type_ |= tRDSEED;
			if (data[1] & (1U << 19)) type_ |= tADX;
			if (data[1] & (1U << 20)) type_ |= tSMAP;
#ifdef XBYAK64
			if (data[1] & (1U << 4)) type_ |= tHLE;
			if (data[1] & (1U << 11)) type_ |= tRTM;
#endif
		}
		setFamily();
	}
	bool has(Type type) const
	{
		return (type & type_) != 0;
	}
};

#ifdef XBYAK64
const int UseRCX = 1 << 6;
const int UseRDX = 1 << 7;

class Pack {
	static const size_t maxTblNum = 10;
	const Xbyak::Reg64 *tbl_[maxTblNum];
	size_t n_;
public:
	Pack() : tbl_(), n_(0) {}
	Pack(const Xbyak::Reg64 *tbl, size_t n) { init(tbl, n); }
	Pack(const Pack& rhs)
		: n_(rhs.n_)
	{
		if (n_ > maxTblNum) throw Error(ERR_INTERNAL);
		for (size_t i = 0; i < n_; i++) tbl_[i] = rhs.tbl_[i];
	}
	Pack(const Xbyak::Reg64& t0)
	{ n_ = 1; tbl_[0] = &t0; }
	Pack(const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 2; tbl_[0] = &t0; tbl_[1] = &t1; }
	Pack(const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 3; tbl_[0] = &t0; tbl_[1] = &t1; tbl_[2] = &t2; }
	Pack(const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 4; tbl_[0] = &t0; tbl_[1] = &t1; tbl_[2] = &t2; tbl_[3] = &t3; }
	Pack(const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 5; tbl_[0] = &t0; tbl_[1] = &t1; tbl_[2] = &t2; tbl_[3] = &t3; tbl_[4] = &t4; }
	Pack(const Xbyak::Reg64& t5, const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 6; tbl_[0] = &t0; tbl_[1] = &t1; tbl_[2] = &t2; tbl_[3] = &t3; tbl_[4] = &t4; tbl_[5] = &t5; }
	Pack(const Xbyak::Reg64& t6, const Xbyak::Reg64& t5, const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 7; tbl_[0] = &t0; tbl_[1] = &t1; tbl_[2] = &t2; tbl_[3] = &t3; tbl_[4] = &t4; tbl_[5] = &t5; tbl_[6] = &t6; }
	Pack(const Xbyak::Reg64& t7, const Xbyak::Reg64& t6, const Xbyak::Reg64& t5, const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 8; tbl_[0] = &t0; tbl_[1] = &t1; tbl_[2] = &t2; tbl_[3] = &t3; tbl_[4] = &t4; tbl_[5] = &t5; tbl_[6] = &t6; tbl_[7] = &t7; }
	Pack(const Xbyak::Reg64& t8, const Xbyak::Reg64& t7, const Xbyak::Reg64& t6, const Xbyak::Reg64& t5, const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 9; tbl_[0] = &t0; tbl_[1] = &t1; tbl_[2] = &t2; tbl_[3] = &t3; tbl_[4] = &t4; tbl_[5] = &t5; tbl_[6] = &t6; tbl_[7] = &t7; tbl_[8] = &t8; }
	Pack(const Xbyak::Reg64& t9, const Xbyak::Reg64& t8, const Xbyak::Reg64& t7, const Xbyak::Reg64& t6, const Xbyak::Reg64& t5, const Xbyak::Reg64& t4, const Xbyak::Reg64& t3, const Xbyak::Reg64& t2, const Xbyak::Reg64& t1, const Xbyak::Reg64& t0)
	{ n_ = 10; tbl_[0] = &t0; tbl_[1] = &t1; tbl_[2] = &t2; tbl_[3] = &t3; tbl_[4] = &t4; tbl_[5] = &t5; tbl_[6] = &t6; tbl_[7] = &t7; tbl_[8] = &t8; tbl_[9] = &t9; }
	Pack& append(const Xbyak::Reg64& t)
	{
		if (n_ == 10)
			throw Error(ERR_BAD_PARAMETER);
		tbl_[n_++] = &t;
		return *this;
	}
	void init(const Xbyak::Reg64 *tbl, size_t n)
	{
		if (n > maxTblNum)
			throw Error(ERR_BAD_PARAMETER);
		n_ = n;
		for (size_t i = 0; i < n; i++) {
			tbl_[i] = &tbl[i];
		}
	}
	const Xbyak::Reg64& operator[](size_t n) const
	{
		if (n >= n_)
			throw Error(ERR_BAD_PARAMETER);
		return *tbl_[n];
	}
	size_t size() const { return n_; }
	/*
		get tbl[pos, pos + num)
	*/
	Pack sub(size_t pos, size_t num = size_t(-1)) const
	{
		if (num == size_t(-1)) num = n_ - pos;
		if (pos + num > n_)
			throw Error(ERR_BAD_PARAMETER);
		Pack pack;
		pack.n_ = num;
		for (size_t i = 0; i < num; i++) {
			pack.tbl_[i] = tbl_[pos + i];
		}
		return pack;
	}
};

#endif

} } // end of util
#endif
