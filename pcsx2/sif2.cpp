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

#define _PC_	// disables MIPS opcode macros.

#include "R3000A.h"
#include "Common.h"
#include "Sif.h"
#include "IopHw.h"

_sif sif2;

static __fi void Sif2Init(void)
{
	sif2.ee.cycles = 0;
	sif2.iop.cycles = 0;
}

__fi bool ReadFifoSingleWord(void)
{
	u32 ptag[4];

	sif2.fifo.read((u32*)&ptag[0], 1);
	psHu32(0x1000f3e0) = ptag[0];
	if (sif2.fifo.size == 0) psxHu32(0x1000f300) |= 0x4000000;
	if (sif2.iop.busy && sif2.fifo.size <= 8)SIF2Dma();
	return true;
}

// Write from FIFO to EE.
static __fi void WriteFifoToEE(void)
{
	const int readSize = std::min((s32)sif2dma.qwc, sif2.fifo.size >> 2);
	tDMA_TAG *ptag     = sif2dma.getAddr(sif2dma.madr, DMAC_SIF2, true);
	if (!ptag)
		return;

	sif2.fifo.read((u32*)ptag, readSize << 2);

	sif2dma.madr += readSize << 4;
	sif2.ee.cycles += readSize;	// fixme : BIAS is factored in above
	sif2dma.qwc -= readSize;
}

// Write IOP to FIFO.
static __fi void WriteIOPtoFifo(void)
{
	// There's some data ready to transfer into the FIFO..
	const int writeSize = std::min(sif2.iop.counter, sif2.fifo.sif_free());

	sif2.fifo.write((u32*)iopPhysMem(hw_dma2.madr), writeSize);
	hw_dma2.madr += writeSize << 2;

	// IOP is 1/8th the clock rate of the EE and psxcycles is in words (not quadwords).
	sif2.iop.cycles += (writeSize >> 2)/* * BIAS*/;		// fixme : should be >> 4
	sif2.iop.counter -= writeSize;
	if (sif2.iop.counter == 0) hw_dma2.madr = sif2data & 0xffffff;
	if (sif2.fifo.size > 0) psxHu32(0x1000f300) &= ~0x4000000;
}

// Read Fifo into an ee tag, transfer it to sif2dma, and process it.
static __fi void ProcessEETag(void)
{
	static __aligned16 u32 tag[4];
	tDMA_TAG& ptag(*(tDMA_TAG*)tag);

	sif2.fifo.read((u32*)&tag[0], 4); // Tag

	sif2dma.unsafeTransfer(&ptag);
	sif2dma.madr = tag[1];

	if (sif2dma.chcr.TIE && ptag.IRQ)
		sif2.ee.end = true;

	switch (ptag.ID)
	{
		case TAG_CNT:
		case TAG_CNTS:
			break;

		case TAG_END:
			sif2.ee.end = true;
			break;
	}
}

// Read FIFO into an iop tag, and transfer it to hw_dma9. And presumably process it.
static __fi void ProcessIOPTag(void)
{
	// Process DMA tag at hw_dma9.tadr
	
	sif2.iop.data.words = sif2.iop.data.data >> 24; // Round up to nearest 4.

	// send the EE's side of the DMAtag.  The tag is only 64 bits, with the upper 64 bits
	// ignored by the EE.
	
	// We're only copying the first 24 bits.  Bits 30 and 31 (checked below) are Stop/IRQ bits.
	
	sif2.iop.counter =  (HW_DMA2_BCR_H16 * HW_DMA2_BCR_L16); //makes it do more stuff?? //sif2words;
	sif2.iop.end     = true;
}

// Stop transferring EE, and signal an interrupt.
static __fi void EndEE(void)
{
	sif2.ee.end = false;
	sif2.ee.busy = false;
	if (sif2.ee.cycles == 0)
		sif2.ee.cycles = 1;

	CPU_INT(DMAC_SIF2, sif2.ee.cycles*BIAS);
}

// Stop transferring IOP, and signal an interrupt.
static __fi void EndIOP(void)
{
	sif2data      = 0;
	sif2.iop.busy = false;

	if (sif2.iop.cycles == 0)
		sif2.iop.cycles = 1;
	// IOP is 1/8th the clock rate of the EE and psxcycles is in words (not quadwords)
	// So when we're all done, the equation looks like thus:
	PSX_INT(IopEvt_SIF2, sif2.iop.cycles);
}

// Handle the EE transfer.
static __fi void HandleEETransfer(void)
{
	if (!sif2dma.chcr.STR)
	{
		sif2.ee.end = false;
		sif2.ee.busy = false;
		return;
	}
	
	if (sif2dma.qwc <= 0)
	{
		// Stop transferring ee, and signal an interrupt.
		if ((sif2dma.chcr.MOD == NORMAL_MODE) || sif2.ee.end)
			EndEE();
		else if (sif2.fifo.size >= 4) // Read a tag
		{
			// Read Fifo into an ee tag, transfer it to sif2dma
			// and process it.
			//SIF2 EE Chain?!
			ProcessEETag();
		}
	}

	if (sif2dma.qwc > 0) // If we're writing something, continue to do so.
	{
		// Write from Fifo to EE.
		if (sif2.fifo.size > 0)
			WriteFifoToEE();
	}
}

// Handle the IOP transfer.
// Note: Test any changes in this function against Grandia III.
// What currently happens is this:
// SIF2 DMA start...
// SIF + 4 = 4 (pos=4)
// SIF2 IOP Tag: madr=19870, tadr=179cc, counter=8 (00000008_80019870)
// SIF - 4 = 0 (pos=4)
// SIF2 EE read tag: 90000002 935c0 0 0
// SIF2 EE dest chain tag madr:000935C0 qwc:0002 id:1 irq:1(000935C0_90000002)
// Write Fifo to EE: ----------- 0 of 8
// SIF - 0 = 0 (pos=4)
// Write IOP to Fifo: +++++++++++ 8 of 8
// SIF + 8 = 8 (pos=12)
// Write Fifo to EE: ----------- 8 of 8
// SIF - 8 = 0 (pos=12)
// Sif0: End IOP
// Sif0: End EE
// SIF2 DMA end...

// What happens if (sif2.iop.counter > 0) is handled first is this

// SIF2 DMA start...
// ...
// SIF + 8 = 8 (pos=12)
// Sif0: End IOP
// Write Fifo to EE: ----------- 8 of 8
// SIF - 8 = 0 (pos=12)
// SIF2 DMA end...

static __fi void HandleIOPTransfer(void)
{
	if (sif2.iop.counter <= 0) // If there's no more to transfer
	{
		// Stop transferring iop, and signal an interrupt.
		if (sif2.iop.end)
			EndIOP();
		else
		{
			// Read Fifo into an iop tag, and transfer it to hw_dma9.
			// And presumably process it.
			ProcessIOPTag();
		}
	}
	else
	{
		// Write IOP to Fifo.
		if (sif2.fifo.sif_free() > 0)
			WriteIOPtoFifo();
	}
}

static __fi void Sif2End(void)
{
	psHu32(SBUS_F240) &= ~0x80;
	psHu32(SBUS_F240) &= ~0x8000;
}

// Transfer IOP to EE, putting data in the fifo as an intermediate step.
__fi void SIF2Dma(void)
{
	int BusyCheck = 0;
	Sif2Init();

	do
	{
		//I realise this is very hacky in a way but its an easy way of checking if both are doing something
		BusyCheck = 0;

		if (sif2.iop.busy)
		{
			if (sif2.fifo.sif_free() > 0 || (sif2.iop.end && sif2.iop.counter == 0))
			{
				BusyCheck++;
				HandleIOPTransfer();
			}
		}
		if (sif2.ee.busy)
		{
			if (sif2.fifo.size >= 4 || (sif2.ee.end && sif2dma.qwc == 0))
			{
				BusyCheck++;
				HandleEETransfer();
			}
		}
	} while (BusyCheck > 0); // Substituting (sif2.ee.busy || sif2.iop.busy) breaks things.

	Sif2End();
}

__fi void  sif2Interrupt(void)
{
	if (!sif2.iop.end || sif2.iop.counter > 0)
	{
		SIF2Dma();
		return;
	}
	
	HW_DMA2_CHCR &= ~0x01000000;
	psxDmaInterrupt2(2);
}

__fi void  EEsif2Interrupt(void)
{
	hwDmacIrq(DMAC_SIF2);
	sif2dma.chcr.STR = false;
}

__fi void dmaSIF2(void)
{
	psHu32(SBUS_F240) |= 0x8000;
	sif2.ee.busy = true;

	// Okay, this here is needed currently (r3644). 
	// FFX battles in the thunder plains map die otherwise, Phantasy Star 4 as well
	// These 2 games could be made playable again by increasing the time the EE or the IOP run,
	// showing that this is very timing sensible.
	// Doing this DMA unfortunately brings back an old warning in Legend of Legaia though, but it still works.

	//Updated 23/08/2011: The hangs are caused by the EE suspending SIF1 DMA and restarting it when in the middle 
	//of processing a "REFE" tag, so the hangs can be solved by forcing the ee.end to be false
	// (as it should always be at the beginning of a DMA).  using "if iop is busy" flags breaks Tom Clancy Rainbow Six.
	// Legend of Legaia doesn't throw a warning either :)
	//sif2.ee.end = false;
	SIF2Dma();
}
