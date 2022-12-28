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

#pragma once

//------------------------------------------------------------------
// Program Range Checking and Setting up Ranges
//------------------------------------------------------------------

// Used by mVUsetupRange
static __fi void mVUcheckIsSame(mV)
{
	if (mVU.prog.isSame == -1)
		mVU.prog.isSame = !memcmp((u8*)mVUcurProg.data, mVU.regs().Micro, mVU.microMemSize);
	if (mVU.prog.isSame == 0)
	{
		mVUcacheProg(mVU, *mVU.prog.cur);
		mVU.prog.isSame = 1;
	}
}

// Sets up microProgram PC ranges based on whats been recompiled
static void mVUsetupRange(microVU& mVU, s32 pc, bool isStartPC)
{
	std::deque<microRange>*& ranges = mVUcurProg.ranges;

	if (isStartPC) { // Check if startPC is already within a block we've recompiled
		std::deque<microRange>::const_iterator it(ranges->begin());
		for ( ; it != ranges->end(); ++it) {
			if ((pc >= it[0].start) && (pc <= it[0].end)) {
				if (it[0].start != it[0].end)
				{
					microRange mRange = { it[0].start, it[0].end };
					ranges->erase(it);
					ranges->push_front(mRange);
					return; // new start PC is inside the range of another range
				}
			}
		}
	}
	// existing range covers more area than current PC so no need to process it
	else if (mVUrange.end >= pc)
		return;

	mVUcheckIsSame(mVU);

	if (isStartPC) {
		microRange mRange = {pc, -1};
		ranges->push_front(mRange);
		return;
	}
	if (mVUrange.start <= pc)
	{
		mVUrange.end     = pc;
		bool mergedRange = false;
		s32  rStart      = mVUrange.start;
		s32  rEnd        = mVUrange.end;
		std::deque<microRange>::iterator it(ranges->begin());
		for (++it; it != ranges->end(); ++it) {
			if((it[0].start >= rStart) && (it[0].start <= rEnd)) { // Starts after this prog but starts before the end of current prog
				it[0].start = std::min(it[0].start, rStart); // Choose the earlier start
				mergedRange = true;
			}
			// Make sure we check both as the start on the other one may be later, we don't want to delete that
			if ((it[0].end >= rStart) && (it[0].end <= rEnd)) { // Ends after this prog starts but ends before this one ends
				it[0].end   = std::max(it[0].end, rEnd); // Extend the end of this prog to match this program
				mergedRange = true;
			}
		}
		if (mergedRange)
			ranges->erase(ranges->begin());
	}
	else
	{
		mVUrange.end = mVU.microMemSize;
		microRange mRange = {0, pc};
		ranges->push_front(mRange);
	}
}

//------------------------------------------------------------------
// Execute VU Opcode/Instruction (Upper and Lower)
//------------------------------------------------------------------

static __ri void doUpperOp(mV) { mVUopU(mVU, 1); mVUdivSet(mVU); }
static __ri void doLowerOp(mV) { incPC(-1); mVUopL(mVU, 1); incPC(1); }
static __ri void flushRegs(mV) { if (!doRegAlloc) mVU.regAlloc->flushAll(); }

static void doIbit(mV)
{ 
	if (mVUup.iBit)
	{ 
		incPC(-1);
		mVU.regAlloc->clearRegVF(33);
		if (EmuConfig.Gamefixes.IbitHack) {
			xMOV(gprT1, ptr32[&curI]);
			xMOV(ptr32[&mVU.getVI(REG_I)], gprT1);
		}
		else
		{
			u32 tempI;
			if (CHECK_VU_OVERFLOW && ((curI & 0x7fffffff) >= 0x7f800000))
				tempI = (0x80000000 & curI) | 0x7f7fffff; // Clamp I Reg
			else
				tempI = curI;

			xMOV(ptr32[&mVU.getVI(REG_I)], tempI);
		}
		incPC(1);
	} 
}

static void doSwapOp(mV)
{
	if (mVUinfo.backupVF && !mVUlow.noWriteVF)
	{
		// Allocate t1 first for better chance of reg-alloc
		const xmm& t1 = mVU.regAlloc->allocReg(mVUlow.VF_write.reg);
		const xmm& t2 = mVU.regAlloc->allocReg();
		xMOVAPS(t2, t1); // Backup VF reg
		mVU.regAlloc->clearNeeded(t1);

		mVUopL(mVU, 1);

		const xmm& t3 = mVU.regAlloc->allocReg(mVUlow.VF_write.reg, mVUlow.VF_write.reg, 0xf, 0);
		xXOR.PS(t2, t3); // Swap new and old values of the register
		xXOR.PS(t3, t2); // Uses xor swap trick...
		xXOR.PS(t2, t3);
		mVU.regAlloc->clearNeeded(t3);

		incPC(1); 
		doUpperOp(mVU);

		const xmm& t4 = mVU.regAlloc->allocReg(-1, mVUlow.VF_write.reg, 0xf);
		xMOVAPS(t4, t2);
		mVU.regAlloc->clearNeeded(t4);
		mVU.regAlloc->clearNeeded(t2);
	}
	else
	{
		mVUopL(mVU, 1);
		incPC(1);
		flushRegs(mVU);
		doUpperOp(mVU);
	}
}

static void mVUexecuteInstruction(mV)
{
	if (mVUlow.isNOP)
	{
		incPC(1);
		doUpperOp(mVU);
		flushRegs(mVU);
		doIbit(mVU);
	}
	else if (!mVUinfo.swapOps)
	{
		incPC(1);
		doUpperOp(mVU);
		flushRegs(mVU);
		doLowerOp(mVU);
	}
	else
		doSwapOp(mVU);

	flushRegs(mVU);
}

//------------------------------------------------------------------
// Warnings / Errors / Illegal Instructions
//------------------------------------------------------------------

// If 1st op in block is a bad opcode, then don't compile rest of block (Dawn of Mana Level 2)
static __fi void mVUcheckBadOp(mV)
{
	// The BIOS writes upper and lower NOPs in reversed slots (bug)
	//So to prevent spamming we ignore these, however its possible the real VU will bomb out if 
	//this happens, so we will bomb out without warning.
	if (mVUinfo.isBadOp && mVU.code != 0x8000033c)
		mVUinfo.isEOB = true;
}

static __ri void branchWarning(mV)
{
	incPC(-2);
	incPC(2);
	if (mVUup.eBit && mVUbranch)
		mVUlow.isNOP = true;

	if (mVUinfo.isBdelay && !mVUlow.evilBranch) { // Check if VI Reg Written to on Branch Delay Slot Instruction
		if (mVUlow.VI_write.reg && mVUlow.VI_write.used && !mVUlow.readFlags) {
			mVUlow.backupVI = true;
			mVUregs.viBackUp = mVUlow.VI_write.reg;
		}
	}
}

static __fi void eBitPass1(mV, int& branch)
{
	if (mVUregs.blockType != 1)
	{
		branch     = 1; 
		mVUup.eBit = true;
	}
}

static __ri void eBitWarning(mV)
{
	incPC(2);
	if (curI & _Ebit_)
		mVUregs.blockType = 1;
	incPC(-2);
}

//------------------------------------------------------------------
// Cycles / Pipeline State / Early Exit from Execution
//------------------------------------------------------------------
static __fi void optimizeReg(u8& rState)	 { rState = (rState==1) ? 0 : rState; }
static __fi void calcCycles(u8& reg, u8 x)	 { reg = ((reg > x) ? (reg - x) : 0); }
static __fi void tCycles(u8& dest, u8& src) { dest = std::max(dest, src); }
static __fi void incP(mV)					 { mVU.p ^= 1; }
static __fi void incQ(mV)					 { mVU.q ^= 1; }

// Optimizes the End Pipeline State Removing Unnecessary Info
// If the cycles remaining is just '1', we don't have to transfer it to the next block
// because mVU automatically decrements this number at the start of its loop,
// so essentially '1' will be the same as '0'...
static void mVUoptimizePipeState(mV)
{
	for (int i = 0; i < 32; i++) {
		optimizeReg(mVUregs.VF[i].x);
		optimizeReg(mVUregs.VF[i].y);
		optimizeReg(mVUregs.VF[i].z);
		optimizeReg(mVUregs.VF[i].w);
	}
	for (int i = 0; i < 16; i++)
		optimizeReg(mVUregs.VI[i]);
	if (mVUregs.q) { optimizeReg(mVUregs.q); if (!mVUregs.q) { incQ(mVU); } }
	if (mVUregs.p) { optimizeReg(mVUregs.p); if (!mVUregs.p) { incP(mVU); } }
	mVUregs.r = 0; // There are no stalls on the R-reg, so its Safe to discard info
}

void mVUincCycles(mV, int x)
{
	mVUcycles += x;
	// VF[0] is a constant value (0.0 0.0 0.0 1.0)
	for (int z = 31; z > 0; z--) {
		calcCycles(mVUregs.VF[z].x, x);
		calcCycles(mVUregs.VF[z].y, x);
		calcCycles(mVUregs.VF[z].z, x);
		calcCycles(mVUregs.VF[z].w, x);
	}
	// VI[0] is a constant value (0)
	for (int z = 15; z > 0; z--)
		calcCycles(mVUregs.VI[z], x);
	if (mVUregs.q) {
		if (mVUregs.q > 4) { calcCycles(mVUregs.q, x); if (mVUregs.q <= 4) { mVUinfo.doDivFlag = 1; } }
		else			   { calcCycles(mVUregs.q, x); }
		if (!mVUregs.q)    { incQ(mVU); }
	}
	if (mVUregs.p) {
		calcCycles(mVUregs.p, x);
		if (!mVUregs.p || mVUregsTemp.p) { incP(mVU); }
	}
	if (mVUregs.xgkick) {
		calcCycles(mVUregs.xgkick, x);
		if (!mVUregs.xgkick) { mVUinfo.doXGKICK = 1; mVUinfo.XGKICKPC = xPC;}
	}
	calcCycles(mVUregs.r, x);
}

// Helps check if upper/lower ops read/write to same regs...
static void cmpVFregs(microVFreg& VFreg1, microVFreg& VFreg2, bool& xVar)
{
	if (VFreg1.reg == VFreg2.reg) {
		if ((VFreg1.x && VFreg2.x) || (VFreg1.y && VFreg2.y)
		||	(VFreg1.z && VFreg2.z) || (VFreg1.w && VFreg2.w))
		{ xVar = 1; }
	}
}

static void mVUsetCycles(mV)
{
	mVUincCycles(mVU, mVUstall);
	// If upper Op && lower Op write to same VF reg:
	if ((mVUregsTemp.VFreg[0] == mVUregsTemp.VFreg[1]) && mVUregsTemp.VFreg[0]) {
		if (mVUregsTemp.r || mVUregsTemp.VI)
			mVUlow.noWriteVF = true; 
		else
			mVUlow.isNOP = true; // If lower Op doesn't modify anything else, then make it a NOP
	}
	// If lower op reads a VF reg that upper Op writes to:
	if ((mVUlow.VF_read[0].reg || mVUlow.VF_read[1].reg) && mVUup.VF_write.reg) {
		cmpVFregs(mVUup.VF_write, mVUlow.VF_read[0], mVUinfo.swapOps);
		cmpVFregs(mVUup.VF_write, mVUlow.VF_read[1], mVUinfo.swapOps);
	}
	// If above case is true, and upper op reads a VF reg that lower Op Writes to:
	if (mVUinfo.swapOps && ((mVUup.VF_read[0].reg || mVUup.VF_read[1].reg) && mVUlow.VF_write.reg)) {
		cmpVFregs(mVUlow.VF_write, mVUup.VF_read[0], mVUinfo.backupVF);
		cmpVFregs(mVUlow.VF_write, mVUup.VF_read[1], mVUinfo.backupVF);
	}

	tCycles(mVUregs.VF[mVUregsTemp.VFreg[0]].x, mVUregsTemp.VF[0].x);
	tCycles(mVUregs.VF[mVUregsTemp.VFreg[0]].y, mVUregsTemp.VF[0].y);
	tCycles(mVUregs.VF[mVUregsTemp.VFreg[0]].z, mVUregsTemp.VF[0].z);
	tCycles(mVUregs.VF[mVUregsTemp.VFreg[0]].w, mVUregsTemp.VF[0].w);

	tCycles(mVUregs.VF[mVUregsTemp.VFreg[1]].x, mVUregsTemp.VF[1].x);
	tCycles(mVUregs.VF[mVUregsTemp.VFreg[1]].y, mVUregsTemp.VF[1].y);
	tCycles(mVUregs.VF[mVUregsTemp.VFreg[1]].z, mVUregsTemp.VF[1].z);
	tCycles(mVUregs.VF[mVUregsTemp.VFreg[1]].w, mVUregsTemp.VF[1].w);

	tCycles(mVUregs.VI[mVUregsTemp.VIreg],	mVUregsTemp.VI);
	tCycles(mVUregs.q,						mVUregsTemp.q);
	tCycles(mVUregs.p,						mVUregsTemp.p);
	tCycles(mVUregs.r,						mVUregsTemp.r);
	tCycles(mVUregs.xgkick,					mVUregsTemp.xgkick);
}

// Saves Pipeline State for resuming from early exits
static __fi void mVUsavePipelineState(microVU& mVU)
{
	u32* lpS = (u32*)&mVU.prog.lpState;
	for(size_t i = 0; i < (sizeof(microRegInfo)-4)/4; i++, lpS++)
		xMOV(ptr32[lpS], lpS[0]);
}

// Test cycles to see if we need to exit-early...
static void mVUtestCycles(microVU& mVU, microFlagCycles& mFC)
{
	iPC = mVUstartPC;
	xMOV(eax, ptr32[&mVU.cycles]);
	if (!EmuConfig.Gamefixes.VUKickstartHack)
		xSUB(eax, mVUcycles); // Running behind, make sure we have time to run the block
	else
		xSUB(eax, 1); // Running ahead, make sure cycles left are above 0
	xCMP(eax, 0);
	xForwardJGE32 skip;
	mVUsavePipelineState(mVU);
	mVUendProgram(mVU, &mFC, 0);
	skip.SetTarget();
	xSUB(ptr32[&mVU.cycles], mVUcycles);
}

//------------------------------------------------------------------
// Initializing
//------------------------------------------------------------------

// This gets run at the start of every loop of mVU's first pass
static __fi void startLoop(mV)
{
	memzero(mVUinfo);
	memzero(mVUregsTemp);
}

// Initialize VI Constants (vi15 propagates through blocks)
static __fi void mVUinitConstValues(microVU& mVU)
{
	for (int i = 0; i < 16; i++) {
		mVUconstReg[i].isValid	= 0;
		mVUconstReg[i].regValue	= 0;
	}
	mVUconstReg[15].isValid  = mVUregs.vi15v;
	mVUconstReg[15].regValue = mVUregs.vi15v ? mVUregs.vi15 : 0;
}

// Initialize Variables
static __fi void mVUinitFirstPass(microVU& mVU, uptr pState, u8* thisPtr)
{
	mVUstartPC				= iPC;	// Block Start PC
	mVUbranch				= 0;	// Branch Type
	mVUcount				= 0;	// Number of instructions ran
	mVUcycles				= 0;	// Skips "M" phase, and starts counting cycles at "T" stage
	mVU.p					= 0;	// All blocks start at p index #0
	mVU.q					= 0;	// All blocks start at q index #0
	if ((uptr)&mVUregs != pState)	// Loads up Pipeline State Info
		memcpy((u8*)&mVUregs, (u8*)pState, sizeof(microRegInfo));
	if (((uptr)&mVU.prog.lpState != pState))
		memcpy((u8*)&mVU.prog.lpState, (u8*)pState, sizeof(microRegInfo));
	mVUblock.x86ptrStart	= thisPtr;
	mVUpBlock				= mVUblocks[mVUstartPC/2]->add(&mVUblock); // Add this block to block manager
	mVUregs.needExactMatch	= (mVUpBlock->pState.blockType)?7:0; // ToDo: Fix 1-Op block flag linking (MGS2:Demo/Sly Cooper)
	mVUregs.blockType		= 0;
	mVUregs.viBackUp		= 0;
	mVUregs.flagInfo		= 0;
	mVUregs.mbitinblock             = false;
	mVUsFlagHack			= CHECK_VU_FLAGHACK;
	mVUinitConstValues(mVU);
}

//------------------------------------------------------------------
// Recompiler
//------------------------------------------------------------------

//This bastardized function is used when a linked branch is in a conditional delay slot. It's messy, it's horrible, but it works.
//Unfortunately linking the reg manually and using the normal evil block method seems to suck at this :/
//If this is removed, test Evil Dead: Fistful of Boomstick (hangs going ingame), Mark of Kri (collision detection)
//and Tony Hawks Project 8 (graphics are half missing, requires Negative rounding when working)
void* mVUcompileSingleInstruction(microVU& mVU, u32 startPC, uptr pState, microFlagCycles& mFC)
{
	u8*				thisPtr  = x86Ptr;
	
	// First Pass
	iPC = startPC / 4;
	
	mVUbranch = 0;
	incPC(1);
	startLoop(mVU);

	mVUincCycles(mVU, 1);
	mVUopU(mVU, 0);
	mVUcheckBadOp(mVU);
	if (curI & _Ebit_) 
		eBitPass1(mVU, g_branch);
	if (curI & _Dbit_) { mVUup.dBit = true; }
	if (curI & _Tbit_) { mVUup.tBit = true; }
	if (curI & _Mbit_)
		mVUup.mBit = true;
	if (curI & _Ibit_)  
	{ 
		mVUlow.isNOP = true;
		mVUup.iBit   = true;
	}
	else			    { incPC(-1); mVUopL(mVU, 0); incPC(1); }
	mVUsetCycles(mVU);
	mVUinfo.readQ  =  mVU.q;
	mVUinfo.writeQ = !mVU.q;
	mVUinfo.readP  =  mVU.p && isVU1;
	mVUinfo.writeP = !mVU.p && isVU1;
	mVUcount++;
	mVUsetFlagInfo(mVU);
	incPC(1);


	mVUsetFlags(mVU, mFC);           // Sets Up Flag instances
	mVUoptimizePipeState(mVU);       // Optimize the End Pipeline State for nicer Block Linking

	// Second Pass
	iPC = startPC / 4;
	setCode();

	if (mVUup.mBit) {
		xOR(ptr32[&mVU.regs().flags], VUFLAG_MFLAGSET);
	}
	mVUexecuteInstruction(mVU);

	mVUincCycles(mVU, 1); //Just incase the is XGKick
	if (mVUinfo.doXGKICK) {
		mVU_XGKICK_DELAY(mVU);
	}

	return thisPtr;
}

static void mVUDoDBit(microVU& mVU, microFlagCycles* mFC)
{
	xTEST(ptr32[&VU0.VI[REG_FBRST].UL], (isVU1 ? 0x400 : 0x4));
	xForwardJump32 eJMP(Jcc_Zero);
	xOR(ptr32[&VU0.VI[REG_VPU_STAT].UL], (isVU1 ? 0x200 : 0x2));
	xOR(ptr32[&mVU.regs().flags], VUFLAG_INTCINTERRUPT);
	incPC(1);
	mVUDTendProgram(mVU, mFC, 1);
	incPC(-1);
	eJMP.SetTarget();
}

static void mVUDoTBit(microVU& mVU, microFlagCycles* mFC)
{
	xTEST(ptr32[&VU0.VI[REG_FBRST].UL], (isVU1 ? 0x800 : 0x8));
	xForwardJump32 eJMP(Jcc_Zero);
	xOR(ptr32[&VU0.VI[REG_VPU_STAT].UL], (isVU1 ? 0x400 : 0x4));
	xOR(ptr32[&mVU.regs().flags], VUFLAG_INTCINTERRUPT);
	incPC(1);
	mVUDTendProgram(mVU, mFC, 1);
	incPC(-1);

	eJMP.SetTarget();
}

void *mVUcompile(microVU& mVU, u32 startPC, uptr pState)
{
	microFlagCycles mFC;
	u8* thisPtr = x86Ptr;
	const u32 endCount = (((microRegInfo*)pState)->blockType) ? 1 : (mVU.microMemSize / 8);

	// First Pass
	iPC = startPC / 4;
	mVUsetupRange(mVU, startPC, 1); // Setup Program Bounds/Range
	mVU.regAlloc->reset();          // Reset regAlloc
	mVUinitFirstPass(mVU, pState, thisPtr);
	mVUbranch = 0;
	for (int branch = 0; mVUcount < endCount;) {
		incPC(1);
		startLoop(mVU);
		mVUincCycles(mVU, 1);
		mVUopU(mVU, 0);
		mVUcheckBadOp(mVU);
		if (curI & _Ebit_) {
			eBitPass1(mVU, branch);
			// VU0 end of program MAC results can be read by COP2, so best to make sure the last instance is valid
			// Needed for State of Emergency 2 and Driving Emotion Type-S
			if (isVU0) mVUregs.needExactMatch |= 7;
		}

		if ((curI & _Mbit_) && isVU0) {
			mVUregs.mbitinblock = true;
			if (xPC > 0)
			{
				incPC(-2);
				incPC(2);
				if (!(curI & _Mbit_)) //If the last instruction was also M-Bit we don't need to sync again
					mVUup.mBit = true;
			}
			else
				mVUup.mBit = true;
		}

		if (curI & _Ibit_) {
			mVUlow.isNOP = true;
			mVUup.iBit = true;
			if (EmuConfig.Gamefixes.IbitHack) {
				mVUsetupRange(mVU, xPC, false);
				if (branch < 2)
					mVUsetupRange(mVU, xPC+8, true);  // Ideally we'd do +4 but the mmx compare only works in 64bits, this should be fine
			}
		}
		else {
			incPC(-1);
			mVUopL(mVU, 0);
			incPC(1);
		}
		if (curI & _Dbit_)
			mVUup.dBit = true;
		if (curI & _Tbit_)
			mVUup.tBit = true;
		mVUsetCycles(mVU);
		mVUinfo.readQ = mVU.q;
		mVUinfo.writeQ = !mVU.q;
		mVUinfo.readP = mVU.p && isVU1;
		mVUinfo.writeP = !mVU.p && isVU1;
		mVUcount++;

		if (branch >= 2) {
			mVUinfo.isEOB = true;

			if (branch == 3)
				mVUinfo.isBdelay = true;

			branchWarning(mVU);
			break;
		}
		else if (branch == 1)
			branch = 2;

		if (mVUbranch) {
			mVUsetFlagInfo(mVU);
			eBitWarning(mVU);
			branch = 3;
			mVUbranch = 0;
		}

		if (mVUup.mBit && !branch && !mVUup.eBit)
		{
			mVUregs.needExactMatch |= 7;
			break;
		}

		if (mVUinfo.isEOB)
			break;

		incPC(1);
	}

	// Fix up vi15 const info for propagation through blocks
	mVUregs.vi15 = (doConstProp && mVUconstReg[15].isValid) ? (u16)mVUconstReg[15].regValue : 0;
	mVUregs.vi15v = (doConstProp && mVUconstReg[15].isValid) ? 1 : 0;

	mVUsetFlags(mVU, mFC);           // Sets Up Flag instances
	mVUoptimizePipeState(mVU);       // Optimize the End Pipeline State for nicer Block Linking
	mVUtestCycles(mVU, mFC);              // Update VU Cycles and Exit Early if Necessary

	// Second Pass
	iPC = mVUstartPC;
	setCode();
	mVUbranch = 0;
	u32 x = 0;

	for (; x < endCount; x++) {
		if (mVUinfo.isEOB)
			x = 0xffff;
		if (mVUup.mBit) {
			xOR(ptr32[&mVU.regs().flags], VUFLAG_MFLAGSET);
		}
		mVUexecuteInstruction(mVU);
		if (!mVUinfo.isBdelay && !mVUlow.branch) //T/D Bit on branch is handled after the branch, branch delay slots are executed.
		{
			if (mVUup.tBit)
				mVUDoTBit(mVU, &mFC);
			else if (mVUup.dBit && doDBitHandling)
				mVUDoDBit(mVU, &mFC);
			else if (mVUup.mBit && !mVUup.eBit && !mVUinfo.isEOB) {
				// Need to make sure the flags are exact, Gungrave does FCAND with Mbit, then directly after FMAND with M-bit
				// Also call setupBranch to sort flag instances

				mVUsetupBranch(mVU, mFC);
				// Make sure we save the current state so it can come back to it
				u32* cpS = (u32*)&mVUregs;
				u32* lpS = (u32*)&mVU.prog.lpState;
				for (size_t i = 0; i < (sizeof(microRegInfo) - 4) / 4; i++, lpS++, cpS++) {
					xMOV(ptr32[lpS], cpS[0]);
				}
				incPC(2);
				mVUsetupRange(mVU, xPC, false);
				mVUendProgram(mVU, &mFC, 0);
				normBranchCompile(mVU, xPC);
				incPC(-2);
				return thisPtr;
			}
		}

		if (mVUinfo.doXGKICK) {
			mVU_XGKICK_DELAY(mVU);
		}

		if (isEvilBlock) {
			mVUsetupRange(mVU, xPC+8, false);
			normJumpCompile(mVU, mFC, true);
			return thisPtr;
		}
		else if (!mVUinfo.isBdelay) {
			// Handle range wrapping
			if ((xPC + 8) == mVU.microMemSize)
			{
				mVUsetupRange(mVU, xPC+8, false);
				mVUsetupRange(mVU, 0, 1);
			}
			incPC(1);
		}
		else {
			incPC(1);
			mVUsetupRange(mVU, xPC, false);
			incPC(-4); // Go back to branch opcode

			switch (mVUlow.branch) {
			case 1: // B/BAL
			case 2:
				normBranch(mVU, mFC);
				return thisPtr;
			case 9: // JR/JALR
			case 10:
				normJump(mVU, mFC);
				return thisPtr;
			case 3: // IBEQ
				condBranch(mVU, mFC, Jcc_Equal);
				return thisPtr;
			case 4: // IBGEZ
				condBranch(mVU, mFC, Jcc_GreaterOrEqual);
				return thisPtr;
			case 5: // IBGTZ
				condBranch(mVU, mFC, Jcc_Greater);
				return thisPtr;
			case 6: // IBLEQ
				condBranch(mVU, mFC, Jcc_LessOrEqual);
				return thisPtr;
			case 7: // IBLTZ
				condBranch(mVU, mFC, Jcc_Less);
				return thisPtr;
			case 8: // IBNEQ
				condBranch(mVU, mFC, Jcc_NotEqual);
				return thisPtr;
			}
		}
	}

	// E-bit End
	mVUsetupRange(mVU, xPC, false);
	mVUendProgram(mVU, &mFC, 1);

	return thisPtr;
}

// Returns the entry point of the block (compiles it if not found)
__fi void* mVUentryGet(microVU& mVU, microBlockManager* block, u32 startPC, uptr pState)
{
	microBlock* pBlock = block->search((microRegInfo*)pState);
	if (pBlock)
		return pBlock->x86ptrStart;
	return mVUcompile(mVU, startPC, pState);
}

 // Search for Existing Compiled Block (if found, return x86ptr; else, compile and return x86ptr)
__fi void* mVUblockFetch(microVU& mVU, u32 startPC, uptr pState)
{
	startPC &= mVU.microMemSize-8;

	blockCreate(startPC/8);
	return mVUentryGet(mVU, mVUblocks[startPC/8], startPC, pState);
}

// mVUcompileJIT() - Called By JR/JALR during execution
_mVUt void* mVUcompileJIT(u32 startPC, uptr ptr)
{
       if (doJumpAsSameProgram) { // Treat jump as part of same microProgram
               if (doJumpCaching) { // When doJumpCaching, ptr is a microBlock pointer
                       microVU& mVU = mVUx;
                       microBlock* pBlock = (microBlock*)ptr;
                       microJumpCache& jc = pBlock->jumpCache[startPC / 8];
                       if (jc.prog && jc.prog == mVU.prog.quick[startPC / 8].prog) return jc.x86ptrStart;
                       void* v = mVUblockFetch(mVUx, startPC, (uptr)&pBlock->pStateEnd);
                       jc.prog = mVU.prog.quick[startPC / 8].prog;
                       jc.x86ptrStart = v;
                       return v;
               }
               return mVUblockFetch(mVUx, startPC, ptr);
       }
       mVUx.regs().start_pc = startPC;
       if (doJumpCaching)
       {
	       // When doJumpCaching, ptr is a microBlock pointer
               microVU& mVU       = mVUx;
               microBlock* pBlock = (microBlock*)ptr;
               microJumpCache& jc = pBlock->jumpCache[startPC/8];
               if (jc.prog && jc.prog == mVU.prog.quick[startPC/8].prog)
		       return jc.x86ptrStart;
               void* v = mVUsearchProg<vuIndex>(startPC, (uptr)&pBlock->pStateEnd);
               jc.prog = mVU.prog.quick[startPC/8].prog;
               jc.x86ptrStart = v;
               return v;
       }
       // When !doJumpCaching, pBlock param is really a microRegInfo pointer
       return mVUsearchProg<vuIndex>(startPC, ptr); // Find and set correct program
}
