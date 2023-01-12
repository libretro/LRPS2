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
#include "Vif_Dma.h"
#include "newVif.h"
#include "VUmicro.h"
#include "MTVU.h"

#define vifOp(vifCodeName) _vifT int vifCodeName(int pass, const u32 *data)
#define pass1    if (pass == 0)
#define pass2    if (pass == 1)
#define pass3    if (pass == 2)
#define pass1or2 if (pass == 0 || pass == 1)
#define vif1Only() { if (!idx) return vifCode_Null<idx>(pass, (u32*)data); }
vifOp(vifCode_Null);

//------------------------------------------------------------------
// Vif0/Vif1 Misc Functions
//------------------------------------------------------------------

__ri void vifExecQueue(int idx)
{
	if (!GetVifX.queued_program || (VU0.VI[REG_VPU_STAT].UL & 1 << (idx * 8)))
		return;

	GetVifX.queued_program = false;

	if (!idx) vu0ExecMicro(vif0.queued_pc);
	else	  vu1ExecMicro(vif1.queued_pc);

	// Hack for Wakeboarding Unleashed, game runs a VU program in parallel with a VIF unpack list.
	// The start of the VU program clears the VU memory, while VIF populates it from behind, so we need to get the clear out of the way.
	/*if (idx && !INSTANT_VU1)
	{
		VU1.cycle -= 256;
		CpuVU1->ExecuteBlock(0);
	}*/
}

static __fi void vifFlush(int idx)
{
	vifExecQueue(idx);

	if (!idx) vif0FLUSH();
	else      vif1FLUSH();

	vifExecQueue(idx);
}

static __fi void vuExecMicro(int idx, u32 addr)
{
	VIFregisters& vifRegs = vifXRegs;

	vifFlush(idx);
	if(GetVifX.waitforvu)
		return;

	if (vifRegs.itops  > (idx ? 0x3ffu : 0xffu))
		vifRegs.itops &= (idx ? 0x3ffu : 0xffu);

	vifRegs.itop = vifRegs.itops;

	if (idx) {
		// in case we're handling a VIF1 execMicro, set the top with the tops value
		vifRegs.top = vifRegs.tops & 0x3ff;

		// is DBF flag set in VIF_STAT?
		if (vifRegs.stat.DBF) {
			// it is, so set tops with base, and clear the stat DBF flag
			vifRegs.tops = vifRegs.base;
			vifRegs.stat.DBF = false;
		}
		else {
			// it is not, so set tops with base + offset, and set stat DBF flag
			vifRegs.tops = vifRegs.base + vifRegs.ofst;
			vifRegs.stat.DBF = true;
		}
	}

	GetVifX.queued_program = true;
	if ((s32)addr == -1)
		GetVifX.queued_pc = addr;
	else
		GetVifX.queued_pc = addr & (idx ? 0x7ffu : 0x1ffu);
	GetVifX.unpackcalls = 0;

	if (!idx || (!THREAD_VU1 && !INSTANT_VU1))
		vifExecQueue(idx);
}

void ExecuteVU(int idx)
{
	vifStruct& vifX = GetVifX;

	if((vifX.cmd & 0x7f) == 0x17)
	{
		vuExecMicro(idx, -1);
		vifX.cmd = 0;
		vifX.pass = 0;
	}
	else if((vifX.cmd & 0x7f) == 0x14 || (vifX.cmd & 0x7f) == 0x15)
	{
		vuExecMicro(idx, (u16)(vifXRegs.code));
		vifX.cmd = 0;
		vifX.pass = 0;
	}
	vifExecQueue(idx);
}

//------------------------------------------------------------------
// Vif0/Vif1 Code Implementations
//------------------------------------------------------------------

vifOp(vifCode_Base) {
	vif1Only();
	pass1 { vif1Regs.base = vif1Regs.code & 0x3ff; vif1.cmd = 0; vif1.pass = 0; }
	return 1;
}

template<int idx> __fi int _vifCode_Direct(int pass, const u8* data, bool isDirectHL) {
	vif1Only();
	pass1 {
		int vifImm    = (u16)vif1Regs.code;
		vif1.tag.size = vifImm ? (vifImm*4) : (65536*4);
		vif1.pass = 1;
		return 1;
	}
	pass2 {
		GIF_TRANSFER_TYPE tranType = isDirectHL ? GIF_TRANS_DIRECTHL : GIF_TRANS_DIRECT;
		uint size = std::min(vif1.vifpacketsize, vif1.tag.size) * 4; // Get size in bytes
		uint ret  = gifUnit.TransferGSPacketData(tranType, (u8*)data, size);

		vif1.tag.size    -= ret/4; // Convert to u32's
		vif1Regs.stat.VGW = false;

		if (size != ret)
		{
			// Stall if gif didn't process all the data (path2 queued)
			vif1.vifstalled.enabled   = VifStallEnable(vif1ch);
			vif1.vifstalled.value = VIF_TIMING_BREAK;
			vif1Regs.stat.VGW = true;
			return 0;
		}
		if (vif1.tag.size == 0) {
			vif1.cmd = 0;
			vif1.pass = 0;
			vif1.vifstalled.enabled   = VifStallEnable(vif1ch);
			vif1.vifstalled.value = VIF_TIMING_BREAK;
		}
		return ret / 4;
	}
	return 0;
}

vifOp(vifCode_Direct) {
	return _vifCode_Direct<idx>(pass, (u8*)data, 0);
}

vifOp(vifCode_DirectHL) {
	return _vifCode_Direct<idx>(pass, (u8*)data, 1);
}

vifOp(vifCode_Flush) {
	vif1Only();
	pass1or2 {
		bool p1or2 = (gifRegs.stat.APATH != 0 && gifRegs.stat.APATH != 3);
		vif1Regs.stat.VGW = false;
		vifFlush(idx);
		if (gifUnit.checkPaths(1,1,0) || p1or2) {
			vif1Regs.stat.VGW = true;
			vif1.vifstalled.enabled   = VifStallEnable(vif1ch);
			vif1.vifstalled.value = VIF_TIMING_BREAK;
			return 0;
		}
		else vif1.cmd = 0;
		vif1.pass = 0;
	}
	return 1;
}

vifOp(vifCode_FlushA) {
	vif1Only();
	pass1or2 {
		u32       gifBusy   = gifUnit.checkPaths(1,1,1) || (gifRegs.stat.APATH != 0);
		vif1Regs.stat.VGW = false;
		vifFlush(idx);

		if (gifBusy) 
		{
			vif1Regs.stat.VGW = true;
			vif1.vifstalled.enabled   = VifStallEnable(vif1ch);
			vif1.vifstalled.value = VIF_TIMING_BREAK;
			return 0;
			
		}
		vif1.cmd = 0;
		vif1.pass = 0;
	}
	return 1;
}

// ToDo: FixMe
vifOp(vifCode_FlushE) {
	vifStruct& vifX = GetVifX;
	pass1 { vifFlush(idx); vifX.cmd = 0; vifX.pass = 0;}
	return 1;
}

vifOp(vifCode_ITop) {
	pass1 { vifXRegs.itops = vifXRegs.code & 0x3ff; GetVifX.cmd = 0; GetVifX.pass = 0; }
	return 1;
}

vifOp(vifCode_Mark) {
	vifStruct& vifX = GetVifX;
	pass1 {
		vifXRegs.mark     = (u16)vifXRegs.code;
		vifXRegs.stat.MRK = true;
		vifX.cmd          = 0;
		vifX.pass		  = 0;
	}
	return 1;
}

static __fi void _vifCode_MPG(int idx, u32 addr, const u32 *data, int size) {
	VURegs& VUx = idx ? VU1 : VU0;
	vifStruct& vifX = GetVifX;
	u16 vuMemSize = idx ? 0x4000 : 0x1000;

	vifExecQueue(idx);

	if (idx && THREAD_VU1) {
		if ((addr + size * 4) > vuMemSize)
		{
			vu1Thread.WriteMicroMem(addr, (u8*)data, vuMemSize - addr);
			size -= (vuMemSize - addr) / 4;
			data += (vuMemSize - addr) / 4;
			vu1Thread.WriteMicroMem(0, (u8*)data, size * 4);
			vifX.tag.addr = size * 4;
		} 		
		else
		{
			vu1Thread.WriteMicroMem(addr, (u8*)data, size * 4);
			vifX.tag.addr += size * 4;
		}
		return;
	}	

	// Don't forget the Unsigned designator for these checks
	if((addr + size *4) > vuMemSize)
	{
		if (!idx)  CpuVU0->Clear(addr, vuMemSize - addr);
		else	   CpuVU1->Clear(addr, vuMemSize - addr);
		
		memcpy(VUx.Micro + addr, data, vuMemSize - addr);
		size -= (vuMemSize - addr) / 4;
		data += (vuMemSize - addr) / 4;
		memcpy(VUx.Micro, data, size * 4);

		vifX.tag.addr = size * 4;
	}
	else
	{
		// Clear VU memory before writing!
		if (!idx)  CpuVU0->Clear(addr, size*4);
		else	   CpuVU1->Clear(addr, size*4);
		memcpy(VUx.Micro + addr, data, size*4); //from tests, memcpy is 1fps faster on Grandia 3 than memcpy

		vifX.tag.addr   +=   size * 4;
	}
}

vifOp(vifCode_MPG) {
	vifStruct& vifX = GetVifX;
	pass1 {
		int    vifNum =  (u8)(vifXRegs.code >> 16);
		vifX.tag.addr = (u16)(vifXRegs.code <<  3) & (idx ? 0x3fff : 0xfff);
		vifX.tag.size = vifNum ? (vifNum*2) : 512;
		vifFlush(idx);

		if(vifX.vifstalled.enabled) return 0;
		else
		{
			vifX.pass = 1;
			return 1;
		}
	}
	pass2 {
		if (vifX.vifpacketsize < vifX.tag.size) { // Partial Transfer
			_vifCode_MPG(idx,    vifX.tag.addr, data, vifX.vifpacketsize);
			vifX.tag.size -= vifX.vifpacketsize; //We can do this first as its passed as a pointer
			return vifX.vifpacketsize;
		}
		else { // Full Transfer
			_vifCode_MPG(idx,  vifX.tag.addr, data, vifX.tag.size);
			int ret = vifX.tag.size;
			vifX.tag.size = 0;
			vifX.cmd      = 0;
			vifX.pass		= 0;
			return ret;
		}
	}
	return 0;
}

vifOp(vifCode_MSCAL) {
	vifStruct& vifX = GetVifX;
	pass1 { 
		vifFlush(idx); 

		if(!vifX.waitforvu)
		{
			vuExecMicro(idx, (u16)(vifXRegs.code)); 
			vifX.cmd = 0;
			vifX.pass = 0;
			if(GetVifX.vifpacketsize > 1)
			{
				//Warship Gunner 2 has a rather big dislike for the delays
				if(((data[1] >> 24) & 0x60) == 0x60) // Immediate following Unpack
				{ 
					//Snowblind games only use MSCAL, so other MS kicks force the program directly.
					vifExecQueue(idx);
				}
			}
		} 
	}
	return 1;
}

vifOp(vifCode_MSCALF) {
	vifStruct& vifX = GetVifX;
	pass1or2 {
		vifXRegs.stat.VGW = false;
		vifFlush(idx);
		if (u32 a = gifUnit.checkPaths(1,1,0)) {
			vif1Regs.stat.VGW = true;
			vifX.vifstalled.enabled   = VifStallEnable(vifXch);
			vifX.vifstalled.value = VIF_TIMING_BREAK;
		}
		if(!vifX.waitforvu)
		{
			vuExecMicro(idx, (u16)(vifXRegs.code));
			vifX.cmd = 0;
			vifX.pass = 0;
			vifExecQueue(idx);
		}
	}
	return 1;
}

vifOp(vifCode_MSCNT) {
	vifStruct& vifX = GetVifX;
	pass1 { 
		vifFlush(idx); 
		if(!vifX.waitforvu)
		{
			vuExecMicro(idx, -1);
			vifX.cmd = 0;
			vifX.pass = 0;
			if (GetVifX.vifpacketsize > 1)
			{
				if (((data[1] >> 24) & 0x60) == 0x60) // Immediate following Unpack
				{
					vifExecQueue(idx);
				}
			}
		}
	}
	return 1;
}

// ToDo: FixMe
vifOp(vifCode_MskPath3) {
	vif1Only();
	pass1 {
		vif1Regs.mskpath3 = (vif1Regs.code >> 15) & 0x1;
		gifRegs.stat.M3P  = (vif1Regs.code >> 15) & 0x1;
		if(!vif1Regs.mskpath3)
			gifInterrupt();
		vif1.cmd = 0;
		vif1.pass = 0;
	}
	return 1;
}

vifOp(vifCode_Nop) {
	pass1 { 
		GetVifX.cmd = 0;
		GetVifX.pass = 0;
		vifExecQueue(idx);

		if (GetVifX.vifpacketsize > 1)
		{
			if(((data[1] >> 24) & 0x7f) == 0x6 && (data[1] & 0x1)) //is mskpath3 next
			{ 
				GetVifX.vifstalled.enabled   = VifStallEnable(vifXch);
				GetVifX.vifstalled.value = VIF_TIMING_BREAK;
			}
		}
	}
	return 1;
}

// ToDo: Review Flags
vifOp(vifCode_Null) {
	vifStruct& vifX = GetVifX;
	pass1 {
		// if ME1, then force the vif to interrupt
		if (!(vifXRegs.err.ME1))
		{
			// Ignore vifcode and tag mismatch error
			vifXRegs.stat.ER1       = true;
			vifX.vifstalled.enabled = VifStallEnable(vifXch);
			vifX.vifstalled.value   = VIF_IRQ_STALL;
		}
		vifX.cmd  = 0;
		vifX.pass = 0;

		//If the top bit was set to interrupt, we don't want it to take commands from a bad code
		if (vifXRegs.code & 0x80000000)
			vifX.irq = 0;
	}
	return 1;
}

vifOp(vifCode_Offset) {
	vif1Only();
	pass1 {
		vif1Regs.stat.DBF	= false;
		vif1Regs.ofst		= vif1Regs.code & 0x3ff;
		vif1Regs.tops		= vif1Regs.base;
		vif1.cmd			= 0;
		vif1.pass			= 0;
	}
	return 1;
}

template<int idx> static __fi int _vifCode_STColRow(const u32* data, u32* pmem2) {
	vifStruct& vifX = GetVifX;

	int ret = std::min(4 - vifX.tag.addr, vifX.vifpacketsize);

	switch (ret) {
		case 4: 
			pmem2[3] = data[3]; 
			// Fall through
		case 3: 
			pmem2[2] = data[2];
			// Fall through
		case 2: 
			pmem2[1] = data[1];
			// Fall through
		case 1: 
			pmem2[0] = data[0];
			break;
		default:
			break;
	}

	vifX.tag.addr += ret;
	vifX.tag.size -= ret;
	if (!vifX.tag.size)
	{
		vifX.pass = 0;
		vifX.cmd = 0;
	}

	

	return ret;
}

vifOp(vifCode_STCol) {
	vifStruct& vifX = GetVifX;
	pass1 {
		vifX.tag.addr = 0;
		vifX.tag.size = 4;
		vifX.pass = 1;
		return 1;
	}
	pass2 {
		u32 ret = _vifCode_STColRow<idx>(data, &vifX.MaskCol._u32[vifX.tag.addr]);
		if (idx && vifX.tag.size == 0)
			vu1Thread.WriteCol(vifX);
		return ret;
	}
	return 0;
}

vifOp(vifCode_STRow) {
	vifStruct& vifX = GetVifX;
	pass1 {
		vifX.tag.addr = 0;
		vifX.tag.size = 4;
		vifX.pass = 1;
		return 1;
	}
	pass2 {
		u32 ret = _vifCode_STColRow<idx>(data, &vifX.MaskRow._u32[vifX.tag.addr]);
		if (idx && vifX.tag.size == 0)
			vu1Thread.WriteRow(vifX);
		return ret;
	}
	return 1;
}

vifOp(vifCode_STCycl) {
	vifStruct& vifX = GetVifX;
	pass1 {
		vifXRegs.cycle.cl = (u8)(vifXRegs.code);
		vifXRegs.cycle.wl = (u8)(vifXRegs.code >> 8);
		vifX.cmd		   = 0;
		vifX.pass		   = 0;
	}
	return 1;
}

vifOp(vifCode_STMask) {
	vifStruct& vifX = GetVifX;
	pass1 { vifX.tag.size = 1; vifX.pass = 1; return 1; }
	pass2 { vifXRegs.mask = data[0]; vifX.tag.size = 0; vifX.cmd = 0; vifX.pass = 0;}
	return 1;
}

vifOp(vifCode_STMod) {
	pass1 { vifXRegs.mode = vifXRegs.code & 0x3; GetVifX.cmd = 0; GetVifX.pass = 0;}
	return 1;
}

vifOp(vifCode_Unpack) {
	pass1 {
		vifUnpackSetup<idx>(data);
		
		return 1;
	}
	pass2 { 
		return nVifUnpack<idx>((u8*)data);
	}
	return 0;
}

//------------------------------------------------------------------
// Vif0/Vif1 Code Tables
//------------------------------------------------------------------

__aligned16 FnType_VifCmdHandler* const vifCmdHandler[2][128] =
{
	{
		vifCode_Nop<0>     , vifCode_STCycl<0>  , vifCode_Offset<0>	, vifCode_Base<0>   , vifCode_ITop<0>   , vifCode_STMod<0>  , vifCode_MskPath3<0>, vifCode_Mark<0>,   /*0x00*/
		vifCode_Null<0>    , vifCode_Null<0>    , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x08*/
		vifCode_FlushE<0>  , vifCode_Flush<0>   , vifCode_Null<0>	, vifCode_FlushA<0> , vifCode_MSCAL<0>  , vifCode_MSCALF<0> , vifCode_Null<0>	 , vifCode_MSCNT<0>,  /*0x10*/
		vifCode_Null<0>    , vifCode_Null<0>    , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x18*/
		vifCode_STMask<0>  , vifCode_Null<0>    , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>	 , vifCode_Null<0>,   /*0x20*/
		vifCode_Null<0>    , vifCode_Null<0>    , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>	 , vifCode_Null<0>,   /*0x28*/
		vifCode_STRow<0>   , vifCode_STCol<0>	, vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>	 , vifCode_Null<0>,   /*0x30*/
		vifCode_Null<0>    , vifCode_Null<0>    , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x38*/
		vifCode_Null<0>    , vifCode_Null<0>    , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x40*/
		vifCode_Null<0>    , vifCode_Null<0>    , vifCode_MPG<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x48*/
		vifCode_Direct<0>  , vifCode_DirectHL<0>, vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x50*/
		vifCode_Null<0>	   , vifCode_Null<0>	, vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x58*/
		vifCode_Unpack<0>  , vifCode_Unpack<0>  , vifCode_Unpack<0>	, vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0>  , vifCode_Null<0>,   /*0x60*/
		vifCode_Unpack<0>  , vifCode_Unpack<0>  , vifCode_Unpack<0>	, vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0>  , vifCode_Unpack<0>, /*0x68*/
		vifCode_Unpack<0>  , vifCode_Unpack<0>  , vifCode_Unpack<0>	, vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0>  , vifCode_Null<0>,   /*0x70*/
		vifCode_Unpack<0>  , vifCode_Unpack<0>  , vifCode_Unpack<0>	, vifCode_Null<0>   , vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0>  , vifCode_Unpack<0>  /*0x78*/
	},
	{
		vifCode_Nop<1>     , vifCode_STCycl<1>  , vifCode_Offset<1>	, vifCode_Base<1>   , vifCode_ITop<1>   , vifCode_STMod<1>  , vifCode_MskPath3<1>, vifCode_Mark<1>,   /*0x00*/
		vifCode_Null<1>    , vifCode_Null<1>    , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x08*/
		vifCode_FlushE<1>  , vifCode_Flush<1>   , vifCode_Null<1>	, vifCode_FlushA<1> , vifCode_MSCAL<1>  , vifCode_MSCALF<1> , vifCode_Null<1>	 , vifCode_MSCNT<1>,  /*0x10*/
		vifCode_Null<1>    , vifCode_Null<1>    , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x18*/
		vifCode_STMask<1>  , vifCode_Null<1>    , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>	 , vifCode_Null<1>,   /*0x20*/
		vifCode_Null<1>    , vifCode_Null<1>    , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>	 , vifCode_Null<1>,   /*0x28*/
		vifCode_STRow<1>   , vifCode_STCol<1>	, vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>	 , vifCode_Null<1>,   /*0x30*/
		vifCode_Null<1>    , vifCode_Null<1>    , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x38*/
		vifCode_Null<1>    , vifCode_Null<1>    , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x40*/
		vifCode_Null<1>    , vifCode_Null<1>    , vifCode_MPG<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x48*/
		vifCode_Direct<1>  , vifCode_DirectHL<1>, vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x50*/
		vifCode_Null<1>	   , vifCode_Null<1>	, vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x58*/
		vifCode_Unpack<1>  , vifCode_Unpack<1>  , vifCode_Unpack<1>	, vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1>  , vifCode_Null<1>,   /*0x60*/
		vifCode_Unpack<1>  , vifCode_Unpack<1>  , vifCode_Unpack<1>	, vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1>  , vifCode_Unpack<1>, /*0x68*/
		vifCode_Unpack<1>  , vifCode_Unpack<1>  , vifCode_Unpack<1>	, vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1>  , vifCode_Null<1>,   /*0x70*/
		vifCode_Unpack<1>  , vifCode_Unpack<1>  , vifCode_Unpack<1>	, vifCode_Null<1>   , vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1>  , vifCode_Unpack<1>  /*0x78*/
	}
};
