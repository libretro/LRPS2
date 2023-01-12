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

_sif sif0;

static __fi void Sif0Init(void)
{
	sif0.ee.cycles = 0;
	sif0.iop.cycles = 0;
}

// Write from Fifo to EE.
static __fi void WriteFifoToEE(void)
{
	const int readSize = std::min((s32)sif0ch.qwc, sif0.fifo.size >> 2);

	tDMA_TAG *ptag = sif0ch.getAddr(sif0ch.madr, DMAC_SIF0, true);
	if (!ptag)
		return;

	sif0.fifo.read((u32*)ptag, readSize << 2);

	sif0ch.madr += readSize << 4;
	sif0.ee.cycles += readSize;	// FIXME : BIAS is factored in above
	sif0ch.qwc -= readSize;

	if (sif0ch.qwc == 0 && dmacRegs.ctrl.STS == STS_SIF0)
	{
		if ((sif0ch.chcr.MOD == NORMAL_MODE) || ((sif0ch.chcr.TAG >> 28) & 0x7) == TAG_CNTS)
			dmacRegs.stadr.ADDR = sif0ch.madr;
	}
}

// Write IOP to Fifo.
static __fi void WriteIOPtoFifo(void)
{
	// There's some data ready to transfer into the fifo..
	const int writeSize = std::min(sif0.iop.counter, sif0.fifo.sif_free());

	sif0.fifo.write((u32*)iopPhysMem(hw_dma9.madr), writeSize);
	hw_dma9.madr += writeSize << 2;

	// IOP is 1/8th the clock rate of the EE and psxcycles is in words (not quadwords).
	sif0.iop.cycles += writeSize; //1 word per cycle
	sif0.iop.counter -= writeSize;

	if (sif0.iop.counter == 0 && sif0.iop.writeJunk)
	{
		sif0.fifo.writeJunk(sif0.iop.writeJunk);
		sif0.iop.writeJunk = 0;
	}
}

// Read Fifo into an ee tag, transfer it to sif0ch, and process it.
static __fi void ProcessEETag(void)
{
	static __aligned16 u32 tag[4];
	tDMA_TAG& ptag(*(tDMA_TAG*)tag);

	sif0.fifo.read((u32*)&tag[0], 2); // Tag

	sif0ch.unsafeTransfer(&ptag);
	sif0ch.madr = tag[1];

	if (sif0ch.chcr.TIE && ptag.IRQ)
		sif0.ee.end = true;

	switch (ptag.ID)
	{
		case TAG_CNT:	break;

		case TAG_CNTS:
			if (dmacRegs.ctrl.STS == STS_SIF0) // STS == SIF0 - Initial Value
					dmacRegs.stadr.ADDR = sif0ch.madr;
			break;

		case TAG_END:
			sif0.ee.end = true;
			break;
	}
}

// Read FIFO into an iop tag, and transfer it to hw_dma9. And presumably process it.
static __fi void ProcessIOPTag(void)
{
	// Process DMA tag at hw_dma9.tadr
	sif0.iop.data = *(sifData *)iopPhysMem(hw_dma9.tadr);
	sif0.iop.data.words = sif0.iop.data.words;

	// send the EE's side of the DMAtag.  The tag is only 64 bits, with the upper 64 bits
	// ignored by the EE.

	sif0.fifo.write((u32*)iopPhysMem(hw_dma9.tadr + 8), 2);

	hw_dma9.tadr += 16; ///hw_dma9.madr + 16 + sif0.sifData.words << 2;

	// We're only copying the first 24 bits.  Bits 30 and 31 (checked below) are Stop/IRQ bits.
	hw_dma9.madr = sif0data & 0xFFFFFF;
	//Maximum transfer amount 1MB-16 also masking out top part which is a "Mode" cache stuff, we don't care :)
	sif0.iop.counter   = sif0words & 0xFFFFF;

	sif0.iop.writeJunk = (sif0.iop.counter & 0x3) ? (4 - sif0.iop.counter & 0x3) : 0;
	// IOP tags have an IRQ bit and an End of Transfer bit:
	if (sif0tag.IRQ  || (sif0tag.ID & 4)) sif0.iop.end = true;
}

// Stop transferring EE, and signal an interrupt.
static __fi void EndEE(void)
{
	sif0.ee.end  = false;
	sif0.ee.busy = false;
	if (sif0.ee.cycles == 0)
		sif0.ee.cycles = 1;

	CPU_INT(DMAC_SIF0, sif0.ee.cycles*BIAS);
}

// Stop transferring IOP, and signal an interrupt.
static __fi void EndIOP(void)
{
	sif0data      = 0;
	sif0.iop.end  = false;
	sif0.iop.busy = false;

	if (sif0.iop.cycles == 0)
		sif0.iop.cycles = 1;

	// Hack alert
	// Parappa The Rapper hates SIF0 taking the length of time it should do on bigger packets
	// I logged it and couldn't work out why, changing any other SIF timing (EE or IOP) seems to have no effect.
	if (sif0.iop.cycles > 1000)
		sif0.iop.cycles >>= 1; //2 word per cycles

	PSX_INT(IopEvt_SIF0, sif0.iop.cycles);
}

// Handle the EE transfer.
static __fi void HandleEETransfer(void)
{
	if(!sif0ch.chcr.STR)
	{
		sif0.ee.end = false;
		sif0.ee.busy = false;
		return;
	}

	if (sif0ch.qwc <= 0)
	{
		// Stop transferring ee, and signal an interrupt.
		if ((sif0ch.chcr.MOD == NORMAL_MODE) || sif0.ee.end)
			EndEE();
		else if (sif0.fifo.size >= 4) // Read a tag
		{
			// Read Fifo into an ee tag, transfer it to sif0ch
			// and process it.
			ProcessEETag();
		}
	}

	if (sif0ch.qwc > 0) // If we're writing something, continue to do so.
	{
		// Write from FIFO to EE.
		if (sif0.fifo.size > 0)
			WriteFifoToEE();
	}
}

// Handle the IOP transfer.
// Note: Test any changes in this function against Grandia III.
// What currently happens is this:
// SIF0 DMA start...
// SIF + 4 = 4 (pos=4)
// SIF0 IOP Tag: madr=19870, tadr=179cc, counter=8 (00000008_80019870)
// SIF - 4 = 0 (pos=4)
// SIF0 EE read tag: 90000002 935c0 0 0
// SIF0 EE dest chain tag madr:000935C0 qwc:0002 id:1 irq:1(000935C0_90000002)
// Write Fifo to EE: ----------- 0 of 8
// SIF - 0 = 0 (pos=4)
// Write IOP to Fifo: +++++++++++ 8 of 8
// SIF + 8 = 8 (pos=12)
// Write Fifo to EE: ----------- 8 of 8
// SIF - 8 = 0 (pos=12)
// Sif0: End IOP
// Sif0: End EE
// SIF0 DMA end...

// What happens if (sif0.iop.counter > 0) is handled first is this

// SIF0 DMA start...
// ...
// SIF + 8 = 8 (pos=12)
// Sif0: End IOP
// Write Fifo to EE: ----------- 8 of 8
// SIF - 8 = 0 (pos=12)
// SIF0 DMA end...

static __fi void HandleIOPTransfer(void)
{
	if (sif0.iop.counter <= 0) // If there's no more to transfer
	{
		// Stop transferring IOP, and signal an interrupt.
		if (sif0.iop.end)
			EndIOP();
		else
		{
			// Read Fifo into an IOP tag, and transfer it to hw_dma9.
			// And presumably process it.
			ProcessIOPTag();
		}
	}
	else
	{
		// Write IOP to Fifo.
		if (sif0.fifo.sif_free() > 0)
			WriteIOPtoFifo();
	}
}

static __fi void Sif0End(void)
{
	psHu32(SBUS_F240) &= ~0x20;
	psHu32(SBUS_F240) &= ~0x2000;
}

// Transfer IOP to EE, putting data in the fifo as an intermediate step.
__fi void SIF0Dma(void)
{
	int BusyCheck = 0;
	Sif0Init();

	do
	{
		//I realise this is very hacky in a way but its an easy way of checking if both are doing something
		BusyCheck = 0;

		if (sif0.iop.busy)
		{
			if(sif0.fifo.sif_free() > 0 || (sif0.iop.end && sif0.iop.counter == 0))
			{
				BusyCheck++;
				HandleIOPTransfer();
			}
		}
		if (sif0.ee.busy)
		{
			if(sif0.fifo.size >= 4 || (sif0.ee.end && sif0ch.qwc == 0))
			{
				BusyCheck++;
				HandleEETransfer();
			}
		}
	} while (BusyCheck > 0); // Substituting (sif0.ee.busy || sif0.iop.busy) breaks things.

	Sif0End();
}

__fi void  sif0Interrupt(void)
{
	HW_DMA9_CHCR &= ~0x01000000;
	psxDmaInterrupt2(2);
}

__fi void  EEsif0Interrupt(void)
{
	hwDmacIrq(DMAC_SIF0);
	sif0ch.chcr.STR = false;
}

__fi void dmaSIF0(void)
{
	psHu32(SBUS_F240) |= 0x2000;
	sif0.ee.busy = true;

	// Okay, this here is needed currently (r3644). 
	// FFX battles in the thunder plains map die otherwise, Phantasy Star 4 as well
	// These 2 games could be made playable again by increasing the time the EE or the IOP run,
	// showing that this is very timing sensible.
	// Doing this DMA unfortunately brings back an old warning in Legend of Legaia though, but it still works.

	//Updated 23/08/2011: The hangs are caused by the EE suspending SIF1 DMA and restarting it when in the middle 
	//of processing a "REFE" tag, so the hangs can be solved by forcing the ee.end to be false
	// (as it should always be at the beginning of a DMA).  using "if IOP is busy" flags breaks Tom Clancy Rainbow Six.
	// Legend of Legaia doesn't throw a warning either :)
	sif0.ee.end = false;
	SIF0Dma();
}
