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

#include "VU.h"
#include "VUops.h"
#include "R5900.h"

#define VU0_MEMSIZE	0x1000		// 4kb
#define VU0_PROGSIZE	0x1000		// 4kb
#define VU1_MEMSIZE	0x4000		// 16kb
#define VU1_PROGSIZE	0x4000		// 16kb

static const uint VU0_MEMMASK	= VU0_MEMSIZE-1;
static const uint VU0_PROGMASK	= VU0_PROGSIZE-1;
static const uint VU1_MEMMASK	= VU1_MEMSIZE-1;
static const uint VU1_PROGMASK	= VU1_PROGSIZE-1;

#define vu1RunCycles (3000000) // mVU1 uses this for inf loop detection on dev builds

// --------------------------------------------------------------------------------------
//  BaseCpuProvider
// --------------------------------------------------------------------------------------
//
// Design Note: This class is only partial C++ style.  It still relies on Alloc and Shutdown
// calls for memory and resource management.  This is because the underlying implementations
// of our CPU emulators don't have properly encapsulated objects yet -- if we allocate ram
// in a constructor, it won't get free'd if an exception occurs during object construction.
// Once we've resolved all the 'dangling pointers' and stuff in the recompilers, Alloc
// and Shutdown can be removed in favor of constructor/destructor syntax.
//
class BaseCpuProvider
{
protected:
	// allocation counter for multiple calls to Reserve.  Most implementations should utilize
	// this variable for sake of robustness.
	std::atomic<int>		m_Reserved;

public:
	BaseCpuProvider()
	{
		m_Reserved = 0;
	}

	virtual ~BaseCpuProvider()
	{
	}

	virtual void Reserve()=0;
	virtual void Shutdown()=0;
	virtual void Reset()=0;
	virtual void SetStartPC(u32 startPC) = 0;
	virtual void Execute(u32 cycles)=0;
	virtual void ExecuteBlock(bool startUp)=0;

	virtual void Clear(u32 Addr, u32 Size)=0;

	// C++ Calling Conventions are unstable, and some compilers don't even allow us to take the
	// address of C++ methods.  We need to use a wrapper function to invoke the ExecuteBlock from
	// recompiled code.
	static void ExecuteBlockJIT( BaseCpuProvider* cpu )
	{
		cpu->Execute(1024);
	}

	// Gets the current cache reserve allocated to this CPU (value returned in megabytes)
	virtual uint GetCacheReserve() const=0;
	
	// Specifies the maximum cache reserve amount for this CPU (value in megabytes).
	// CPU providers are allowed to reset their reserves (recompiler resets, etc) if such is
	// needed to conform to the new amount requested.
	virtual void SetCacheReserve( uint reserveInMegs ) const=0;

};

// --------------------------------------------------------------------------------------
//  BaseVUmicroCPU
// --------------------------------------------------------------------------------------
// Layer class for possible future implementation (currently is nothing more than a type-safe
// type define).
//
class BaseVUmicroCPU : public BaseCpuProvider {
public:
	int m_Idx;

	BaseVUmicroCPU() {
		m_Idx		   = 0;
	}
	virtual ~BaseVUmicroCPU() = default;

	// Executes a Block based on EE delta time (see VUmicro.cpp)
	virtual void ExecuteBlock(bool startUp=0);

	static void ExecuteBlockJIT(BaseVUmicroCPU* cpu);

	// VU1 sometimes needs to break execution on XGkick Path1 transfers if
	// there is another gif path 2/3 transfer already taking place.
	// Use this method to resume execution of VU1.
	virtual void ResumeXGkick() {}
};


// --------------------------------------------------------------------------------------
//  InterpVU0 / InterpVU1
// --------------------------------------------------------------------------------------
class InterpVU0 : public BaseVUmicroCPU
{
public:
	InterpVU0();
	virtual ~InterpVU0() { Shutdown(); }

	void Reserve() { }
	void Shutdown() noexcept { }
	void Reset() { }

	void SetStartPC(u32 startPC);
	void Execute(u32 cycles);
	void Clear(u32 addr, u32 size) {}

	uint GetCacheReserve() const { return 0; }
	void SetCacheReserve( uint reserveInMegs ) const {}
};

class InterpVU1 : public BaseVUmicroCPU
{
public:
	InterpVU1();
	virtual ~InterpVU1() { Shutdown(); }

	void Reserve() { }
	void Shutdown() noexcept;
	void Reset();

	void SetStartPC(u32 startPC);
	void Execute(u32 cycles);
	void Clear(u32 addr, u32 size) {}
	void ResumeXGkick() {}

	uint GetCacheReserve() const { return 0; }
	void SetCacheReserve( uint reserveInMegs ) const {}
};

// --------------------------------------------------------------------------------------
//  recMicroVU0 / recMicroVU1
// --------------------------------------------------------------------------------------
class recMicroVU0 : public BaseVUmicroCPU
{
public:
	recMicroVU0();
	virtual ~recMicroVU0() { Shutdown(); }

	void Reserve();
	void Shutdown() noexcept;

	void Reset();
	void SetStartPC(u32 startPC);
	void Execute(u32 cycles);
	void Clear(u32 addr, u32 size);

	uint GetCacheReserve() const;
	void SetCacheReserve( uint reserveInMegs ) const;
};

class recMicroVU1 : public BaseVUmicroCPU
{
public:
	recMicroVU1();
	virtual ~recMicroVU1() { Shutdown(); }

	void Reserve();
	void Shutdown() noexcept;
	void Reset();
	void SetStartPC(u32 startPC);
	void Execute(u32 cycles);
	void Clear(u32 addr, u32 size);
	void ResumeXGkick();

	uint GetCacheReserve() const;
	void SetCacheReserve( uint reserveInMegs ) const;
};

extern BaseVUmicroCPU* CpuVU0;
extern BaseVUmicroCPU* CpuVU1;


// VU0
extern void vu0ResetRegs(void);
extern void vu0ExecMicro(u32 addr);
extern void _vu0FinishMicro(void);
extern void vu0Finish(void);

// VU1
extern void vu1Finish(bool add_cycles);
extern void vu1ResetRegs(void);
extern void vu1ExecMicro(u32 addr);
