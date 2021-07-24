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

#include "../Memory.h"


extern char* disVU0MicroUF(u32 code, u32 pc);
extern char* disVU0MicroLF(u32 code, u32 pc);
extern char* disVU1MicroUF(u32 code, u32 pc);
extern char* disVU1MicroLF(u32 code, u32 pc);

namespace R5900
{
	void disR5900Fasm( std::string& output, u32 code, u32 pc, bool simplify = false);

	extern const char * const GPR_REG[32];
	extern const char * const COP0_REG[32];
	extern const char * const COP1_REG_FP[32];
	extern const char * const COP1_REG_FCR[32];
	extern const char * const COP2_REG_FP[32];
	extern const char * const COP2_REG_CTL[32];
	extern const char * const COP2_VFnames[4];
}

namespace R3000A
{
	extern void (*IOP_DEBUG_BSC[64])(char *buf);

	extern const char * const disRNameGPR[];
	extern char* disR3000AF(u32 code, u32 pc);
}

// Helper macro for cut&paste.  Note that we intentionally use a top-level *inline* bitcheck
// against Trace.Enabled, to avoid extra overhead in Debug builds when logging is disabled.
// (specifically this allows debug builds to skip havingto resolve all the parameters being
//  passed into the function)
#define SysTraceActive(trace)	(false)

#ifdef __WXMAC__
    // Not available on OSX, apparently always double buffered window.
#   define                          SetDoubleBuffered(x)

    // TODO OSX OsxKeyCodes.cpp pending
#endif

#define macTrace(trace)

#define SIF_LOG			macTrace(SIF)

#define BIOS_LOG		macTrace(EE.Bios)
#define CPU_LOG			macTrace(EE.R5900)
#define COP0_LOG		macTrace(EE.COP0)
#define VUM_LOG			macTrace(EE.COP2)
#define MEM_LOG			macTrace(EE.Memory)
#define CACHE_LOG		macTrace(EE.Cache)
#define HW_LOG			macTrace(EE.KnownHw)
#define UnknownHW_LOG	macTrace(EE.UnknownHw)
#define DMA_LOG			macTrace(EE.DMAhw)
#define IPU_LOG			macTrace(EE.IPU)
#define VIF_LOG			macTrace(EE.VIF)
#define SPR_LOG			macTrace(EE.SPR)
#define GIF_LOG			macTrace(EE.GIF)
#define MSKPATH3_LOG	macTrace(EE.MSKPATH3)
#define EECNT_LOG		macTrace(EE.Counters)
#define VifCodeLog		macTrace(EE.VIFcode)
#define GifTagLog		macTrace(EE.GIFtag)


#define PSXBIOS_LOG		macTrace(IOP.Bios)
#define PSXCPU_LOG		macTrace(IOP.R3000A)
#define PSXMEM_LOG		macTrace(IOP.Memory)
#define PSXHW_LOG		macTrace(IOP.KnownHw)
#define PSXUnkHW_LOG	macTrace(IOP.UnknownHw)
#define PSXDMA_LOG		macTrace(IOP.DMAhw)
#define PSXCNT_LOG		macTrace(IOP.Counters)
#define MEMCARDS_LOG	macTrace(IOP.Memcards)
#define PAD_LOG			macTrace(IOP.PAD)
#define GPU_LOG			macTrace(IOP.GPU)
#define CDVD_LOG		macTrace(IOP.CDVD)
#define MDEC_LOG		macTrace(IOP.MDEC)


#define ELF_LOG
#define eeRecPerfLog		false	
#define eeConLog		false
#define eeDeci2Log		false
#define iopConLog		false
#define sysConLog		false
