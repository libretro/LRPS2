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


#include "PrecompiledHeader.h"
#include "Common.h"
#include "COP0.h"

u32 s_iLastCOP0Cycle = 0;
u32 s_iLastPERFCycle[2] = { 0, 0 };

void __fastcall WriteCP0Status(u32 value) {

	//DMA_LOG("COP0 Status write = 0x%08x", value);

	cpuRegs.CP0.n.Status.val = value;
    cpuSetNextEventDelta(4);
}


//////////////////////////////////////////////////////////////////////////////////////////
// Performance Counters Update Stuff!
//
// Note regarding updates of PERF and TIMR registers: never allow increment to be 0.
// That happens when a game loads the MFC0 twice in the same recompiled block (before the
// cpuRegs.cycles update), and can cause games to lock up since it's an unexpected result.
//
// PERF Overflow exceptions:  The exception is raised when the MSB of the Performance
// Counter Register is set.  I'm assuming the exception continues to re-raise until the
// app clears the bit manually (needs testing).
//
// PERF Events:
//  * Event 0 on PCR 0 is unused (counter disable)
//  * Event 16 is usable as a specific counter disable bit (since CTE affects both counters)
//  * Events 17-31 are reserved (act as counter disable)
//
// Most event mode aren't supported, and issue a warning and do a standard instruction
// count.  But only mode 1 (instruction counter) has been found to be used by games thus far.
//

static __fi bool PERF_ShouldCountEvent( uint evt )
{
	switch( evt )
	{
		case 1:		// cpu cycle counter.
		case 2:		// single/dual instruction issued
		case 3:		// Branch issued / Branch mispredicated
		case 12:	// Instruction completed
		case 13:	// non-delayslot instruction completed
		case 14:	// COP2/COP1 instruction complete
		case 15:	// Load/Store completed
			return true;
		default:
			break;
	}

	return false;
}

__fi void COP0_UpdatePCCR()
{
	// Counting and counter exceptions are not performed if we are currently executing a Level 2 exception (ERL)
	// or the counting function is not enabled (CTE)
	if (cpuRegs.CP0.n.Status.b.ERL || !cpuRegs.PERF.n.pccr.b.CTE)
	{
		s_iLastPERFCycle[0] = cpuRegs.cycle;
		s_iLastPERFCycle[1] = s_iLastPERFCycle[0];
		return;
	}

	// Implemented memory mode check (kernel/super/user)

	if( cpuRegs.PERF.n.pccr.val & ((1 << (cpuRegs.CP0.n.Status.b.KSU + 2)) | (cpuRegs.CP0.n.Status.b.EXL << 1)))
	{
		// ----------------------------------
		//    Update Performance Counter 0
		// ----------------------------------

		if( PERF_ShouldCountEvent( cpuRegs.PERF.n.pccr.b.Event0 ) )
		{
			u32 incr = cpuRegs.cycle - s_iLastPERFCycle[0];
			if( incr == 0 ) incr++;

			// use prev/XOR method for one-time exceptions (but likely less correct)
			//u32 prev = cpuRegs.PERF.n.pcr0;
			cpuRegs.PERF.n.pcr0 += incr;
			s_iLastPERFCycle[0] = cpuRegs.cycle;
		}
	}

	if( cpuRegs.PERF.n.pccr.val & ((1 << (cpuRegs.CP0.n.Status.b.KSU + 12)) | (cpuRegs.CP0.n.Status.b.EXL << 11)))
	{
		// ----------------------------------
		//    Update Performance Counter 1
		// ----------------------------------

		if( PERF_ShouldCountEvent( cpuRegs.PERF.n.pccr.b.Event1 ) )
		{
			u32 incr = cpuRegs.cycle - s_iLastPERFCycle[1];
			if( incr == 0 ) incr++;

			cpuRegs.PERF.n.pcr1 += incr;
			s_iLastPERFCycle[1] = cpuRegs.cycle;

		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//


void MapTLB(int i)
{
	u32 mask, addr;
	u32 saddr, eaddr;

	COP0_LOG("MAP TLB %d: 0x%08X-> [0x%08X 0x%08X] S=%d G=%d ASID=%d Mask=0x%03X EntryLo0 PFN=%x EntryLo0 Cache=%x EntryLo1 PFN=%x EntryLo1 Cache=%x VPN2=%x",
		i, tlb[i].VPN2, tlb[i].PFN0, tlb[i].PFN1, tlb[i].S >> 31, tlb[i].G, tlb[i].ASID,
		tlb[i].Mask, tlb[i].EntryLo0 >> 6, (tlb[i].EntryLo0 & 0x38) >> 3, tlb[i].EntryLo1 >> 6, (tlb[i].EntryLo1 & 0x38) >> 3, tlb[i].VPN2);

	if (tlb[i].S)
	{
		vtlb_VMapBuffer(tlb[i].VPN2, eeMem->Scratch, Ps2MemSize::Scratch);
	}

	if (tlb[i].VPN2 == 0x70000000) return; //uh uhh right ...
	if (tlb[i].EntryLo0 & 0x2) {
		mask  = ((~tlb[i].Mask) << 1) & 0xfffff;
		saddr = tlb[i].VPN2 >> 12;
		eaddr = saddr + tlb[i].Mask + 1;

		for (addr=saddr; addr<eaddr; addr++) {
			if ((addr & mask) == ((tlb[i].VPN2 >> 12) & mask)) { //match
				memSetPageAddr(addr << 12, tlb[i].PFN0 + ((addr - saddr) << 12));
				Cpu->Clear(addr << 12, 0x400);
			}
		}
	}

	if (tlb[i].EntryLo1 & 0x2) {
		mask  = ((~tlb[i].Mask) << 1) & 0xfffff;
		saddr = (tlb[i].VPN2 >> 12) + tlb[i].Mask + 1;
		eaddr = saddr + tlb[i].Mask + 1;

		for (addr=saddr; addr<eaddr; addr++) {
			if ((addr & mask) == ((tlb[i].VPN2 >> 12) & mask)) { //match
				memSetPageAddr(addr << 12, tlb[i].PFN1 + ((addr - saddr) << 12));
				Cpu->Clear(addr << 12, 0x400);
			}
		}
	}
}

void UnmapTLB(int i)
{
	//log_cb(RETRO_LOG_DEBUG, "Clear TLB %d: %08x-> [%08x %08x] S=%d G=%d ASID=%d Mask= %03X\n", i,tlb[i].VPN2,tlb[i].PFN0,tlb[i].PFN1,tlb[i].S,tlb[i].G,tlb[i].ASID,tlb[i].Mask);
	u32 mask, addr;
	u32 saddr, eaddr;

	if (tlb[i].S)
	{
		vtlb_VMapUnmap(tlb[i].VPN2,0x4000);
		return;
	}

	if (tlb[i].EntryLo0 & 0x2)
	{
		mask  = ((~tlb[i].Mask) << 1) & 0xfffff;
		saddr = tlb[i].VPN2 >> 12;
		eaddr = saddr + tlb[i].Mask + 1;
		//log_cb(RETRO_LOG_DEBUG, "Clear TLB: %08x ~ %08x\n",saddr,eaddr-1);
		for (addr=saddr; addr<eaddr; addr++) {
			if ((addr & mask) == ((tlb[i].VPN2 >> 12) & mask)) { //match
				memClearPageAddr(addr << 12);
				Cpu->Clear(addr << 12, 0x400);
			}
		}
	}

	if (tlb[i].EntryLo1 & 0x2) {
		mask  = ((~tlb[i].Mask) << 1) & 0xfffff;
		saddr = (tlb[i].VPN2 >> 12) + tlb[i].Mask + 1;
		eaddr = saddr + tlb[i].Mask + 1;
		//log_cb(RETRO_LOG_DEBUG, "Clear TLB: %08x ~ %08x\n",saddr,eaddr-1);
		for (addr=saddr; addr<eaddr; addr++) {
			if ((addr & mask) == ((tlb[i].VPN2 >> 12) & mask)) { //match
				memClearPageAddr(addr << 12);
				Cpu->Clear(addr << 12, 0x400);
			}
		}
	}
}

void WriteTLB(int i)
{
	tlb[i].PageMask = cpuRegs.CP0.n.PageMask;
	tlb[i].EntryHi = cpuRegs.CP0.n.EntryHi;
	tlb[i].EntryLo0 = cpuRegs.CP0.n.EntryLo0;
	tlb[i].EntryLo1 = cpuRegs.CP0.n.EntryLo1;

	tlb[i].Mask = (cpuRegs.CP0.n.PageMask >> 13) & 0xfff;
	tlb[i].nMask = (~tlb[i].Mask) & 0xfff;
	tlb[i].VPN2 = ((cpuRegs.CP0.n.EntryHi >> 13) & (~tlb[i].Mask)) << 13;
	tlb[i].ASID = cpuRegs.CP0.n.EntryHi & 0xfff;
	tlb[i].G = cpuRegs.CP0.n.EntryLo0 & cpuRegs.CP0.n.EntryLo1 & 0x1;
	tlb[i].PFN0 = (((cpuRegs.CP0.n.EntryLo0 >> 6) & 0xFFFFF) & (~tlb[i].Mask)) << 12;
	tlb[i].PFN1 = (((cpuRegs.CP0.n.EntryLo1 >> 6) & 0xFFFFF) & (~tlb[i].Mask)) << 12;
	tlb[i].S = cpuRegs.CP0.n.EntryLo0&0x80000000;

	MapTLB(i);
}

namespace R5900 {
namespace Interpreter {
namespace OpcodeImpl {
namespace COP0 {

void TLBR() {
	COP0_LOG("COP0_TLBR %d:%x,%x,%x,%x",
			cpuRegs.CP0.n.Index,   cpuRegs.CP0.n.PageMask, cpuRegs.CP0.n.EntryHi,
			cpuRegs.CP0.n.EntryLo0, cpuRegs.CP0.n.EntryLo1);

	int i = cpuRegs.CP0.n.Index & 0x3f;

	cpuRegs.CP0.n.PageMask = tlb[i].PageMask;
	cpuRegs.CP0.n.EntryHi = tlb[i].EntryHi&~(tlb[i].PageMask|0x1f00);
	cpuRegs.CP0.n.EntryLo0 = (tlb[i].EntryLo0&~1)|((tlb[i].EntryHi>>12)&1);
	cpuRegs.CP0.n.EntryLo1 =(tlb[i].EntryLo1&~1)|((tlb[i].EntryHi>>12)&1);
}

void TLBWI() {
	int j = cpuRegs.CP0.n.Index & 0x3f;

	//if (j > 48) return;

	COP0_LOG("COP0_TLBWI %d:%x,%x,%x,%x",
			cpuRegs.CP0.n.Index,    cpuRegs.CP0.n.PageMask, cpuRegs.CP0.n.EntryHi,
			cpuRegs.CP0.n.EntryLo0, cpuRegs.CP0.n.EntryLo1);

	UnmapTLB(j);
	tlb[j].PageMask = cpuRegs.CP0.n.PageMask;
	tlb[j].EntryHi = cpuRegs.CP0.n.EntryHi;
	tlb[j].EntryLo0 = cpuRegs.CP0.n.EntryLo0;
	tlb[j].EntryLo1 = cpuRegs.CP0.n.EntryLo1;
	WriteTLB(j);
}

void TLBWR() {
	int j = cpuRegs.CP0.n.Random & 0x3f;

	//if (j > 48) return;

#ifndef NDEBUG
	log_cb(RETRO_LOG_DEBUG, "COP0_TLBWR %d:%x,%x,%x,%x\n",
			cpuRegs.CP0.n.Random,   cpuRegs.CP0.n.PageMask, cpuRegs.CP0.n.EntryHi,
			cpuRegs.CP0.n.EntryLo0, cpuRegs.CP0.n.EntryLo1);
#endif

	//if (j > 48) return;

	UnmapTLB(j);
	tlb[j].PageMask = cpuRegs.CP0.n.PageMask;
	tlb[j].EntryHi = cpuRegs.CP0.n.EntryHi;
	tlb[j].EntryLo0 = cpuRegs.CP0.n.EntryLo0;
	tlb[j].EntryLo1 = cpuRegs.CP0.n.EntryLo1;
	WriteTLB(j);
}

void TLBP() {
	int i;

	union {
		struct {
			u32 VPN2:19;
			u32 VPN2X:2;
			u32 G:3;
			u32 ASID:8;
		} s;
		u32 u;
	} EntryHi32;

	EntryHi32.u = cpuRegs.CP0.n.EntryHi;

	cpuRegs.CP0.n.Index=0xFFFFFFFF;
	for(i=0;i<48;i++){
		if (tlb[i].VPN2 == ((~tlb[i].Mask) & (EntryHi32.s.VPN2))
		&& ((tlb[i].G&1) || ((tlb[i].ASID & 0xff) == EntryHi32.s.ASID))) {
			cpuRegs.CP0.n.Index = i;
			break;
		}
	}
	 if(cpuRegs.CP0.n.Index == 0xFFFFFFFF) cpuRegs.CP0.n.Index = 0x80000000;
}

void MFC0()
{
	// Note on _Rd_ Condition 9: CP0.Count should be updated even if _Rt_ is 0.
	if ((_Rd_ != 9) && !_Rt_ ) return;

#if 0
	if(bExecBIOS == FALSE && _Rd_ == 25)
		log_cb(RETRO_LOG_DEBUG, "MFC0 _Rd_ %x = %x\n", _Rd_, cpuRegs.CP0.r[_Rd_]);
#endif
	switch (_Rd_)
	{
		case 12:
			cpuRegs.GPR.r[_Rt_].SD[0] = (s32)(cpuRegs.CP0.r[_Rd_] & 0xf0c79c1f);
		break;

		case 25:
			if (0 == (_Imm_ & 1)) // MFPS, register value ignored
			{
				cpuRegs.GPR.r[_Rt_].SD[0] = (s32)cpuRegs.PERF.n.pccr.val;
			}
			else if (0 == (_Imm_ & 2)) // MFPC 0, only LSB of register matters
			{
				COP0_UpdatePCCR();
				cpuRegs.GPR.r[_Rt_].SD[0] = (s32)cpuRegs.PERF.n.pcr0;
			}
			else // MFPC 1
			{
				COP0_UpdatePCCR();
				cpuRegs.GPR.r[_Rt_].SD[0] = (s32)cpuRegs.PERF.n.pcr1;
			}
			/*
			   log_cb(RETRO_LOG_DEBUG, "MFC0 PCCR = %x PCR0 = %x PCR1 = %x IMM= %x\n",  params
			   cpuRegs.PERF.n.pccr, cpuRegs.PERF.n.pcr0, cpuRegs.PERF.n.pcr1, _Imm_ & 0x3F);
			 */
		break;

		case 24:
			COP0_LOG("MFC0 Breakpoint debug Registers code = %x", cpuRegs.code & 0x3FF);
		break;

		case 9:
		{
			u32 incr = cpuRegs.cycle-s_iLastCOP0Cycle;
			if( incr == 0 ) incr++;
			cpuRegs.CP0.n.Count += incr;
			s_iLastCOP0Cycle = cpuRegs.cycle;
			if( !_Rt_ ) break;
		}

		default:
			cpuRegs.GPR.r[_Rt_].UD[0] = (s64)cpuRegs.CP0.r[_Rd_];
	}
}

void MTC0()
{
#if 0
	if(bExecBIOS == FALSE && _Rd_ == 25)
		log_cb(RETRO_LOG_DEBUG, "MTC0 _Rd_ %x = %x\n", _Rd_, cpuRegs.CP0.r[_Rd_]);
#endif
	switch (_Rd_)
	{
		case 9:
			s_iLastCOP0Cycle = cpuRegs.cycle;
			cpuRegs.CP0.r[9] = cpuRegs.GPR.r[_Rt_].UL[0];
		break;

		case 12:
			WriteCP0Status(cpuRegs.GPR.r[_Rt_].UL[0]);
		break;

		case 24:
			COP0_LOG("MTC0 Breakpoint debug Registers code = %x", cpuRegs.code & 0x3FF);
		break;

		case 25:
#if 0
		if(bExecBIOS == FALSE && _Rd_ == 25)
			log_cb(RETRO_LOG_DEBUG, "MTC0 PCCR = %x PCR0 = %x PCR1 = %x IMM= %x\n", params
					cpuRegs.PERF.n.pccr, cpuRegs.PERF.n.pcr0, cpuRegs.PERF.n.pcr1, _Imm_ & 0x3F);
#endif
			if (0 == (_Imm_ & 1)) // MTPS
			{
				if (0 != (_Imm_ & 0x3E)) // only effective when the register is 0
					break;
				// Updates PCRs and sets the PCCR.
				COP0_UpdatePCCR();
				cpuRegs.PERF.n.pccr.val = cpuRegs.GPR.r[_Rt_].UL[0];
			}
			else if (0 == (_Imm_ & 2)) // MTPC 0, only LSB of register matters
			{
				cpuRegs.PERF.n.pcr0 = cpuRegs.GPR.r[_Rt_].UL[0];
				s_iLastPERFCycle[0] = cpuRegs.cycle;
			}
			else // MTPC 1
			{
				cpuRegs.PERF.n.pcr1 = cpuRegs.GPR.r[_Rt_].UL[0];
				s_iLastPERFCycle[1] = cpuRegs.cycle;
			}
		break;

		default:
			cpuRegs.CP0.r[_Rd_] = cpuRegs.GPR.r[_Rt_].UL[0];
		break;
	}
}

int CPCOND0() {
	return (((dmacRegs.stat.CIS | ~dmacRegs.pcr.CPC) & 0x3FF) == 0x3ff);
}

//#define CPCOND0	1

void BC0F() {
	if (CPCOND0() == 0) intDoBranch(_BranchTarget_);
}

void BC0T() {
	if (CPCOND0() == 1) intDoBranch(_BranchTarget_);
}

void BC0FL() {
	if (CPCOND0() == 0)
		intDoBranch(_BranchTarget_);
	else
		cpuRegs.pc+= 4;

}

void BC0TL() {
	if (CPCOND0() == 1)
		intDoBranch(_BranchTarget_);
	else
		cpuRegs.pc+= 4;
}

void ERET() {
	if (cpuRegs.CP0.n.Status.b.ERL) {
		cpuRegs.pc = cpuRegs.CP0.n.ErrorEPC;
		cpuRegs.CP0.n.Status.b.ERL = 0;
	} else {
		cpuRegs.pc = cpuRegs.CP0.n.EPC;
		cpuRegs.CP0.n.Status.b.EXL = 0;
	}
	cpuSetNextEventDelta(4);
	intSetBranch();
}

void DI() {
	if (cpuRegs.CP0.n.Status.b._EDI || cpuRegs.CP0.n.Status.b.EXL ||
		cpuRegs.CP0.n.Status.b.ERL || (cpuRegs.CP0.n.Status.b.KSU == 0)) {
		cpuRegs.CP0.n.Status.b.EIE = 0;
		// IRQs are disabled so no need to do a cpu exception/event test...
		//cpuSetNextEventDelta();
	}
}

void EI() {
	if (cpuRegs.CP0.n.Status.b._EDI || cpuRegs.CP0.n.Status.b.EXL ||
		cpuRegs.CP0.n.Status.b.ERL || (cpuRegs.CP0.n.Status.b.KSU == 0)) {
		cpuRegs.CP0.n.Status.b.EIE = 1;
		// schedule an event test, which will check for and raise pending IRQs.
		cpuSetNextEventDelta(4);
	}
}

} } } }	// end namespace R5900::Interpreter::OpcodeImpl
