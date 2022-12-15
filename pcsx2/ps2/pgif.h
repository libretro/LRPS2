/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2016-2016  PCSX2 Dev Team
*  Copyright (C) 2016 Wisi
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

void pgifInit(void);

extern void psxGPUw(int, u32);
extern u32 psxGPUr(int);

extern void PGIFw(int, u32);
extern u32 PGIFr(int);

extern void PGIFwQword(u32 addr, void *);
extern void PGIFrQword(u32 addr, void *);

extern u32 psxDma2GpuR(u32 addr);
extern void psxDma2GpuW(u32 addr, u32 data);
