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

#include "IopHw_Internal.h"
#include "Sif.h"
#include "Sio.h"
#include "FW.h"
#include "R3000A.h"
#include "CDVD/CdRom.h"
#include "DEV9/DEV9.h"
#include "SPU2/spu2.h"
#include "USB/USB.h"

#include "ps2/pgif.h"
#include "Mdec.h"
#include "IopCounters.h"
#include "IopSio2.h"

namespace IopMemory
{
using namespace Internal;

//////////////////////////////////////////////////////////////////////////////////////////
//
mem8_t iopHwRead8_Page1( u32 addr )
{
	u32 masked_addr = addr & 0x0fff;

	mem8_t ret;		// using a return var can be helpful in debugging.
	switch( masked_addr )
	{
		mcase(HW_SIO_DATA) :
			// 1F801040h 1/4  JOY_DATA Joypad/Memory Card Data (R/W)
			// psxmode: documentation suggests a valid 8 bit read and the rest of the 32 bit register is unclear.
			// todo: check this and compare with the HW_SIO_DATA read around line 245 as well.
			ret = sioRead8();
		break;

		// for use of serial port ignore for now
		//case 0x50: ret = serial_read8(); break;

		mcase(HW_DEV9_DATA): ret = DEV9read8( addr ); break;

		mcase(HW_CDR_DATA0): ret = cdrRead0(); break;
		mcase(HW_CDR_DATA1): ret = cdrRead1(); break;
		mcase(HW_CDR_DATA2): ret = cdrRead2(); break;
		mcase(HW_CDR_DATA3): ret = cdrRead3(); break;

		default:
			if( masked_addr >= 0x100 && masked_addr < 0x130 )
				ret = psxHu8( addr );
			else if( masked_addr >= 0x480 && masked_addr < 0x4a0 )
				ret = psxHu8( addr );
			else if( (masked_addr >= pgmsk(HW_USB_START)) && (masked_addr < pgmsk(HW_USB_END)) )
				ret = USBread8( addr );
			else
				ret = psxHu8(addr);
		return ret;
	}

	return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
mem8_t iopHwRead8_Page3( u32 addr )
{
	if( addr == 0x1f803100 ) // PS/EE/IOP conf related
		return 0xFF; //all high bus is the corect default state for CEX PS2!
	return psxHu8( addr );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
mem8_t iopHwRead8_Page8( u32 addr )
{
	if( addr == HW_SIO2_FIFO )
		return sio2_fifoOut();//sio2 serial data feed/fifo_out
	return psxHu8( addr );
}
//////////////////////////////////////////////////////////////////////////////////////////
//
template< typename T >
static __fi T _HwRead_16or32_Page1( u32 addr )
{
	u32 masked_addr = pgmsk( addr );
	T ret = 0;

	// ------------------------------------------------------------------------
	// Counters, 16-bit varieties!
	//
	if( masked_addr >= 0x100 && masked_addr < 0x130 )
	{
		int cntidx = ( masked_addr >> 4 ) & 0xf;
		switch( masked_addr & 0xf )
		{
			case 0x0:
				ret = (T)psxRcntRcount16( cntidx );
			break;

			case 0x4:
				ret = psxCounters[cntidx].mode;

				// hmm!  The old code only did this bitwise math for 16 bit reads.
				// Logic indicates it should do the math consistently.  Question is,
				// should it do the logic for both 16 and 32, or not do logic at all?

				psxCounters[cntidx].mode &= ~0x1800;
			break;

			case 0x8:
				ret = psxCounters[cntidx].target;
			break;
			
			default:
				ret = psxHu32(addr);
			break;
		}
	}
	// ------------------------------------------------------------------------
	// Counters, 32-bit varieties!
	//
	else if( masked_addr >= 0x480 && masked_addr < 0x4b0 )
	{
		int cntidx = (( masked_addr >> 4 ) & 0xf) - 5;
		switch( masked_addr & 0xf )
		{
			case 0x0:
				ret = (T)psxRcntRcount32( cntidx );
			break;

			case 0x2:
				ret = (T)(psxRcntRcount32( cntidx ) >> 16);
			break;

			case 0x4:
				ret = psxCounters[cntidx].mode;

				// hmm!  The old code only did the following bitwise math for 16 bit reads.
				// Logic indicates it should do the math consistently.  Question is,
				// should it do the logic for both 16 and 32, or not do logic at all?

				psxCounters[cntidx].mode &= ~0x1800;
			break;

			case 0x8:
				ret = psxCounters[cntidx].target;
			break;

			case 0xa:
				ret = psxCounters[cntidx].target >> 16;
			break;

			default:
				ret = psxHu32(addr);
			break;
		}
	}
	// ------------------------------------------------------------------------
	// USB, with both 16 and 32 bit interfaces
	//
	else if( (masked_addr >= pgmsk(HW_USB_START)) && (masked_addr < pgmsk(HW_USB_END)) )
	{
		ret = (sizeof(T) == 2) ? USBread16( addr ) : USBread32( addr );
	}
	// ------------------------------------------------------------------------
	// SPU2, accessible in 16 bit mode only!
	//
	else if( masked_addr >= pgmsk(HW_SPU2_START) && masked_addr < pgmsk(HW_SPU2_END) )
	{
		if( sizeof(T) == 2 )
			ret = SPU2read( addr );
		else
		{
			ret = psxHu32(addr);
		}
	}
	// ------------------------------------------------------------------------
	// PS1 GPU access
	//
	else if( (masked_addr >= pgmsk(HW_PS1_GPU_START)) && (masked_addr < pgmsk(HW_PS1_GPU_END)) )
		ret = psxDma2GpuR(addr);
	else
	{
		switch( masked_addr )
		{
			// ------------------------------------------------------------------------
			mcase(HW_SIO_DATA):
				ret  = sioRead8();
				ret |= sioRead8() << 8;
				if( sizeof(T) == 4 )
				{
					ret |= sioRead8() << 16;
					ret |= sioRead8() << 24;
				}
			break;

			mcase(HW_SIO_STAT):
				ret = sio.StatReg;
				sioStatRead();
			break;

			mcase(HW_SIO_MODE):
				ret = sio.ModeReg;
				if( sizeof(T) == 4 )
				{
					// My guess on 32-bit accesses.  Dunno yet what the real hardware does. --air
					ret |= sio.CtrlReg << 16;
				}
			break;

			mcase(HW_SIO_CTRL):
				ret = sio.CtrlReg;
			break;

			mcase(HW_SIO_BAUD):
				ret = sio.BaudReg;
			break;

			// ------------------------------------------------------------------------
			//Serial port stuff not support now ;P
			// case 0x050: hard = serial_read32(); break;
			//	case 0x054: hard = serial_status_read(); break;
			//	case 0x05a: hard = serial_control_read(); break;
			//	case 0x05e: hard = serial_baud_read(); break;

			mcase(HW_ICTRL):
				ret = psxHu32(0x1078);
				psxHu32(0x1078) = 0;
			break;

			mcase(HW_ICTRL+2):
				ret = psxHu16(0x107a);
				psxHu32(0x1078) = 0;	// most likely should clear all 32 bits here.
			break;

			// ------------------------------------------------------------------------
			// Soon-to-be outdated SPU2 DMA hack (spu2 manages its own DMA MADR).
			//
			mcase(0x1f8010C0):
				ret = SPU2ReadMemAddr(0);
			break;

			mcase(0x1f801500):
				ret = SPU2ReadMemAddr(1);
			break;

			// ------------------------------------------------------------------------
			// Legacy GPU  emulation
			//
			mcase(0x1f8010ac) :
				ret = psxHu32(addr);
			break;

			mcase(HW_PS1_GPU_DATA) :
				ret = psxGPUr(addr);
			break;
			
			mcase(HW_PS1_GPU_STATUS) :
				ret = psxGPUr(addr);
			break;
			
			mcase (0x1f801820): // MDEC
				// ret = psxHu32(addr); // old
				ret = mdecRead0();
			break;
			
			mcase (0x1f801824): // MDEC
				//ret = psxHu32(addr); // old
				ret = mdecRead1();
			break;

			// ------------------------------------------------------------------------

			mcase(0x1f80146e):
				ret = DEV9read16( addr );
			break;

			default:
				ret = psxHu32(addr);
			break;
		}
	}

	return ret;
}

// Some Page 2 mess?  I love random question marks for comments!
//case 0x1f802030: hard =   //int_2000????
//case 0x1f802040: hard =//dip switches...??

//////////////////////////////////////////////////////////////////////////////////////////
//
mem16_t iopHwRead16_Page1( u32 addr )
{
	return _HwRead_16or32_Page1<mem16_t>( addr );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
mem16_t iopHwRead16_Page3( u32 addr )
{
	return psxHu16(addr);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
mem16_t iopHwRead16_Page8( u32 addr )
{
	return psxHu16(addr);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
mem32_t iopHwRead32_Page1( u32 addr )
{
	return _HwRead_16or32_Page1<mem32_t>( addr );
}

//////////////////////////////////////////////////////////////////////////////////////////
//
mem32_t iopHwRead32_Page3( u32 addr )
{
	return psxHu32(addr);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
mem32_t iopHwRead32_Page8( u32 addr )
{
	u32 masked_addr = addr & 0x0fff;
	mem32_t ret;

	if( masked_addr >= 0x200 )
	{
		if( masked_addr < 0x240 )
		{
			const int parm = (masked_addr-0x200) / 4;
			ret = sio2_getSend3( parm );
		}
		else if( masked_addr < 0x260 )
		{
			// SIO2 Send commands alternate registers.  First reg maps to Send1, second
			// to Send2, third to Send1, etc.  And the following clever code does this:

			const int parm = (masked_addr-0x240) / 8;
			ret = (masked_addr & 4) ? sio2_getSend2( parm ) : sio2_getSend1( parm );
		}
		else if( masked_addr <= 0x280 )
		{
			switch( masked_addr )
			{
				mcase(HW_SIO2_CTRL):	ret = sio2_getCtrl();	break;
				mcase(HW_SIO2_RECV1):	ret = sio2_getRecv1();	break;
				mcase(HW_SIO2_RECV2):	ret = sio2_getRecv2();	break;
				mcase(HW_SIO2_RECV3):	ret = sio2_getRecv3();	break;
				mcase(0x1f808278):		ret = sio2_get8278();	break;
				mcase(0x1f80827C):		ret = sio2_get827C();	break;
				mcase(HW_SIO2_INTR):	ret = sio2_getIntr();	break;

				// HW_SIO2_FIFO -- A yet unknown: Should this be ignored on 32 bit writes, or handled as a
				// 4-byte FIFO input?
				// The old IOP system just ignored it, so that's what we do here.  I've included commented code
				// for treating it as a 16/32 bit write though [which is what the SIO does, for example).
				mcase(HW_SIO2_FIFO) :
					ret = psxHu32(addr);
				break;

				default:
					ret = psxHu32(addr);
				break;
			}
		}
		else if( masked_addr >= pgmsk(HW_FW_START) && masked_addr <= pgmsk(HW_FW_END) )
		{
			ret = FWread32( addr );
		} else {
			ret = psxHu32(addr);
		}
	}
	else ret = psxHu32(addr);

	return ret;
}

}

