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

// Micro VU recompiler! - author: cottonvibes(@gmail.com)

#include "microVU.h"

#include <utility>

//------------------------------------------------------------------
// Micro VU - Private Functions
//------------------------------------------------------------------

// Caches Micro Program
__ri void mVUcacheProg(microVU& mVU, microProgram& prog)
{
	if (!mVU.index)
		memcpy(prog.data, mVU.regs().Micro, 0x1000);
	else
		memcpy(prog.data, mVU.regs().Micro, 0x4000);
}

// Creates a new Micro Program
static __ri microProgram* mVUcreateProg(microVU& mVU, int startPC) {
	microProgram* prog = (microProgram*)AlignedMalloc(sizeof(microProgram), 64);
	memset(prog, 0, sizeof(microProgram));
	prog->idx     = mVU.prog.total++;
	prog->ranges  = new std::deque<microRange>();
	prog->startPC = startPC;
	mVUcacheProg(mVU, *prog); // Cache Micro Program
	return prog;
}

// Deletes a program
static __ri void mVUdeleteProg(microVU& mVU, microProgram*& prog) {
	for (u32 i = 0; i < (mVU.progSize / 2); i++)
		safe_delete(prog->block[i]);
	safe_delete(prog->ranges);
	safe_aligned_free(prog);
}

// Compare Cached microProgram to mVU.regs().Micro
static __fi bool mVUcmpProg(microVU& mVU, microProgram& prog, const bool cmpWholeProg) {
	if (cmpWholeProg)
	{
		if (memcmp((u8*)prog.data, mVU.regs().Micro, mVU.microMemSize))
			return false;
	} 
	else
	{
		for (const auto& range : *prog.ranges)
		{
			auto cmpOffset = [&](void* x) { return (u8*)x + range.start; };
			if (memcmp(cmpOffset(prog.data), cmpOffset(mVU.regs().Micro), (range.end - range.start)))
				return false;
		}
	}
	mVU.prog.cleared = 0;
	mVU.prog.cur = &prog;
	mVU.prog.isSame = cmpWholeProg ? 1 : -1;
	return true;
}

// Searches for Cached Micro Program and sets prog.cur to it (returns entry-point to program)
_mVUt __fi void* mVUsearchProg(u32 startPC, uptr pState)
{
	microVU& mVU = mVUx;
	microProgramQuick& quick = mVU.prog.quick[mVU.regs().start_pc / 8];
	microProgramList* list = mVU.prog.prog[mVU.regs().start_pc / 8];

	if(!quick.prog) { // If null, we need to search for new program
		std::deque<microProgram*>::iterator it(list->begin());
		for ( ; it != list->end(); ++it) {
			bool b = mVUcmpProg(mVU, *it[0], 0);
			if (b) {
				quick.block = it[0]->block[startPC/8];
				quick.prog  = it[0];
				list->erase(it);
				list->push_front(quick.prog);
				// Sanity check, in case for some reason the program compilation aborted half way through (JALR for example)
				if (quick.block == nullptr)
				{
					void* entryPoint = mVUblockFetch(mVU, startPC, pState);
					return entryPoint;
				}
				return mVUentryGet(mVU, quick.block, startPC, pState);
			}
		}

		// If cleared and program not found, make a new program instance
		mVU.prog.cleared	= 0;
		mVU.prog.isSame		= 1;
		mVU.prog.cur		= mVUcreateProg(mVU, mVU.regs().start_pc / 8);
		void* entryPoint	= mVUblockFetch(mVU,  startPC, pState);
		quick.block			= mVU.prog.cur->block[startPC/8];
		quick.prog			= mVU.prog.cur;
		list->push_front(mVU.prog.cur);
		return entryPoint;
	}

	// If list.quick, then we've already found and recompiled the program ;)
	mVU.prog.isSame = -1;
	mVU.prog.cur = quick.prog;
	// Because the VU's can now run in sections and not whole programs at once
	// we need to set the current block so it gets the right program back
	quick.block = mVU.prog.cur->block[startPC / 8];

	// Sanity check, in case for some reason the program compilation aborted half way through
	if (quick.block == nullptr)
	{
		void* entryPoint = mVUblockFetch(mVU, startPC, pState);
		return entryPoint;
	}
	return mVUentryGet(mVU, quick.block, startPC, pState);
}


//------------------------------------------------------------------
// Micro VU - Main Functions
//------------------------------------------------------------------
static u8 __pagealigned vu0_RecDispatchers[mVUdispCacheSize];
static u8 __pagealigned vu1_RecDispatchers[mVUdispCacheSize];

static void mVUreserveCache(microVU& mVU)
{
	mVU.cache_reserve = new RecompiledCodeReserve(_16mb);
	
	mVU.cache = mVU.index ?
		(u8*)mVU.cache_reserve->Reserve(GetVmMemory().MainMemory(), HostMemoryMap::mVU1recOffset, mVU.cacheSize * _1mb):
		(u8*)mVU.cache_reserve->Reserve(GetVmMemory().MainMemory(), HostMemoryMap::mVU0recOffset, mVU.cacheSize * _1mb);
}

// Only run this once per VU! ;)
static void mVUinit(microVU& mVU, uint vuIndex)
{
	/* TODO/FIXME - implement different way of checking hardware
	   SSE2 requirements without throwing exception */
	memzero(mVU.prog);

	if(x86caps.hasStreamingSIMD4Extensions)
        {
		mVUclamp2  	= mVUclamp2_SSE4;
		mVUsaveReg 	= mVUsaveReg_SSE4;
	}

	mVU.index		=  vuIndex;
	mVU.cop2		=  0;
	mVU.vuMemSize		= (mVU.index ? 0x4000 : 0x1000);
	mVU.microMemSize	= (mVU.index ? 0x4000 : 0x1000);
	mVU.progSize		= (mVU.index ? 0x4000 : 0x1000) / 4;
	mVU.progMemMask		= mVU.progSize-1;
	mVU.cacheSize		= mVUcacheReserve;
	mVU.cache		= NULL;
	mVU.dispCache		= NULL;
	mVU.startFunct		= NULL;
	mVU.exitFunct		= NULL;

	mVUreserveCache(mVU);

	if (vuIndex) mVU.dispCache = vu1_RecDispatchers;
	else mVU.dispCache = vu0_RecDispatchers;

	mVU.regAlloc.reset(new microRegAlloc(mVU.index));
}

// Resets Rec Data
void mVUreset(microVU& mVU, bool resetReserve) {
	if (THREAD_VU1)
	{
		// If MTVU is toggled on during gameplay we need to flush the running VU1 program, else it gets in a mess
		if (VU0.VI[REG_VPU_STAT].UL & 0x100)
		{
			CpuVU1->Execute(vu1RunCycles);
		}
		VU0.VI[REG_VPU_STAT].UL &= ~0x100;
	}
	// Restore reserve to uncommitted state
	if (resetReserve) mVU.cache_reserve->Reset();

	HostSys::MemProtect(mVU.dispCache, mVUdispCacheSize, PageAccess_ReadWrite());
	memset(mVU.dispCache, 0xcc, mVUdispCacheSize);

	x86SetPtr(mVU.dispCache);
	mVUdispatcherAB(mVU);
	mVUdispatcherCD(mVU);
	mVUemitSearch();

	mVU.regs().nextBlockCycles = 0;
	memset(&mVU.prog.lpState, 0, sizeof(mVU.prog.lpState));

	// Program Variables
	mVU.prog.cleared	=  1;
	mVU.prog.isSame		= -1;
	mVU.prog.cur		= NULL;
	mVU.prog.total		=  0;
	mVU.prog.curFrame	=  0;

	// Setup Dynarec Cache Limits for Each Program
	u8* z = mVU.cache;
	mVU.prog.x86start	= z;
	mVU.prog.x86ptr		= z;
	mVU.prog.x86end		= z + ((mVU.cacheSize - mVUcacheSafeZone) * _1mb);

	for(u32 i = 0; i < (mVU.progSize / 2); i++) {
		if(!mVU.prog.prog[i]) {
			mVU.prog.prog[i] = new std::deque<microProgram*>();
			continue;
		}
		std::deque<microProgram*>::iterator it(mVU.prog.prog[i]->begin());
		for ( ; it != mVU.prog.prog[i]->end(); ++it) {
			mVUdeleteProg(mVU, it[0]);
		}
		mVU.prog.prog[i]->clear();
		mVU.prog.quick[i].block = NULL;
		mVU.prog.quick[i].prog  = NULL;
	}

	HostSys::MemProtect(mVU.dispCache, mVUdispCacheSize, PageAccess_ExecOnly());
}

// Free Allocated Resources
static void mVUclose(microVU& mVU)
{
	safe_delete  (mVU.cache_reserve);

	// Delete Programs and Block Managers
	for (u32 i = 0; i < (mVU.progSize / 2); i++) {
		if (!mVU.prog.prog[i]) continue;
		std::deque<microProgram*>::iterator it(mVU.prog.prog[i]->begin());
		for ( ; it != mVU.prog.prog[i]->end(); ++it) {
			mVUdeleteProg(mVU, it[0]);
		}
		safe_delete(mVU.prog.prog[i]);
	}
}

// Clears Block Data in specified range
static __fi void mVUclear(mV, u32 addr, u32 size)
{
	if(!mVU.prog.cleared) {
		mVU.prog.cleared = 1;		// Next execution searches/creates a new microprogram
		memzero(mVU.prog.lpState); // Clear pipeline state
		for(u32 i = 0; i < (mVU.progSize / 2); i++) {
			mVU.prog.quick[i].block = NULL; // Clear current quick-reference block
			mVU.prog.quick[i].prog  = NULL; // Clear current quick-reference prog
		}
	}
}

//------------------------------------------------------------------
// recMicroVU0 / recMicroVU1
//------------------------------------------------------------------
recMicroVU0::recMicroVU0()		  { m_Idx = 0; }
recMicroVU1::recMicroVU1()		  { m_Idx = 1; }

void recMicroVU0::Reserve() {
	if (m_Reserved.exchange(1) == 0)
		mVUinit(microVU0, 0);
}
void recMicroVU1::Reserve() {
	if (m_Reserved.exchange(1) == 0) {
		mVUinit(microVU1, 1);
		vu1Thread.Start();
	}
}

void recMicroVU0::Shutdown() noexcept {
	if (m_Reserved.exchange(0) == 1)
		mVUclose(microVU0);
}
void recMicroVU1::Shutdown() noexcept {
	if (m_Reserved.exchange(0) == 1) {
		vu1Thread.WaitVU();
		mVUclose(microVU1);
	}
}

void recMicroVU0::Reset() {
	if(!m_Reserved) return;
	mVUreset(microVU0, true);
}
void recMicroVU1::Reset() {
	if(!m_Reserved) return;
	vu1Thread.WaitVU();
	mVUreset(microVU1, true);
}

void recMicroVU0::SetStartPC(u32 startPC)
{
	VU0.start_pc = startPC;
}

void recMicroVU0::Execute(u32 cycles) {
	VU0.flags &= ~VUFLAG_MFLAGSET;

	if(!(VU0.VI[REG_VPU_STAT].UL & 1)) return;
	VU0.VI[REG_TPC].UL <<= 3;

	((mVUrecCall)microVU0.startFunct)(VU0.VI[REG_TPC].UL, cycles);
	VU0.VI[REG_TPC].UL >>= 3;
	if(microVU0.regs().flags & 0x4)
	{
		microVU0.regs().flags &= ~0x4;
		hwIntcIrq(6);
	}
}

void recMicroVU1::SetStartPC(u32 startPC)
{
	VU1.start_pc = startPC;
}

void recMicroVU1::Execute(u32 cycles) {
	if (!THREAD_VU1) {
		if(!(VU0.VI[REG_VPU_STAT].UL & 0x100)) return;
	}
	VU1.VI[REG_TPC].UL <<= 3;
	((mVUrecCall)microVU1.startFunct)(VU1.VI[REG_TPC].UL, cycles);
	VU1.VI[REG_TPC].UL >>= 3;
	if(microVU1.regs().flags & 0x4)
	{
		microVU1.regs().flags &= ~0x4;
		hwIntcIrq(7);
	}
}

void recMicroVU0::Clear(u32 addr, u32 size) {
	mVUclear(microVU0, addr, size);
}
void recMicroVU1::Clear(u32 addr, u32 size) {
	mVUclear(microVU1, addr, size);
}

uint recMicroVU0::GetCacheReserve() const {
	return microVU0.cacheSize;
}
uint recMicroVU1::GetCacheReserve() const {
	return microVU1.cacheSize;
}

void recMicroVU0::SetCacheReserve(uint reserveInMegs) const {
	microVU0.cacheSize = std::min(reserveInMegs, mVUcacheReserve);
	safe_delete(microVU0.cache_reserve); // I assume this unmaps the memory
	mVUreserveCache(microVU0); // Need rec-reset after this
}
void recMicroVU1::SetCacheReserve(uint reserveInMegs) const {
	microVU1.cacheSize = std::min(reserveInMegs, mVUcacheReserve);
	safe_delete(microVU1.cache_reserve); // I assume this unmaps the memory
	mVUreserveCache(microVU1); // Need rec-reset after this
}

void recMicroVU1::ResumeXGkick() {
	if(!(VU0.VI[REG_VPU_STAT].UL & 0x100)) return;
	((mVUrecCallXG)microVU1.startFunctXG)();
}
