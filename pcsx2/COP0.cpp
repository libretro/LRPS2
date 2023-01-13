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
#include "COP0.h"

void WriteCP0Status(u32 value)
{
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

/* 
 * This is a rough table of actions for various PCR modes.  
 * Some of these can be implemented more accurately later.
 * Others (WBBs in particular) probably cannot without some 
 * severe complications.
 * left sides are PCR0 / right sides are PCR1 
 *
 * 1       - cpu cycle counter
 * 2       - single/dual instruction issued
 * 3       - Branch issued / Branch mispredicated
 * 4       - BTAC/TLB miss
 * 5       - ITLB/DTLB miss
 * 6       - Data/Instruction cache miss
 * 7       - Access to DTLB / WBB single request fail
 * 8       - Non-blocking load / WBB burst request fail
 * 11      - CPU address bus busy / CPU data bus busy
 * 12      - Instruction completed
 * 13      - Non-delayslot instruction completed
 * 14      - COP1/COP2 instruction complete
 * 15      - Load/Store completed
 */
#define PERF_ShouldCountEvent(evt) ((evt == 1 || evt == 2  || evt == 3  || evt == 12 || evt == 13 || evt == 14 || evt == 15))

__fi void COP0_UpdatePCCR(void)
{
	// Counting and counter exceptions are not performed if we are currently executing a Level 2 exception (ERL)
	// or the counting function is not enabled (CTE)
	if (cpuRegs.CP0.n.Status.b.ERL || !cpuRegs.PERF.n.pccr.b.CTE)
	{
		cpuRegs.lastPERFCycle[0] = cpuRegs.cycle;
		cpuRegs.lastPERFCycle[1] = cpuRegs.lastPERFCycle[0];
		return;
	}

	// Implemented memory mode check (kernel/super/user)

	if( cpuRegs.PERF.n.pccr.val & ((1 << (cpuRegs.CP0.n.Status.b.KSU + 2)) | (cpuRegs.CP0.n.Status.b.EXL << 1)))
	{
		// ----------------------------------
		//    Update Performance Counter 0
		// ----------------------------------
		uint evt = cpuRegs.PERF.n.pccr.b.Event0;

		if (PERF_ShouldCountEvent(evt))
		{
			u32 incr = cpuRegs.cycle - cpuRegs.lastPERFCycle[0];
			if( incr == 0 ) incr++;

			// use prev/XOR method for one-time exceptions (but likely less correct)
			//u32 prev = cpuRegs.PERF.n.pcr0;
			cpuRegs.PERF.n.pcr0 += incr;
			cpuRegs.lastPERFCycle[0]  = cpuRegs.cycle;
		}
	}

	if( cpuRegs.PERF.n.pccr.val & ((1 << (cpuRegs.CP0.n.Status.b.KSU + 12)) | (cpuRegs.CP0.n.Status.b.EXL << 11)))
	{
		// ----------------------------------
		//    Update Performance Counter 1
		// ----------------------------------
		uint evt = cpuRegs.PERF.n.pccr.b.Event1;

		if (PERF_ShouldCountEvent(evt))
		{
			u32 incr = cpuRegs.cycle - cpuRegs.lastPERFCycle[1];
			if( incr == 0 ) incr++;

			cpuRegs.PERF.n.pcr1      += incr;
			cpuRegs.lastPERFCycle[1]  = cpuRegs.cycle;

		}
	}
}

void MapTLB(int i)
{
	if (tlb[i].S)
		vtlb_VMapBuffer(tlb[i].VPN2, eeMem->Scratch, Ps2MemSize::Scratch);

	if (tlb[i].VPN2 == 0x70000000)
		return; /* uh uhh right ... */

	if (tlb[i].EntryLo0 & 0x2)
	{
		u32 addr;
		u32 mask  = ((~tlb[i].Mask) << 1) & 0xfffff;
		u32 saddr = tlb[i].VPN2 >> 12;
		u32 eaddr = saddr + tlb[i].Mask + 1;

		for (addr=saddr; addr<eaddr; addr++)
		{
			if ((addr & mask) == ((tlb[i].VPN2 >> 12) & mask))
			{
				//match
				vtlb_VMap(addr << 12, tlb[i].PFN0 + ((addr - saddr) << 12), 0x1000);
				Cpu->Clear(addr << 12, 0x400);
			}
		}
	}

	if (tlb[i].EntryLo1 & 0x2)
	{
		u32 addr;
		u32 mask  = ((~tlb[i].Mask) << 1) & 0xfffff;
		u32 saddr = (tlb[i].VPN2 >> 12) + tlb[i].Mask + 1;
		u32 eaddr = saddr + tlb[i].Mask + 1;

		for (addr=saddr; addr<eaddr; addr++)
		{
			if ((addr & mask) == ((tlb[i].VPN2 >> 12) & mask))
			{
				//match
				vtlb_VMap(addr << 12, tlb[i].PFN1 + ((addr - saddr) << 12), 0x1000);
				Cpu->Clear(addr << 12, 0x400);
			}
		}
	}
}

void UnmapTLB(int i)
{
	if (tlb[i].S)
	{
		vtlb_VMapUnmap(tlb[i].VPN2,0x4000);
		return;
	}

	if (tlb[i].EntryLo0 & 0x2)
	{
		u32 addr;
		u32 mask  = ((~tlb[i].Mask) << 1) & 0xfffff;
		u32 saddr = tlb[i].VPN2 >> 12;
		u32 eaddr = saddr + tlb[i].Mask + 1;
		for (addr=saddr; addr<eaddr; addr++)
		{
			if ((addr & mask) == ((tlb[i].VPN2 >> 12) & mask)) /* match */
			{
				vtlb_VMapUnmap(addr << 12, 0x1000);
				Cpu->Clear(addr << 12, 0x400);
			}
		}
	}

	if (tlb[i].EntryLo1 & 0x2)
	{
		u32 addr;
		u32 mask  = ((~tlb[i].Mask) << 1) & 0xfffff;
		u32 saddr = (tlb[i].VPN2 >> 12) + tlb[i].Mask + 1;
		u32 eaddr = saddr + tlb[i].Mask + 1;
		for (addr=saddr; addr<eaddr; addr++)
		{
			if ((addr & mask) == ((tlb[i].VPN2 >> 12) & mask)) /* match */
			{
				vtlb_VMapUnmap(addr << 12, 0x1000);
				Cpu->Clear(addr << 12, 0x400);
			}
		}
	}
}

namespace R5900 {
namespace Interpreter {
namespace OpcodeImpl {
namespace COP0 {

void TLBR(void)
{
	int i = cpuRegs.CP0.n.Index & 0x3f;

	cpuRegs.CP0.n.PageMask = tlb[i].PageMask;
	cpuRegs.CP0.n.EntryHi  = tlb[i].EntryHi&~(tlb[i].PageMask|0x1f00);
	cpuRegs.CP0.n.EntryLo0 = (tlb[i].EntryLo0&~1)|((tlb[i].EntryHi>>12)&1);
	cpuRegs.CP0.n.EntryLo1 = (tlb[i].EntryLo1&~1)|((tlb[i].EntryHi>>12)&1);
}

void TLBWI(void)
{
	int j = cpuRegs.CP0.n.Index & 0x3f;

	UnmapTLB(j);

	tlb[j].PageMask = cpuRegs.CP0.n.PageMask;
	tlb[j].EntryHi  = cpuRegs.CP0.n.EntryHi;
	tlb[j].EntryLo0 = cpuRegs.CP0.n.EntryLo0;
	tlb[j].EntryLo1 = cpuRegs.CP0.n.EntryLo1;

	tlb[j].Mask     = (cpuRegs.CP0.n.PageMask >> 13) & 0xfff;
	tlb[j].nMask    = (~tlb[j].Mask) & 0xfff;
	tlb[j].VPN2     = ((cpuRegs.CP0.n.EntryHi >> 13) & (~tlb[j].Mask)) << 13;
	tlb[j].ASID     = cpuRegs.CP0.n.EntryHi & 0xfff;
	tlb[j].G        = cpuRegs.CP0.n.EntryLo0 & cpuRegs.CP0.n.EntryLo1 & 0x1;
	tlb[j].PFN0     = (((cpuRegs.CP0.n.EntryLo0 >> 6) & 0xFFFFF) & (~tlb[j].Mask)) << 12;
	tlb[j].PFN1     = (((cpuRegs.CP0.n.EntryLo1 >> 6) & 0xFFFFF) & (~tlb[j].Mask)) << 12;
	tlb[j].S        = cpuRegs.CP0.n.EntryLo0 & 0x80000000;

	MapTLB(j);
}

void TLBWR(void)
{
	int j = cpuRegs.CP0.n.Random & 0x3f;

	UnmapTLB(j);

	tlb[j].PageMask = cpuRegs.CP0.n.PageMask;
	tlb[j].EntryHi  = cpuRegs.CP0.n.EntryHi;
	tlb[j].EntryLo0 = cpuRegs.CP0.n.EntryLo0;
	tlb[j].EntryLo1 = cpuRegs.CP0.n.EntryLo1;

	tlb[j].Mask     = (cpuRegs.CP0.n.PageMask >> 13) & 0xfff;
	tlb[j].nMask    = (~tlb[j].Mask) & 0xfff;
	tlb[j].VPN2     = ((cpuRegs.CP0.n.EntryHi >> 13) & (~tlb[j].Mask)) << 13;
	tlb[j].ASID     = cpuRegs.CP0.n.EntryHi & 0xfff;
	tlb[j].G        = cpuRegs.CP0.n.EntryLo0 & cpuRegs.CP0.n.EntryLo1 & 0x1;
	tlb[j].PFN0     = (((cpuRegs.CP0.n.EntryLo0 >> 6) & 0xFFFFF) & (~tlb[j].Mask)) << 12;
	tlb[j].PFN1     = (((cpuRegs.CP0.n.EntryLo1 >> 6) & 0xFFFFF) & (~tlb[j].Mask)) << 12;
	tlb[j].S        = cpuRegs.CP0.n.EntryLo0 & 0x80000000;

	MapTLB(j);
}

void TLBP(void)
{
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
	for(i=0;i<48;i++)
	{
		if (tlb[i].VPN2 == ((~tlb[i].Mask) & (EntryHi32.s.VPN2))
		&& ((tlb[i].G&1) || ((tlb[i].ASID & 0xff) == EntryHi32.s.ASID)))
		{
			cpuRegs.CP0.n.Index = i;
			break;
		}
	}

	if(cpuRegs.CP0.n.Index == 0xFFFFFFFF)
		cpuRegs.CP0.n.Index = 0x80000000;
}

void MFC0(void)
{
	// Note on _Rd_ Condition 9: CP0.Count should be updated even if _Rt_ is 0.
	if ((_Rd_ != 9) && !_Rt_ )
		return;

	switch (_Rd_)
	{
		case 12:
			cpuRegs.GPR.r[_Rt_].SD[0] = (s32)(cpuRegs.CP0.r[_Rd_] & 0xf0c79c1f);
			break;

		case 25:
			if (0 == (_Imm_ & 1)) // MFPS, register value ignored
			{
				cpuRegs.GPR.r[_Rt_].SD[0] = (s32)cpuRegs.PERF.n.pccr.val;
				break;
			}

			COP0_UpdatePCCR();
			if (0 == (_Imm_ & 2)) // MFPC 0, only LSB of register matters
				cpuRegs.GPR.r[_Rt_].SD[0] = (s32)cpuRegs.PERF.n.pcr0;
			else // MFPC 1
				cpuRegs.GPR.r[_Rt_].SD[0] = (s32)cpuRegs.PERF.n.pcr1;
			break;

		case 24:
			break;

		case 9:
		{
			u32 incr = cpuRegs.cycle - cpuRegs.lastCOP0Cycle;
			if( incr == 0 ) incr++;
			cpuRegs.CP0.n.Count  += incr;
			cpuRegs.lastCOP0Cycle = cpuRegs.cycle;
			if( !_Rt_ )
				break;
		}

		default:
			cpuRegs.GPR.r[_Rt_].SD[0] = (s32)cpuRegs.CP0.r[_Rd_];
			break;
	}
}

void MTC0(void)
{
	switch (_Rd_)
	{
		case 9:
			cpuRegs.lastCOP0Cycle = cpuRegs.cycle;
			cpuRegs.CP0.r[9]      = cpuRegs.GPR.r[_Rt_].UL[0];
			break;

		case 12:
			WriteCP0Status(cpuRegs.GPR.r[_Rt_].UL[0]);
			break;

		case 24:
			break;

		case 25:
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
				cpuRegs.lastPERFCycle[0] = cpuRegs.cycle;
			}
			else // MTPC 1
			{
				cpuRegs.PERF.n.pcr1 = cpuRegs.GPR.r[_Rt_].UL[0];
				cpuRegs.lastPERFCycle[1] = cpuRegs.cycle;
			}
			break;

		default:
			cpuRegs.CP0.r[_Rd_] = cpuRegs.GPR.r[_Rt_].UL[0];
			break;
	}
}

#define CPCOND0() (((dmacRegs.stat.CIS | ~dmacRegs.pcr.CPC) & 0x3FF) == 0x3ff)

void BC0F(void)
{
	if (CPCOND0() == 0) intDoBranch(_BranchTarget_);
}

void BC0T(void)
{
	if (CPCOND0() == 1) intDoBranch(_BranchTarget_);
}

void BC0FL(void)
{
	if (CPCOND0() == 0)
		intDoBranch(_BranchTarget_);
	else
		cpuRegs.pc += 4;

}

void BC0TL(void)
{
	if (CPCOND0() == 1)
		intDoBranch(_BranchTarget_);
	else
		cpuRegs.pc += 4;
}

void ERET(void)
{
	if (cpuRegs.CP0.n.Status.b.ERL)
	{
		cpuRegs.pc                 = cpuRegs.CP0.n.ErrorEPC;
		cpuRegs.CP0.n.Status.b.ERL = 0;
	}
	else
	{
		cpuRegs.pc                 = cpuRegs.CP0.n.EPC;
		cpuRegs.CP0.n.Status.b.EXL = 0;
	}
	cpuSetNextEventDelta(4);
}

void DI(void)
{
	// IRQs are disabled so no need to do a cpu exception/event test...
	if (       cpuRegs.CP0.n.Status.b._EDI
		|| cpuRegs.CP0.n.Status.b.EXL
		|| cpuRegs.CP0.n.Status.b.ERL
		|| (cpuRegs.CP0.n.Status.b.KSU == 0))
		cpuRegs.CP0.n.Status.b.EIE = 0;
}

void EI(void)
{
	if (       cpuRegs.CP0.n.Status.b._EDI 
		|| cpuRegs.CP0.n.Status.b.EXL 
		|| cpuRegs.CP0.n.Status.b.ERL
		|| (cpuRegs.CP0.n.Status.b.KSU == 0))
	{
		cpuRegs.CP0.n.Status.b.EIE = 1;
		// schedule an event test, which will check for and raise pending IRQs.
		cpuSetNextEventDelta(4);
	}
}

} } } }	// end namespace R5900::Interpreter::OpcodeImpl
