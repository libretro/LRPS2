/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "Global.h"
#include "IopDma.h"

// Core 0 Input is "SPDIF mode" - Source audio is AC3 compressed.

// Core 1 Input is "CDDA mode" - Source audio data is 32 bits.
// PS2 note:  Very! few PS2 games use this mode.  Some PSX games used it, however no
// *known* PS2 game does since it was likely only available if the game was recorded to CD
// media (ie, not available in DVD mode, which almost all PS2 games use).  Plus PS2 games
// generally prefer to use ADPCM streaming audio since they need as much storage space as
// possible for FMVs and high-def textures.
//
StereoOut32 V_Core::ReadInput_HiFi()
{
	InputPosRead &= ~1;

	StereoOut32 retval(
		(s32&)(*GetMemPtr(0x2000 + (Index << 10) + InputPosRead)),
		(s32&)(*GetMemPtr(0x2200 + (Index << 10) + InputPosRead)));

	if (Index == 1)
	{
		// CDDA Mode:
		// give 30 bit data (SndOut downsamples the rest of the way)
		// HACKFIX: 28 bits seems better according to rama.  I should take some time and do some
		//    bitcounting on this one.  --air
		retval.Left >>= 4;
		retval.Right >>= 4;
	}

	InputPosRead += 2;

	// Why does CDDA mode check for InputPos == 0x100? In the old code, SPDIF mode did not but CDDA did.
	//  One of these seems wrong, they should be the same.  Since standard ADMA checks too I'm assuming that as default. -- air

	if ((InputPosRead == 0x100) || (InputPosRead >= 0x200))
	{
		AdmaInProgress = 0;
		if (InputDataLeft >= 0x200)
		{
			AutoDMAReadBuffer(0);
			AdmaInProgress = 1;

			TSA = (Index << 10) + InputPosRead;

			if (InputDataLeft < 0x200)
			{
				InputDataLeft = 0;
				// Hack, kinda. We call the interrupt early here, since PCSX2 doesn't like them delayed.
				//DMAICounter		= 1;
				if (Index == 0)
               spu2DMA4Irq();
				else
               spu2DMA7Irq();
			}
		}
		InputPosRead &= 0x1ff;
	}
	return retval;
}

StereoOut32 V_Core::ReadInput()
{
	StereoOut32 retval;

	if ((Index != 1) || ((PlayMode & 2) == 0))
	{
		for (int i = 0; i < 2; i++)
			if (Cores[i].IRQEnable && 0x2000 + (Index << 10) + InputPosRead == (Cores[i].IRQA & 0xfffffdff))
				SetIrqCall(i);

		retval = StereoOut32(
			(s32)(*GetMemPtr(0x2000 + (Index << 10) + InputPosRead)),
			(s32)(*GetMemPtr(0x2200 + (Index << 10) + InputPosRead)));
	}

	InputPosRead++;

	if (AutoDMACtrl & (Index + 1) && (InputPosRead == 0x100 || InputPosRead == 0x200))
	{
		AdmaInProgress = 0;
		if (InputDataLeft >= 0x200)
		{
			//u8 k=InputDataLeft>=InputDataProgress;

			AutoDMAReadBuffer(0);

			AdmaInProgress = 1;
			TSA = (Index << 10) + InputPosRead;

			if (InputDataLeft < 0x200)
			{
				AutoDMACtrl |= ~3;

				InputDataLeft = 0;
				// Hack, kinda. We call the interrupt early here, since PCSX2 doesn't like them delayed.
				//DMAICounter   = 1;
				if (Index == 0)
               spu2DMA4Irq();
				else
               spu2DMA7Irq();
			}
		}
	}
	InputPosRead &= 0x1ff;
	return retval;
}
