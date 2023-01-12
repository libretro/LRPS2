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
#include "IPU.h"
#include "IPU/IPUdma.h"
#include "mpeg2lib/Mpeg.h"

#define IPU_INT_TO( cycles )  if(!(cpuRegs.interrupt & (1<<4))) CPU_INT( DMAC_TO_IPU, cycles )

IPUStatus IPU1Status;

void ipuDmaReset(void)
{
	IPU1Status.InProgress	= false;
	IPU1Status.DMAFinished	= true;
}

void SaveStateBase::ipuDmaFreeze()
{
	FreezeTag( "IPUdma" );
	Freeze(IPU1Status);
}

static __fi int IPU1chain(void)
{
	int totalqwc = 0;
	int qwc      = ipu1ch.qwc;
	u32 *pMem    = (u32*)dmaGetAddr(ipu1ch.madr, false);

	if (pMem == NULL)
		return totalqwc;

	//Write our data to the fifo
	qwc = ipu_fifo.in.write(pMem, qwc);
	ipu1ch.madr += qwc << 4;
	ipu1ch.qwc -= qwc;
	totalqwc += qwc;

	//Update TADR etc
	hwDmacSrcTadrInc(ipu1ch);

	if (!ipu1ch.qwc)
		IPU1Status.InProgress = false;

	return totalqwc;
}

static void IPU1dma(void)
{
	int tagcycles = 0;
	int totalqwc = 0;

	//We MUST stop the IPU from trying to fill the FIFO with more data if the DMA has been suspended
	//if we don't, we risk causing the data to go out of sync with the fifo and we end up losing some!
	//This is true for Dragons Quest 8 and probably others which suspend the DMA.
	if(!ipu1ch.chcr.STR || ipu1ch.chcr.MOD == 2)
		return;

	if (IPU1Status.DataRequested == false)
	{
		// IPU isn't expecting any data, so put it in to wait mode.
		cpuRegs.eCycle[4] = 0x9999;
		return;
	}

	if (!IPU1Status.InProgress)
	{

		tDMA_TAG* ptag = dmaGetAddr(ipu1ch.tadr, false);  //Set memory pointer to TADR

		if (!ipu1ch.transfer("IPU1", ptag))
			return;
		ipu1ch.madr = ptag[1]._u32;

		tagcycles += 1; // Add 1 cycles from the QW read for the tag

		IPU1Status.DMAFinished = hwDmacSrcChain(ipu1ch, ptag->ID);

		if (ipu1ch.chcr.TIE && ptag->IRQ) //Tag Interrupt is set, so schedule the end/interrupt
			IPU1Status.DMAFinished = true;

		if (ipu1ch.qwc)
			IPU1Status.InProgress = true;
	}

	if (IPU1Status.InProgress)
		totalqwc += IPU1chain();

	//Do this here to prevent double settings on Chain DMA's
	if(totalqwc == 0 || (IPU1Status.DMAFinished && !IPU1Status.InProgress))
	{
		totalqwc = std::max(4, totalqwc) + tagcycles;
		IPU_INT_TO(totalqwc);
	}
	else
	{
		IPU1Status.DataRequested = false;

		if (!(IPU1Status.DMAFinished && !IPU1Status.InProgress))
			cpuRegs.eCycle[4] = 0x9999;//IPU_INT_TO(2048);
		else
		{
			totalqwc = std::max(4, totalqwc) + tagcycles;
			IPU_INT_TO(totalqwc*BIAS);
		}
	}

	IPUProcessInterrupt();
}

static void IPU0dma(void)
{
	if(!ipuRegs.ctrl.OFC) 
	{
		IPUProcessInterrupt();
		return;
	}

	int readsize;
	tDMA_TAG* pMem;

	if ((!(ipu0ch.chcr.STR) || (cpuRegs.interrupt & (1 << DMAC_FROM_IPU))) || (ipu0ch.qwc == 0))
		return;

	pMem = dmaGetAddr(ipu0ch.madr, true);

	readsize = std::min(ipu0ch.qwc, (u32)ipuRegs.ctrl.OFC);
	ipu_fifo.out.read(pMem, readsize);

	ipu0ch.madr += readsize << 4;
	ipu0ch.qwc -= readsize;
	
	if (dmacRegs.ctrl.STS == STS_fromIPU)   // STS == fromIPU
		dmacRegs.stadr.ADDR = ipu0ch.madr;

	IPU_INT_FROM( readsize * BIAS );
}

__fi void dmaIPU0() // fromIPU
{
	if (dmacRegs.ctrl.STS == STS_fromIPU)   // STS == fromIPU - Initial settings
		dmacRegs.stadr.ADDR = ipu0ch.madr;

	// Note: This should probably be a very small value, however anything lower than this will break Mana Khemia
	// This is because the game sends bad DMA information, starts an IDEC, then sets it to the correct values
	// but because our IPU is too quick, it messes up the sync between the DMA and IPU.
	// So this will do until (if) we sort the timing out of IPU, shouldn't cause any problems for games for now.
	//IPU_INT_FROM( 160 );
	// Update 22/12/2021 - Doesn't seem to need this now after fixing some FIFO/DMA behaviour
	IPU0dma();

	// Explanation of this:
	// The DMA logic on a NORMAL transfer is generally a "transfer first, ask questions later" so when it's sent
	// QWC == 0 (which we change to 0x10000) it transfers, causing an underflow, then asks if it's reached 0
	// since IPU_FROM is beholden to the OUT FIFO, if there's nothing to transfer, it will stay at 0 and won't underflow
	// so the DMA will end.
	if (ipu0ch.qwc == 0x10000)
	{
		ipu0ch.qwc = 0;
		ipu0ch.chcr.STR = false;
		hwDmacIrq(DMAC_FROM_IPU);
	}
}

__fi void dmaIPU1(void) // toIPU
{
	if (ipu1ch.chcr.MOD == CHAIN_MODE)  //Chain Mode
	{
		if(ipu1ch.qwc == 0)
		{
			IPU1Status.InProgress = false;
			IPU1Status.DMAFinished = false;
		}
		else // Attempting to continue a previous chain
		{
			IPU1Status.InProgress = true;
			if ((ipu1ch.chcr.tag().ID == TAG_REFE) || (ipu1ch.chcr.tag().ID == TAG_END) || (ipu1ch.chcr.tag().IRQ && ipu1ch.chcr.TIE))
				IPU1Status.DMAFinished = true;
			else
				IPU1Status.DMAFinished = false;
		}

		if(IPU1Status.DataRequested)
			IPU1dma();
		else
			cpuRegs.eCycle[4] = 0x9999;
	}
	else // Normal Mode
	{
			IPU1Status.InProgress = true;
			IPU1Status.DMAFinished = true;

			if (IPU1Status.DataRequested)
				IPU1dma();
			else
				cpuRegs.eCycle[4] = 0x9999;
	}
}

void ipu0Interrupt(void)
{
	if(ipu0ch.qwc > 0)
	{
		IPU0dma();
		return;
	}

	ipu0ch.chcr.STR = false;
	hwDmacIrq(DMAC_FROM_IPU);
}

__fi void ipu1Interrupt(void)
{
	if(!IPU1Status.DMAFinished || IPU1Status.InProgress)  //Sanity Check
	{
		IPU1dma();
		return;
	}

	ipu1ch.chcr.STR = false;
	hwDmacIrq(DMAC_TO_IPU);
}
