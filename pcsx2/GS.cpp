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

#include "GS.h"
#include "Gif_Unit.h"
#include "Counters.h"

__aligned16 u8 g_RealGSMem[Ps2MemSize::GSregs];

void gsSetVideoMode(GS_VideoMode mode )
{
	gsVideoMode = mode;
	UpdateVSyncRate();
}

// Make sure framelimiter options are in sync with the plugin's capabilities.
void gsReset(void)
{
	GetMTGS().ResetGS();

	UpdateVSyncRate();
	memzero(g_RealGSMem);

	CSRreg.Reset();
	GSIMR.reset();
}

static __fi void gsCSRwrite( const tGS_CSR& csr )
{
	if (csr.RESET) {
		gifUnit.gsSIGNAL.queued = false;
		GetMTGS().SendSimplePacket(GS_RINGTYPE_RESET, 0, 0, 0);

		CSRreg.Reset();
		GSIMR.reset();
	}

	if(csr.SIGNAL)
	{
		// SIGNAL : What's not known here is whether or not the SIGID register should be updated
		//  here or when the IMR is cleared (below).
		if (gifUnit.gsSIGNAL.queued) {
			/* Firing pending signal */
			GSSIGLBLID.SIGID = (GSSIGLBLID.SIGID & ~gifUnit.gsSIGNAL.data[1])
				        | (gifUnit.gsSIGNAL.data[0]&gifUnit.gsSIGNAL.data[1]);

			if (!GSIMR.SIGMSK)
				hwIntcIrq(INTC_GS);
			CSRreg.SIGNAL  = true; // Just to be sure :p
		}
		else CSRreg.SIGNAL = false;
		gifUnit.gsSIGNAL.queued = false;
		gifUnit.Execute(false, true); // Resume paused transfers
	}
	
	if (csr.FINISH)	{
		CSRreg.FINISH = false; 
		gifUnit.gsFINISH.gsFINISHFired = false; //Clear the previously fired FINISH (YS, Indiecar 2005, MGS3)
	}
	if(csr.HSINT)	CSRreg.HSINT	= false;
	if(csr.VSINT)	CSRreg.VSINT	= false;
	if(csr.EDWINT)	CSRreg.EDWINT	= false;
}

__fi void gsWrite8(u32 mem, u8 value)
{
	switch (mem)
	{
		// CSR 8-bit write handlers.
		// I'm quite sure these would just write the CSR portion with the other
		// bits set to 0 (no action).  The previous implementation masked the 8-bit 
		// write value against the previous CSR write value, but that really doesn't
		// make any sense, given that the real hardware's CSR circuit probably has no
		// real "memory" where it saves anything.  (for example, you can't write to
		// and change the GS revision or ID portions -- they're all hard wired.) --air
	
		case GS_CSR: // GS_CSR
			gsCSRwrite( tGS_CSR((u32)value) );
			break;
		case GS_CSR + 1: // GS_CSR
			gsCSRwrite( tGS_CSR(((u32)value) <<  8) );
			break;
		case GS_CSR + 2: // GS_CSR
			gsCSRwrite( tGS_CSR(((u32)value) << 16) );
			break;
		case GS_CSR + 3: // GS_CSR
			gsCSRwrite( tGS_CSR(((u32)value) << 24) );
			break;
		default:
			*PS2GS_BASE(mem) = value;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
// GS Write 16 bit

__fi void gsWrite16(u32 mem, u16 value)
{
	switch (mem)
	{
		// See note above about CSR 8 bit writes, and handling them as zero'd bits
		// for all but the written parts.

		case GS_CSR:
			gsCSRwrite( tGS_CSR((u32)value) );
			return; // do not write to MTGS memory

		case GS_CSR+2:
			gsCSRwrite( tGS_CSR(((u32)value) << 16) );
			return; // do not write to MTGS memory

		case GS_IMR:
			/* Get interrupt mask */
			if ((CSRreg._u32 & 0x1f) & (~value & GSIMR._u32) >> 8)
				hwIntcIrq(INTC_GS);

			GSIMR._u32 = (value & 0x1f00)|0x6000;
			return; // do not write to MTGS memory
	}

	*(u16*)PS2GS_BASE(mem) = value;
}

//////////////////////////////////////////////////////////////////////////
// GS Write 32 bit

__fi void gsWrite32(u32 mem, u32 value)
{
	switch (mem)
	{
		case GS_CSR:
			gsCSRwrite(tGS_CSR(value));
			return;

		case GS_IMR:
			/* Get interrupt mask */
			if ((CSRreg._u32 & 0x1f) & (~value & GSIMR._u32) >> 8)
				hwIntcIrq(INTC_GS);

			GSIMR._u32 = (value & 0x1f00)|0x6000;
			return;
	}

	*(u32*)PS2GS_BASE(mem) = value;
}

//////////////////////////////////////////////////////////////////////////
// GS Write 64 bit

void gsWrite64_generic( u32 mem, const mem64_t* value )
{
	const u32* const srcval32 = (u32*)value;

	*(u64*)PS2GS_BASE(mem) = *value;
}

void gsWrite64_page_01( u32 mem, const mem64_t* value )
{
	switch( mem )
	{
		case GS_BUSDIR:

			gifUnit.stat.DIR = value[0] & 1;
			if (gifUnit.stat.DIR)
			{      // Assume will do local->host transfer
				gifUnit.stat.OPH = true; // Should we set OPH here?
				gifUnit.FlushToMTGS();   // Send any pending GS Primitives to the GS
			}

			gsWrite64_generic( mem, value );
			return;

		case GS_CSR:
			gsCSRwrite(tGS_CSR(*value));
			return;

		case GS_IMR:
			u32 _value = (u32)value[0];
			/* Get interrupt mask */
			if ((CSRreg._u32 & 0x1f) & (~_value & GSIMR._u32) >> 8)
				hwIntcIrq(INTC_GS);

			GSIMR._u32 = (_value & 0x1f00)|0x6000;
			return;
	}

	gsWrite64_generic( mem, value );
}

//////////////////////////////////////////////////////////////////////////
// GS Write 128 bit

void gsWrite128_page_01( u32 mem, const mem128_t* value )
{
	switch( mem )
	{
		case GS_CSR:
			gsCSRwrite((u32)value[0]);
			return;

		case GS_IMR:
			u32 _value = (u32)value[0];
			/* Get interrupt mask */
			if ((CSRreg._u32 & 0x1f) & (~_value & GSIMR._u32) >> 8)
				hwIntcIrq(INTC_GS);

			GSIMR._u32 = (_value & 0x1f00)|0x6000;
			return;
	}

	gsWrite128_generic( mem, value );
}

void gsWrite128_generic( u32 mem, const mem128_t* value )
{
	const u32* const srcval32 = (u32*)value;

	CopyQWC(PS2GS_BASE(mem), value);
}

__fi u8 gsRead8(u32 mem)
{
	return *(u8*)PS2GS_BASE(mem);
}

__fi u16 gsRead16(u32 mem)
{
	return *(u16*)PS2GS_BASE(mem);
}

__fi u32 gsRead32(u32 mem)
{
	return *(u32*)PS2GS_BASE(mem);
}

__fi u64 gsRead64(u32 mem)
{
	return *(u64*)PS2GS_BASE(mem);
}

#if 0
__fi u128 gsNonMirroredRead(u32 mem)
{
	return *(u128*)PS2GS_BASE(mem);
}
#endif

void SaveStateBase::gsFreeze(void)
{
	FreezeMem(PS2MEM_GS, 0x2000);
	Freeze(gsVideoMode);
}
