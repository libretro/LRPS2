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

#include "Common.h"
#include "R3000A.h"
#include "newVif.h"
#include "MTVU.h"
#include "x86emitter/x86_intrin.h"

#include "Elfheader.h"

#include "System/RecTypes.h"

#include "CDVD/CDVD.h"

SSE_MXCSR g_sseMXCSR	= { DEFAULT_sseMXCSR };
SSE_MXCSR g_sseVUMXCSR	= { DEFAULT_sseVUMXCSR };

// SetCPUState -- for assignment of SSE roundmodes and clampmodes.
void SetCPUState(SSE_MXCSR sseMXCSR, SSE_MXCSR sseVUMXCSR)
{
	g_sseMXCSR	= sseMXCSR.ApplyReserveMask();
	g_sseVUMXCSR	= sseVUMXCSR.ApplyReserveMask();

	_mm_setcsr( g_sseMXCSR.bitmask );
}


// --------------------------------------------------------------------------------------
//  RecompiledCodeReserve  (implementations)
// --------------------------------------------------------------------------------------

// Constructor!
// Parameters:
//   name - a nice long name that accurately describes the contents of this reserve.
RecompiledCodeReserve::RecompiledCodeReserve( uint defCommit )
	: VirtualMemoryReserve( defCommit )
{
	m_prot_mode		= PageAccess_Any();
}

RecompiledCodeReserve::~RecompiledCodeReserve() { }

void* RecompiledCodeReserve::Assign( VirtualMemoryManagerPtr allocator, void *baseptr, size_t size )
{
	if (!_parent::Assign(std::move(allocator), baseptr, size)) return NULL;

	Commit();

	return m_baseptr;
}

void RecompiledCodeReserve::Reset()
{
	_parent::Reset();

	Commit();
}

bool RecompiledCodeReserve::Commit()
{ 
   return _parent::Commit();
}

void SysOutOfMemory_EmergencyResponse(uptr blocksize)
{
	// An out of memory error occurred.  All we can try to do in response is reset the various
	// recompiler caches (which can sometimes total over 120megs, so it can be quite helpful).
	// If the user is using interpreters, or if the memory allocation failure was on a very small
	// allocation, then this code could fail; but that's fine.  We're already trying harder than
	// 99.995% of all programs ever written. -- air

	if (Cpu)
	{
		Cpu->SetCacheReserve( (Cpu->GetCacheReserve() * 2) / 3 );
		Cpu->Reset();
	}

	if (CpuVU0)
	{
		CpuVU0->SetCacheReserve( (CpuVU0->GetCacheReserve() * 2) / 3 );
		CpuVU0->Reset();
	}

	if (CpuVU1)
	{
		CpuVU1->SetCacheReserve( (CpuVU1->GetCacheReserve() * 2) / 3 );
		CpuVU1->Reset();
	}

	if (psxCpu)
	{
		psxCpu->SetCacheReserve( (psxCpu->GetCacheReserve() * 2) / 3 );
		psxCpu->Reset();
	}
}


Pcsx2Config EmuConfig;

template< typename CpuType >
class CpuInitializer
{
public:
	std::unique_ptr<CpuType> MyCpu;

	CpuInitializer();
	virtual ~CpuInitializer();
	operator CpuType*() { return MyCpu.get(); }
};

// --------------------------------------------------------------------------------------
//  CpuInitializer Template
// --------------------------------------------------------------------------------------
// Helper for initializing various PCSX2 CPU providers, and handing errors and cleanup.
//
template< typename CpuType >
CpuInitializer< CpuType >::CpuInitializer()
{
	try {
		MyCpu = std::make_unique<CpuType>();
		MyCpu->Reserve();
	}
	catch( std::runtime_error& ex )      { MyCpu = nullptr; }
}

template< typename CpuType >
CpuInitializer< CpuType >::~CpuInitializer()
{
	if (MyCpu)
		MyCpu->Shutdown();
}

// --------------------------------------------------------------------------------------
//  CpuInitializerSet
// --------------------------------------------------------------------------------------
class CpuInitializerSet
{
public:
	CpuInitializer<recMicroVU0>		microVU0;
	CpuInitializer<recMicroVU1>		microVU1;

	CpuInitializer<InterpVU0>		interpVU0;
	CpuInitializer<InterpVU1>		interpVU1;

public:
	CpuInitializerSet() {}
	virtual ~CpuInitializerSet() = default;
};

/// Attempts to find a spot near static variables for the main memory
static VirtualMemoryManagerPtr makeMainMemoryManager(void)
{
	// Everything looks nicer when the start of all the sections is a nice round looking number.
	// Also reduces the variation in the address due to small changes in code.
	// Breaks ASLR but so does anything else that tries to make addresses constant for our debugging pleasure
	uptr codeBase = (uptr)(void*)makeMainMemoryManager / (1 << 28) * (1 << 28);

	// The allocation is ~640mb in size, slighly under 3*2^28.
	// We'll hope that the code generated for the PCSX2 executable stays under 512mb (which is likely)
	// On x86-64, code can reach 8*2^28 from its address [-6*2^28, 4*2^28] is the region that allows for code in the 640mb allocation to reach 512mb of code that either starts at codeBase or 256mb before it.
	// We start high and count down because on macOS code starts at the beginning of useable address space, so starting as far ahead as possible reduces address variations due to code size.  Not sure about other platforms.  Obviously this only actually affects what shows up in a debugger and won't affect performance or correctness of anything.
	for (int offset = 4; offset >= -6; offset--) {
		uptr base = codeBase + (offset << 28);
		// VTLB will throw a fit if we try to put EE main memory here
		if ((sptr)base < 0 || (sptr)(base + HostMemoryMap::Size - 1) < 0)
			continue;
		auto mgr = std::make_shared<VirtualMemoryManager>(base, HostMemoryMap::Size, 0, true);
		if (mgr->GetBase() != 0)
			return mgr;
	}

	return std::make_shared<VirtualMemoryManager>(0, HostMemoryMap::Size);
}

// --------------------------------------------------------------------------------------
//  SysReserveVM  (implementations)
// --------------------------------------------------------------------------------------
SysMainMemory::SysMainMemory()
	: m_mainMemory(makeMainMemoryManager())
	, m_bumpAllocator(m_mainMemory, HostMemoryMap::bumpAllocatorOffset, HostMemoryMap::Size - HostMemoryMap::bumpAllocatorOffset) { }

SysMainMemory::~SysMainMemory()
{
	DecommitAll();

	// Just to be sure... (calling order could result 
	// in it getting missed during Decommit).
	vtlb_Core_Free();

	m_ee.Decommit();
	m_iop.Decommit();
	m_vu.Decommit();

	safe_delete(Source_PageFault);
}

void SysMainMemory::ReserveAll()
{
	pxInstallSignalHandler();
	m_ee.Reserve(MainMemory());
	m_iop.Reserve(MainMemory());
	m_vu.Reserve(MainMemory());
}

void SysMainMemory::CommitAll()
{
	vtlb_Core_Alloc();
	if (m_ee.IsCommitted() && m_iop.IsCommitted() && m_vu.IsCommitted()) return;
	m_ee.Commit();
	m_iop.Commit();
	m_vu.Commit();
}


void SysMainMemory::ResetAll()
{
	CommitAll();
	m_ee.Reset();
	m_iop.Reset();
	m_vu.Reset();

	// Note: newVif is reset as part of other VIF structures.
}

void SysMainMemory::DecommitAll()
{
	if (!m_ee.IsCommitted() && !m_iop.IsCommitted() && !m_vu.IsCommitted()) return;
	// On linux, the MTVU isn't empty and the thread still uses the m_ee/m_vu memory
	vu1Thread.WaitVU();
	// The EE thread must be stopped here command mustn't be send
	// to the ring. Let's call it an extra safety valve :)
	vu1Thread.Reset();

	hwShutdown();

	m_ee.Decommit();
	m_iop.Decommit();
	m_vu.Decommit();

	vtlb_Core_Free();
}

// --------------------------------------------------------------------------------------
//  SysCpuProviderPack  (implementations)
// --------------------------------------------------------------------------------------
SysCpuProviderPack::SysCpuProviderPack()
{
	CpuProviders = std::make_unique<CpuInitializerSet>();

	recCpu.Reserve();
	psxRec.Reserve();

	dVifReserve(0);
	dVifReserve(1);
}

SysCpuProviderPack::~SysCpuProviderPack()
{
	psxRec.Shutdown();
	recCpu.Shutdown();

	dVifRelease(0);
	dVifRelease(1);
}

BaseVUmicroCPU* CpuVU0 = NULL;
BaseVUmicroCPU* CpuVU1 = NULL;

void SysCpuProviderPack::ApplyConfig() const
{
	Cpu		= CHECK_EEREC	? &recCpu : &intCpu;
	psxCpu	= CHECK_IOPREC	? &psxRec : &psxInt;

	CpuVU0 = CpuProviders->interpVU0;
	CpuVU1 = CpuProviders->interpVU1;

	if( EmuConfig.Cpu.Recompiler.EnableVU0 )
		CpuVU0 = (BaseVUmicroCPU*)CpuProviders->microVU0;

	if( EmuConfig.Cpu.Recompiler.EnableVU1 )
		CpuVU1 = (BaseVUmicroCPU*)CpuProviders->microVU1;
}

// Resets all PS2 cpu execution caches, which does not affect that actual PS2 state/condition.
// This can be called at any time outside the context of a Cpu->Execute() block without
// bad things happening (recompilers will slow down for a brief moment since rec code blocks
// are dumped).
// Use this method to reset the recs when important global pointers like the MTGS are re-assigned.
void SysClearExecutionCache(void)
{
	GetCpuProviders().ApplyConfig();

	Cpu->Reset();
	psxCpu->Reset();

	// mVU's VU0 needs to be properly initialized for macro mode even if it's not used for micro mode!
	if (CHECK_EEREC)
		((BaseVUmicroCPU*)GetCpuProviders().CpuProviders->microVU0)->Reset();

	CpuVU0->Reset();
	CpuVU1->Reset();

	dVifReset(0);
	dVifReset(1);
}

wxString SysGetBiosDiscID(void)
{
	// FIXME: we should return a serial based on
	// the BIOS being run (either a checksum of the BIOS roms, and/or a string based on BIOS
	// region and revision).
	return wxEmptyString;
}

// This function always returns a valid DiscID -- using the Sony serial when possible, and
// falling back on the CRC checksum of the ELF binary if the PS2 software being run is
// homebrew or some other serial-less item.
wxString SysGetDiscID(void)
{
	if( !DiscSerial.IsEmpty() )
		return DiscSerial;
	// system is currently running the BIOS
	if( !ElfCRC )
		return SysGetBiosDiscID();
	return pxsFmt( L"%08x", ElfCRC );
}
