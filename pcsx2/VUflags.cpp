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

#include <float.h>

#include "VUmicro.h"

__ri u32 VU_MAC_UPDATE( int shift, VURegs * VU, float f )
{
	u32 v   = *(u32*)&f;
	u32 s   = v & 0x80000000;

	if (s)
		VU->macflag |= 0x0010<<shift;
	else
		VU->macflag &= ~(0x0010<<shift);

	if( f == 0 )
		VU->macflag = (VU->macflag & ~(0x1100<<shift)) | (0x0001<<shift);
	else
	{
		int exp = (v >> 23) & 0xff;

		switch(exp)
		{
			case 0:
				VU->macflag = (VU->macflag&~(0x1000<<shift)) | (0x0101<<shift);
				return s;
			case 255:
				VU->macflag = (VU->macflag&~(0x0100<<shift)) | (0x1000<<shift);
				return s|0x7f7fffff; /* max allowed */
			default:
				VU->macflag = (VU->macflag & ~(0x1101<<shift));
				break;
		}
	}

	return v;
}

__ri void VU_STAT_UPDATE(VURegs * VU)
{
	int newflag = 0 ;
	if (VU->macflag & 0x000F) newflag  = 0x1;
	if (VU->macflag & 0x00F0) newflag |= 0x2;
	if (VU->macflag & 0x0F00) newflag |= 0x4;
	if (VU->macflag & 0xF000) newflag |= 0x8;
	VU->statusflag = (VU->statusflag&0xc30) | newflag | ((VU->statusflag&0xf)<<6);
}
