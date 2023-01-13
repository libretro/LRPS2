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
#include "Gif.h"
#include "Sif.h"
#include "SPR.h"
#include "MTVU.h"

#include "IPU/IPUdma.h"
#include "ps2/HwInternal.h"

bool DMACh::transfer(const char *s, tDMA_TAG* ptag)
{
	if (!ptag)  					 // Is ptag empty?
	{
		dmacRegs.stat.BEIS = true;
		return false;
	}
	chcr.TAG = ptag[0]._u32 >> 16;
	qwc      = ptag[0].QWC;
	return true;
}

void DMACh::unsafeTransfer(tDMA_TAG* ptag)
{
    chcr.TAG = ptag[0]._u32 >> 16;
    qwc      = ptag[0].QWC;
}

tDMA_TAG *DMACh::getAddr(u32 addr, u32 num, bool write)
{
	tDMA_TAG *ptr = dmaGetAddr(addr, write);
	if (!ptr)
	{
		dmacRegs.stat.BEIS = true;
		dmacRegs.stat._u32 |= (1 << num);
		chcr.STR = false;
	}

	return ptr;
}

tDMA_TAG *DMACh::DMAtransfer(u32 addr, u32 num)
{
	tDMA_TAG *tag = getAddr(addr, num, false);

	if (tag)
	{
		chcr.TAG = tag[0]._u32 >> 16;
		qwc      = tag[0].QWC;
		return tag;
	}
	return NULL;
}

// Note: Dma addresses are guaranteed to be aligned to 16 bytes (128 bits)
__fi tDMA_TAG* SPRdmaGetAddr(u32 addr, bool write)
{
	//For some reason Getaway references SPR Memory from itself using SPR0, oh well, let it i guess...
	if((addr & 0x70000000) == 0x70000000)
		return (tDMA_TAG*)&eeMem->Scratch[addr & 0x3ff0];

	// FIXME: Why??? DMA uses physical addresses
	addr &= 0x1ffffff0;

	if (addr < Ps2MemSize::MainRam)
		return (tDMA_TAG*)&eeMem->Main[addr];
	else if (addr < 0x10000000)
		return (tDMA_TAG*)(write ? eeMem->ZeroWrite : eeMem->ZeroRead);
	else if ((addr >= 0x11000000) && (addr < 0x11010000))
	{
		if (addr >= 0x11008000 && THREAD_VU1)
			vu1Thread.WaitVU();
		
		//Access for VU Memory

		if((addr >= 0x1100c000) && (addr < 0x11010000))
			return (tDMA_TAG*)(VU1.Mem + (addr & 0x3ff0));

		if((addr >= 0x11004000) && (addr < 0x11008000))
			return (tDMA_TAG*)(VU0.Mem + (addr & 0xff0));
		
		//Possibly not needed but the manual doesn't say SPR cannot access it.
		if((addr >= 0x11000000) && (addr < 0x11004000))
			return (tDMA_TAG*)(VU0.Micro + (addr & 0xff0));

		if((addr >= 0x11008000) && (addr < 0x1100c000))
			return (tDMA_TAG*)(VU1.Micro + (addr & 0x3ff0));
	}
	// Unreachable
	return NULL;
}

// Note: Dma addresses are guaranteed to be aligned to 16 bytes (128 bits)
__ri tDMA_TAG *dmaGetAddr(u32 addr, bool write)
{
	if (DMA_TAG(addr).SPR) return (tDMA_TAG*)&eeMem->Scratch[addr & 0x3ff0];

	// FIXME: Why??? DMA uses physical addresses
	addr &= 0x1ffffff0;

	if (addr < Ps2MemSize::MainRam)
	{
		return (tDMA_TAG*)&eeMem->Main[addr];
	}
	else if (addr < 0x10000000)
	{
		return (tDMA_TAG*)(write ? eeMem->ZeroWrite : eeMem->ZeroRead);
	}
	else if (addr < 0x10004000)
	{
		// Secret scratchpad address for DMA = end of maximum main memory?
		return (tDMA_TAG*)&eeMem->Scratch[addr & 0x3ff0];
	}
	return NULL;
}


// Returns true if the DMA is enabled and executed successfully.  Returns false if execution
// was blocked (DMAE or master DMA enabler).
static bool QuickDmaExec( void (*func)(), u32 mem)
{
	bool ret = false;
	DMACh& reg = (DMACh&)psHu32(mem);

	if (reg.chcr.STR && dmacRegs.ctrl.DMAE && !psHu8(DMAC_ENABLER+2))
	{
		func();
		ret = true;
	}

	return ret;
}


static tDMAC_QUEUE QueuedDMA = {0};
static u32 oldvalue = 0;

static void StartQueuedDMA(void)
{
	if (QueuedDMA.VIF0) { QueuedDMA.VIF0 = !QuickDmaExec(dmaVIF0, D0_CHCR); }
	if (QueuedDMA.VIF1) { QueuedDMA.VIF1 = !QuickDmaExec(dmaVIF1, D1_CHCR); }
	if (QueuedDMA.GIF ) { QueuedDMA.GIF  = !QuickDmaExec(dmaGIF , D2_CHCR); }
	if (QueuedDMA.IPU0) { QueuedDMA.IPU0 = !QuickDmaExec(dmaIPU0, D3_CHCR); }
	if (QueuedDMA.IPU1) { QueuedDMA.IPU1 = !QuickDmaExec(dmaIPU1, D4_CHCR); }
	if (QueuedDMA.SIF0) { QueuedDMA.SIF0 = !QuickDmaExec(dmaSIF0, D5_CHCR); }
	if (QueuedDMA.SIF1) { QueuedDMA.SIF1 = !QuickDmaExec(dmaSIF1, D6_CHCR); }
	if (QueuedDMA.SIF2) { QueuedDMA.SIF2 = !QuickDmaExec(dmaSIF2, D7_CHCR); }
	if (QueuedDMA.SPR0) { QueuedDMA.SPR0 = !QuickDmaExec(dmaSPR0, D8_CHCR); }
	if (QueuedDMA.SPR1) { QueuedDMA.SPR1 = !QuickDmaExec(dmaSPR1, D9_CHCR); }
}

// Believe it or not, making this const can generate compiler warnings in gcc.
static __fi int ChannelNumber(u32 addr)
{
    switch (addr)
    {
        case D0_CHCR: return 0;
        case D1_CHCR: return 1;
        case D2_CHCR: return 2;
        case D3_CHCR: return 3;
        case D4_CHCR: return 4;
        case D5_CHCR: return 5;
        case D6_CHCR: return 6;
        case D7_CHCR: return 7;
        case D8_CHCR: return 8;
        case D9_CHCR: return 9;
		default:
		      break;
    }
    return 51; // some value
}

static __ri void DmaExec( void (*func)(), u32 mem, u32 value )
{
	DMACh& reg = (DMACh&)psHu32(mem);
	tDMA_CHCR chcr(value);

	//It's invalid for the hardware to write a DMA while it is active, not without Suspending the DMAC
	if (reg.chcr.STR)
	{
		const uint channel = ChannelNumber(mem);

		//As the manual states "Fields other than STR can only be written to when the DMA is stopped"
		//Also "The DMA may not stop properly just by writing 0 to STR"
		//So the presumption is that STR can be written to (ala force stop the DMA) but nothing else
		//If the developer wishes to alter any of the other fields, it must be done AFTER the STR has been written,
		//it will not work before or during this event.
		if(chcr.STR == 0)
		{
			reg.chcr.STR = 0;
			//We need to clear any existing DMA loops that are in progress else they will continue!

			if(channel == 1)
			{
				cpuClearInt( 10 );
				QueuedDMA._u16 &= ~(1 << 10); //Clear any queued DMA requests for this channel
			}
			else if(channel == 2)
			{
				cpuClearInt( 11 );
				QueuedDMA._u16 &= ~(1 << 11); //Clear any queued DMA requests for this channel
			}
				
			cpuClearInt( channel );
			QueuedDMA._u16 &= ~(1 << channel); //Clear any queued DMA requests for this channel
		}
		return;
	}

	reg.chcr.set(value);

	//Final Fantasy XII sets the DMA Mode to 3 which doesn't exist. On some channels (like SPR) this will break logic completely. so lets assume they mean chain.
	if (reg.chcr.MOD == 0x3)
	{
		static bool warned; //Check if the warning has already been output to console, to prevent constant spam.
		if (!warned)
			warned = true;
		reg.chcr.MOD = 0x1;
	}

	// As tested on hardware, if NORMAL mode is started with 0 QWC it will actually transfer 1 QWC then underflows and transfer another 0xFFFF QWC's
	// The easiest way to handle this is to just say 0x10000 QWC
	if (reg.chcr.STR && !reg.chcr.MOD && reg.qwc == 0)
		reg.qwc = 0x10000;

	if (reg.chcr.STR && dmacRegs.ctrl.DMAE && !psHu8(DMAC_ENABLER+2))
	{
		func();
	}
	else if(reg.chcr.STR)
	{
		QueuedDMA._u16 |= (1 << ChannelNumber(mem)); //Queue the DMA up to be started then the DMA's are Enabled and or the Suspend is lifted
	}
}

template< uint page >
__fi u32 dmacRead32( u32 mem )
{
	// Fixme: OPH hack. Toggle the flag on GIF_STAT access. (rama)
        if ((CHECK_OPHFLAGHACK) && (page << 12) == (mem & (0xf << 12)) && (mem == GIF_STAT))
	{
		static unsigned counter = 1;
		if (++counter == 8)
			counter = 2;
		// Set OPH and APATH from counter, cycling paths and alternating OPH
		return (gifRegs.stat._u32 & ~(7 << 9)) | ((counter & 1) ? (counter << 9) : 0);
	}
	
	return psHu32(mem);
}

// Returns true if the caller should do writeback of the register to eeHw; false if the
// register has no writeback, or if the writeback is handled internally.
template< uint page >
__fi bool dmacWrite32( u32 mem, mem32_t& value )
{
	switch(mem) {
		case(D0_QWC): // dma0 - vif0
		case(D1_QWC): // dma1 - vif1
		case(D2_QWC): // dma2 - gif
		case(D3_QWC): // dma3 - fromIPU
		case(D4_QWC): // dma4 - toIPU
		case(D5_QWC): // dma5 - sif0
		case(D6_QWC): // dma6 - sif1
		case(D7_QWC): // dma7 - sif2
		case(D8_QWC): // dma8 - fromSPR
		case(D9_QWC): // dma9 - toSPR
		{
			psHu32(mem) = (u16)value;
			return false;
		}

		case(D0_CHCR): // dma0 - vif0
		{
			/* VIF0dma EXECUTE */
			DmaExec(dmaVIF0, mem, value);
			return false;
		}

		case(D1_CHCR): // dma1 - vif1 - chcr
		{
			/* VIF1dma EXECUTE */
			DmaExec(dmaVIF1, mem, value);
			return false;
		}

		case(D2_CHCR): // dma2 - gif
		{
			/* GIFdma EXECUTE */
			DmaExec(dmaGIF, mem, value);
			return false;
		}


		case(D3_CHCR): // dma3 - fromIPU
		{
			/* IPU0dma EXECUTE */
			DmaExec(dmaIPU0, mem, value);
			return false;
		}

		case(D4_CHCR): // dma4 - toIPU
		{
			/* IPU1dma EXECUTE */
			DmaExec(dmaIPU1, mem, value);
			return false;
		}


		case(D5_CHCR): // dma5 - sif0
		{
			/* SIF0dma EXECUTE */
			DmaExec(dmaSIF0, mem, value);
			return false;
		}

		case(D6_CHCR): // dma6 - sif1
		{
			/* SIF1dma EXECUTE */
			DmaExec(dmaSIF1, mem, value);
			return false;
		}


		case(D7_CHCR): // dma7 - sif2
		{
			/* SIF2dma EXECUTE */
			DmaExec(dmaSIF2, mem, value);
			return false;
		}


		case(D8_CHCR): // dma8 - fromSPR
		{
			/* SPR0dma EXECUTE */
			DmaExec(dmaSPR0, mem, value);
			return false;
		}

		case(D9_CHCR): // dma9 - toSPR
		{
			/* SPR1dma EXECUTE (toSPR) */
			DmaExec(dmaSPR1, mem, value);
			return false;
		}


		case(fromSPR_MADR):
		case(toSPR_MADR):
		{
			// SPR bit is fixed at 0 for this channel
			psHu32(mem) = value & 0x7FFFFFFF;
			return false;
		}

		case(fromSPR_SADR):
		case(toSPR_SADR):
		{
			// Address must be QW aligned and fit in the 16K range of SPR
			psHu32(mem) = value & 0x3FF0;
			return false;
		}

		case(DMAC_CTRL):
		{
			u32 oldvalue = psHu32(mem);

			/* DMAC_CTRL Write 32bit */

			psHu32(mem) = value;
			//Check for DMAS that were started while the DMAC was disabled
			if (((oldvalue & 0x1) == 0) && ((value & 0x1) == 1))
			{
				if (!(QueuedDMA._u16 == 0)) StartQueuedDMA();
			}
			return false;
		}

		//Midway are a bunch of idiots, writing to E100 (reserved) instead of E010
		//Which causes a CPCOND0 to fail.
		case(DMAC_FAKESTAT):
		case(DMAC_STAT):
		{
			// lower 16 bits: clear on 1
			// upper 16 bits: reverse on 1

			psHu16(0xe010) &= ~(value & 0xffff);
			psHu16(0xe012) ^= (u16)(value >> 16);

			cpuTestDMACInts();
			return false;
		}

		case(DMAC_ENABLEW):
		{
			/* DMAC_ENABLEW Write 32bit */
			oldvalue = psHu8(DMAC_ENABLEW + 2);
			psHu32(DMAC_ENABLEW) = value;
			psHu32(DMAC_ENABLER) = value;
			if (((oldvalue & 0x1) == 1) && (((value >> 16) & 0x1) == 0))
			{
				if (!(QueuedDMA._u16 == 0)) StartQueuedDMA();
			}
			return false;
		}
	}

	// DMA Writes are invalid to everything except the STR on CHCR when it is busy
	// There's timing problems with many games. Gamefix time!
	if( CHECK_DMABUSYHACK && (mem & 0xf0) )
	{
		if((psHu32(mem & ~0xff) & 0x100) && dmacRegs.ctrl.DMAE && !psHu8(DMAC_ENABLER+2))
			return false;
	}

	// fall-through: use the default writeback provided by caller.
	return true;
}

template u32 dmacRead32<0x03>( u32 mem );

template bool dmacWrite32<0x00>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x01>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x02>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x03>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x04>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x05>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x06>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x07>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x08>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x09>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x0a>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x0b>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x0c>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x0d>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x0e>( u32 mem, mem32_t& value );
template bool dmacWrite32<0x0f>( u32 mem, mem32_t& value );
