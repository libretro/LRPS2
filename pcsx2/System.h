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

#include <memory>

#include "Config.h"

#include "Utilities/SafeArray.h"
#include "Utilities/Threading.h" // to use threading stuff, include the Threading namespace in your file.

#include "vtlb.h"

class CpuInitializerSet;

typedef SafeArray<u8> VmStateBuffer;

class BaseVUmicroCPU;
class RecompiledCodeReserve;

// This is a table of default virtual map addresses for ps2vm components.  These locations
// are provided and used to assist in debugging and possibly hacking; as it makes it possible
// for a programmer to know exactly where to look (consistently!) for the base address of
// the various virtual machine components.  These addresses can be keyed directly into the
// debugger's disasm window to get disassembly of recompiled code, and they can be used to help
// identify recompiled code addresses in the callstack.

// All of these areas should be reserved as soon as possible during program startup, and its
// important that none of the areas overlap.  In all but superVU's case, failure due to overlap
// or other conflict will result in the operating system picking a preferred address for the mapping.

namespace HostMemoryMap
{
	static const u32 Size = 0x28000000;

	// The actual addresses may not be equivalent to Base + Offset in the event that allocation at Base failed
	// Each of these offsets has a debugger-accessible equivalent variable without the Offset suffix that will hold the actual address (not here because we don't want code using it)

	// PS2 main memory, SPR, and ROMs
	static const u32 EEmemOffset   = 0x00000000;

	// IOP main memory and ROMs
	static const u32 IOPmemOffset  = 0x04000000;

	// VU0 and VU1 memory.
	static const u32 VUmemOffset   = 0x08000000;

	// EE recompiler code cache area (64mb)
	static const u32 EErecOffset   = 0x10000000;

	// IOP recompiler code cache area (16 or 32mb)
	static const u32 IOPrecOffset  = 0x14000000;

	// newVif0 recompiler code cache area (16mb)
	static const u32 VIF0recOffset = 0x16000000;

	// newVif1 recompiler code cache area (32mb)
	static const u32 VIF1recOffset = 0x18000000;

	// microVU1 recompiler code cache area (32 or 64mb)
	static const u32 mVU0recOffset = 0x1C000000;

	// microVU0 recompiler code cache area (64mb)
	static const u32 mVU1recOffset = 0x20000000;

	// Bump allocator for any other small allocations
	// size: Difference between it and HostMemoryMap::Size, so nothing should allocate higher than it!
	static const u32 bumpAllocatorOffset = 0x24000000;
}

// --------------------------------------------------------------------------------------
//  SysMainMemory
// --------------------------------------------------------------------------------------
// This class provides the main memory for the virtual machines.
class SysMainMemory
{
protected:
	const VirtualMemoryManagerPtr m_mainMemory;
	VirtualMemoryBumpAllocator    m_bumpAllocator;
	eeMemoryReserve               m_ee;
	iopMemoryReserve              m_iop;
	vuMemoryReserve               m_vu;

public:
	SysMainMemory();
	virtual ~SysMainMemory();

	const VirtualMemoryManagerPtr& MainMemory()    { return m_mainMemory; }
	VirtualMemoryBumpAllocator&    BumpAllocator() { return m_bumpAllocator; }

	virtual void ReserveAll();
	virtual void CommitAll();
	virtual void ResetAll();
	virtual void DecommitAll();
};

// --------------------------------------------------------------------------------------
//  SysCpuProviderPack
// --------------------------------------------------------------------------------------
class SysCpuProviderPack
{
public:
	std::unique_ptr<CpuInitializerSet> CpuProviders;

	SysCpuProviderPack();
	virtual ~SysCpuProviderPack();

	void ApplyConfig() const;
};

// GetCpuProviders - this function is not implemented by PCSX2 core -- it must be
// implemented by the provisioning interface.
extern SysCpuProviderPack& GetCpuProviders();

extern void SysClearExecutionCache();	// clears recompiled execution caches!
extern void SysOutOfMemory_EmergencyResponse(uptr blocksize);

extern void NTFS_CompressFile( const wxString& file, bool compressStatus=true );

extern wxString SysGetBiosDiscID();
extern wxString SysGetDiscID();

extern SysMainMemory& GetVmMemory();

extern void SetCPUState(SSE_MXCSR sseMXCSR, SSE_MXCSR sseVUMXCSR);
extern SSE_MXCSR g_sseVUMXCSR, g_sseMXCSR;
