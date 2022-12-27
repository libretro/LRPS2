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
#include <deque>
#include "gui/SysThreads.h"
#include "Utilities/MemcpyFast.h"
#include "Gif.h"
#include "Vif.h"
#include "GS.h"

 // FIXME common path ?
#include "Utilities/boost_spsc_queue.hpp"

#if !defined(_M_SSE)
#if defined(__GNUC__)
#if defined(__SSE2__)
#define _M_SSE 0x200
#endif
#endif

#if !defined(_M_SSE) && (!defined(_WIN32) || defined(_M_AMD64) || defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define _M_SSE 0x200
#endif
#endif

struct GS_Packet;
extern void Gif_MTGS_Wait(bool isMTVU);
extern void Gif_FinishIRQ(void);
extern bool Gif_HandlerAD(u8* pMem);
extern bool Gif_HandlerAD_MTVU(u8* pMem);
extern void Gif_AddBlankGSPacket(u32 size, GIF_PATH path);
extern void Gif_AddGSPacketMTVU(GS_Packet& gsPack, GIF_PATH path);
extern void Gif_AddCompletedGSPacket(GS_Packet& gsPack, GIF_PATH path);

struct Gif_Tag
{
	struct HW_Gif_Tag
	{
		u16 NLOOP : 15;
		u16 EOP : 1;
		u16 _dummy0 : 16;
		u32 _dummy1 : 14;
		u32 PRE : 1;
		u32 PRIM : 11;
		u32 FLG : 2;
		u32 NREG : 4;
		u32 REGS[2];
	} tag;

	u32 nLoop;    // NLOOP left to process
	u32 nRegs;    // NREG (1~16)
	u32 nRegIdx;  // Current nReg Index (packed mode processing)
	u32 len;      // Packet Length in Bytes (not including tag)
	u32 cycles;   // Time needed to process packet data in ee-cycles
	u8 regs[16];  // Regs
	bool hasAD;   // Has an A+D Write
	bool isValid; // Tag is valid

	__ri Gif_Tag() { Reset(); }
	__ri Gif_Tag(u8* pMem, bool analyze = false)
	{
		setTag(pMem, analyze);
	}

	__ri void Reset() { memzero(*this); }
	__ri u8 curReg() { return regs[nRegIdx & 0xf]; }

	__ri void packedStep()
	{
		if (nLoop > 0)
		{
			nRegIdx++;
			if (nRegIdx >= nRegs)
			{
				nRegIdx = 0;
				nLoop--;
			}
		}
	}

	__ri void setTag(u8* pMem, bool analyze = false)
	{
		tag     = *(HW_Gif_Tag*)pMem;
		nLoop   = tag.NLOOP;
		hasAD   = false;
		nRegIdx = 0;
		isValid = 1;
		len     = 0; // avoid uninitialized compiler warning
		switch (tag.FLG)
		{
		case GIF_FLG_PACKED:
			nRegs = ((tag.NREG - 1) & 0xf) + 1;
			len = (nRegs * tag.NLOOP) * 16;
			cycles = len << 1; // Packed Mode takes 2 ee-cycles
			if (analyze)
				analyzeTag();
			break;
		case GIF_FLG_REGLIST:
			nRegs = ((tag.NREG - 1) & 0xf) + 1;
			len = ((nRegs * tag.NLOOP + 1) >> 1) * 16;
			cycles = len << 2; // Reg-list Mode takes 4 ee-cycles
			break;
		case GIF_FLG_IMAGE:
		case GIF_FLG_IMAGE2:
			nRegs = 0;
			len = tag.NLOOP * 16;
			cycles = len << 2; // Image Mode takes 4 ee-cycles
			tag.FLG = GIF_FLG_IMAGE;
			break;
		default:
			break;
		}
	}

	__ri void analyzeTag(void)
	{
#if _M_SSE >= 0x200
		// zero out bits for registers which shouldn't be tested
		__m128i vregs = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(tag.REGS));
		vregs = _mm_and_si128(vregs, _mm_srli_epi64(_mm_set1_epi32(0xFFFFFFFFu), (64 - nRegs * 4)));

		// get upper nibbles, interleave with lower nibbles, clear upper bits from low nibbles
		vregs = _mm_and_si128(_mm_unpacklo_epi8(vregs, _mm_srli_epi32(vregs, 4)), _mm_set1_epi8(0x0F));

		// compare with GIF_REG_A_D, set hasAD if any lanes passed
		hasAD = (_mm_movemask_epi8(_mm_cmpeq_epi8(vregs, _mm_set1_epi8(GIF_REG_A_D))) != 0);

		// write out unpacked registers
		_mm_storeu_si128(reinterpret_cast<__m128i*>(regs), vregs);
#else
		// Reference C implementation.
		hasAD = false;
		u32 t = tag.REGS[0];
		u32 i = 0;
		u32 j = std::min<u32>(nRegs, 8);
		for (; i < j; i++)
		{
			regs[i] = t & 0xf;
			hasAD |= (regs[i] == GIF_REG_A_D);
			t >>= 4;
		}
		t = tag.REGS[1];
		j = nRegs;
		for (; i < j; i++)
		{
			regs[i] = t & 0xf;
			hasAD |= (regs[i] == GIF_REG_A_D);
			t >>= 4;
		}
#endif
	}
};

struct GS_Packet
{
	// PERF note: this struct is copied various time in hot path. Don't add
	// new field

	u32 offset;     // Path buffer offset for start of packet
	u32 size;       // Full size of GS-Packet
	s32 cycles;     // EE Cycles taken to process this GS packet
	s32 readAmount; // Dummy read-amount data needed for proper buffer calculations
	GS_Packet() { Reset(); }
	void Reset() { memzero(*this); }
};

struct GS_SIGNAL
{
	u32 data[2];
	bool queued;
	void Reset() { memzero(*this); }
};

struct GS_FINISH
{
	bool gsFINISHFired;

	void Reset() { memzero(*this); }
};

static __fi void incTag(u32& offset, u32& size, u32 incAmount)
{
	size += incAmount;
	offset += incAmount;
}

struct Gif_Path_MTVU
{
	u32 fakePackets; // Fake packets pending to be sent to MTGS
	GS_Packet fakePacket;
	// Set a size based on MTGS but keep a factor 2 to avoid too waste to much
	// memory overhead. Note the struct is instantied 3 times (for each gif
	// path)
	ringbuffer_base<GS_Packet, RingBufferSize / 2> gsPackQueue;
	Gif_Path_MTVU() { Reset(); }
	void Reset()
	{
		fakePackets = 0;
		gsPackQueue.reset();
		fakePacket.Reset();
		fakePacket.size = ~0u; // Used to indicate that its a fake packet
	}
};

struct Gif_Path
{
	std::atomic<int> readAmount; // Amount of data MTGS still needs to read
	u8* buffer;                  // Path packet buffer
	u32 buffSize;                // Full size of buffer
	u32 buffLimit;               // Cut off limit to wrap around
	u32 curSize;                 // Used buffer in bytes
	u32 curOffset;               // Offset of current gifTag
	u32 dmaRewind;               // Used by path3 when only part of a DMA chain is used
	Gif_Tag gifTag;              // Current GS Primitive tag
	GS_Packet gsPack;            // Current GS Packet info
	GIF_PATH idx;                // Gif Path Index
	GIF_PATH_STATE state;        // Path State
	Gif_Path_MTVU mtvu;          // Must be last for saved states

	Gif_Path() { Reset(); }
	~Gif_Path() { _aligned_free(buffer); }

	void Init(GIF_PATH _idx, u32 _buffSize, u32 _buffSafeZone)
	{
		idx = _idx;
		buffSize = _buffSize;
		buffLimit = _buffSize - _buffSafeZone;
		buffer = (u8*)_aligned_malloc(buffSize, 16);
		Reset();
	}

	void Reset(bool softReset = false)
	{
		state = GIF_PATH_IDLE;
		if (softReset)
		{
			if (!isMTVU()) // MTVU Freaks out if you try to reset it, so let's just let it transfer
				curSize = curOffset;
			return;
		}
		mtvu.Reset();
		curSize = 0;
		curOffset = 0;
		readAmount = 0;
		gifTag.Reset();
		gsPack.Reset();
	}

	bool isMTVU() const { return !idx && THREAD_VU1; }
	s32 getReadAmount() { return readAmount.load(std::memory_order_acquire) + gsPack.readAmount; }
	bool hasDataRemaining() const { return curOffset < curSize; }
	bool isDone() const { return isMTVU() ? !mtvu.fakePackets : (!hasDataRemaining() && (state == GIF_PATH_IDLE || state == GIF_PATH_WAIT)); }

	// Moves packet data to start of buffer
	void RealignPacket()
	{
		s32 offset    = curOffset - gsPack.size;
		s32 sizeToAdd = curSize - offset;
		s32 intersect = sizeToAdd - offset;
		if (intersect < 0)
			intersect = 0;
		for (;;)
		{
			s32 frontFree = offset - getReadAmount();
			if (frontFree >= sizeToAdd - intersect)
				break;
			Gif_MTGS_Wait(isMTVU());
		}
		if (offset < (s32)buffLimit)
		{ // Needed for correct readAmount values
			if (isMTVU())
				gsPack.readAmount += buffLimit - offset;
			else
				Gif_AddBlankGSPacket(buffLimit - offset, idx);
		}
		if (intersect)
			memmove(buffer, &buffer[offset], curSize - offset);
		else
			memcpy(buffer, &buffer[offset], curSize - offset);
		curSize -= offset;
		curOffset = gsPack.size;
		gsPack.offset = 0;
	}

	void CopyGSPacketData(u8* pMem, u32 size, bool aligned = false)
	{
		// Move gsPack to front of buffer
		if (curSize + size > buffSize)
			RealignPacket();
		for (;;)
		{
			s32 offset  = curOffset - gsPack.size;
			s32 readPos = offset - getReadAmount();
			if (readPos >= 0)
				break; // MTGS is reading in back of curOffset
			if ((s32)buffLimit + readPos > (s32)curSize + (s32)size)
				break;      // Enough free front space
			Gif_MTGS_Wait(isMTVU()); // Let MTGS run to free up buffer space
		}
		memcpy(&buffer[curSize], pMem, size);
		curSize += size;
	}

	// If completed a GS packet (with EOP) then set done to true
	// MTVU: This function only should be called called on EE thread
	GS_Packet ExecuteGSPacket(bool& done)
	{
		if (mtvu.fakePackets)
		{ // For MTVU mode...
			mtvu.fakePackets--;
			done = true;
			return mtvu.fakePacket;
		}
		for (;;)
		{
			if (!gifTag.isValid)
			{ // Need new Gif Tag
				// We don't have enough data for a Gif Tag
				if (curOffset + 16 > curSize)
					return gsPack;

				// Move packet to start of buffer
				if (curOffset > buffLimit)
					RealignPacket();

				gifTag.setTag(&buffer[curOffset], 1);

				state = (GIF_PATH_STATE)(gifTag.tag.FLG + 1);

				// We don't have enough data for a complete GS packet
				if (!gifTag.hasAD && curOffset + 16 + gifTag.len > curSize)
				{
					gifTag.isValid = false; // So next time we test again
					return gsPack;
				}

				incTag(curOffset, gsPack.size, 16); // Tag Size
				gsPack.cycles += 2 + gifTag.cycles; // Tag + Len ee-cycles
			}

			if (gifTag.hasAD)
			{ // Only can be true if GIF_FLG_PACKED
				bool dblSIGNAL = false;
				while (gifTag.nLoop && !dblSIGNAL)
				{
					if (curOffset + 16 > curSize)
						return gsPack; // Exit Early
					if (gifTag.curReg() == GIF_REG_A_D)
					{
						if (!isMTVU())
							dblSIGNAL = Gif_HandlerAD(&buffer[curOffset]);
					}
					incTag(curOffset, gsPack.size, 16); // 1 QWC
					gifTag.packedStep();
				}
				if (dblSIGNAL && !(gifTag.tag.EOP && !gifTag.nLoop))
					return gsPack; // Exit Early
			} 			
			else
				incTag(curOffset, gsPack.size, gifTag.len); // Data length

			// Reload gif tag next loop
			gifTag.isValid = false;

			if (gifTag.tag.EOP)
			{
				GS_Packet t = gsPack;
				done = true;

				dmaRewind = 0;

				gsPack.Reset();
				gsPack.offset = curOffset;

				//Path 3 Masking is timing sensitive, we need to simulate its length! (NFSU2/Outrun 2006)

				if ((gifRegs.stat.APATH - 1) == GIF_PATH_3)
				{
					state = GIF_PATH_WAIT;

					if (curSize - curOffset > 0 && (gifRegs.stat.M3R || gifRegs.stat.M3P))
					{
						//Including breaking packets early (Rewind DMA to pick up where left off)
						//but only do this when the path is masked, else we're pointlessly slowing things down.
						dmaRewind = curSize - curOffset;
						curSize = curOffset;
					}
				} 				
				else
					state = GIF_PATH_IDLE;

				return t; // Complete GS packet
			}
		}
	}

	// MTVU: Gets called on VU XGkicks on MTVU thread
	void ExecuteGSPacketMTVU(void)
	{
		// Move packet to start of buffer
		if (curOffset > buffLimit)
			RealignPacket();
		for (;;)
		{ // needed to be processed by pcsx2...
			if (curOffset + 16 > curSize)
				break;
			gifTag.setTag(&buffer[curOffset], 1);

			if (!gifTag.hasAD && curOffset + 16 + gifTag.len > curSize)
				break;
			incTag(curOffset, gsPack.size, 16); // Tag Size

			if (gifTag.hasAD)
			{ // Only can be true if GIF_FLG_PACKED
				while (gifTag.nLoop)
				{
					if (curOffset + 16 > curSize)
						break; // Exit Early
					incTag(curOffset, gsPack.size, 16); // 1 QWC
					gifTag.packedStep();
				}
			} 			
			else
				incTag(curOffset, gsPack.size, gifTag.len); // Data length
			if (curOffset >= curSize)
				break;
			if (gifTag.tag.EOP)
				break;
		}
		gifTag.isValid = false;
	}

	// MTVU: Gets called after VU1 execution on MTVU thread
	void FinishGSPacketMTVU()
	{
		// Performance note: fetch_add atomic operation might create some stall for atomic
		// operation in gsPack.push
		readAmount.fetch_add(gsPack.size + gsPack.readAmount, std::memory_order_acq_rel);
		while (!mtvu.gsPackQueue.push(gsPack))
			;

		gsPack.Reset();
		gsPack.offset = curOffset;
	}

	// MTVU: Gets called by MTGS thread
	GS_Packet GetGSPacketMTVU()
	{
		// FIXME is the error path useful ?
		if (!mtvu.gsPackQueue.empty())
			return mtvu.gsPackQueue.front();
		return GS_Packet(); // gsPack.size will be 0
	}
};

struct Gif_Unit
{
	Gif_Path gifPath[3];
	GS_SIGNAL gsSIGNAL; // Stalling Signal
	GS_FINISH gsFINISH; // Finish Signal
	tGIF_STAT& stat;
	GIF_TRANSFER_TYPE lastTranType; // Last Transfer Type

	Gif_Unit()
		: gsSIGNAL()
		, gsFINISH()
		, stat(gifRegs.stat)
		, lastTranType(GIF_TRANS_INVALID)
	{
		gifPath[0].Init(GIF_PATH_1, _1mb * 9, _1mb + _1kb);
		gifPath[1].Init(GIF_PATH_2, _1mb * 9, _1mb + _1kb);
		gifPath[2].Init(GIF_PATH_3, _1mb * 9, _1mb + _1kb);
	}

	// Enable softReset when resetting during game emulation
	void Reset(bool softReset = false)
	{
		ResetRegs();
		gsSIGNAL.Reset();
		gsFINISH.Reset();
		gifPath[0].Reset(softReset);
		gifPath[1].Reset(softReset);
		gifPath[2].Reset(softReset);
		if (!softReset)
		{
			lastTranType = GIF_TRANS_INVALID;
		}
		//If the VIF has paused waiting for PATH3, recheck it after the reset has occurred (Eragon)
		if (vif1Regs.stat.VGW)
		{
			if (!(cpuRegs.interrupt & (1 << DMAC_VIF1)))
				CPU_INT(DMAC_VIF1, 1);
		}
	}

	// Resets Gif HW Regs
	void ResetRegs()
	{
		gifRegs.stat._u32 = 0;
		gifRegs.ctrl._u32 = 0;
		gifRegs.mode._u32 = 0;
		gif_fifo.init();
	}

	// Adds a finished GS Packet to the MTGS ring buffer
	__fi void AddCompletedGSPacket(GS_Packet& gsPack, GIF_PATH path)
	{
		if (gsPack.size == ~0u)
			Gif_AddGSPacketMTVU(gsPack, path);
		else
			Gif_AddCompletedGSPacket(gsPack, path);
	}

	// Returns GS Packet Size in bytes
	u32 GetGSPacketSize(GIF_PATH pathIdx, u8* pMem, u32 offset = 0, u32 size = ~0u)
	{
		u32 memMask = pathIdx ? ~0u : 0x3fffu;
		u32 curSize = 0;
		for (;;)
		{
			Gif_Tag gifTag(&pMem[offset & memMask]);
			incTag(offset, curSize, 16 + gifTag.len); // Tag + Data length
			if (pathIdx == GIF_PATH_1 && curSize >= 0x4000)
				return 0; // Bios does this... (Fixed if you delay vu1's xgkick by 103 vu cycles)
			if (curSize >= size)
				return size;
			if(!EmuConfig.Cpu.Recompiler.EnableVU1 && pathIdx == GIF_PATH_1)
				return curSize | ((u32)gifTag.tag.EOP << 31);
			if (gifTag.tag.EOP)
				return curSize;
		}
	}

	// Specify the transfer type you are initiating
	// The return value is the amount of data (in bytes) that was processed
	// If transfer cannot take place at this moment the return value is 0
	u32 TransferGSPacketData(GIF_TRANSFER_TYPE tranType, u8* pMem, u32 size, bool aligned = false)
	{

		if (THREAD_VU1)
		{
			Gif_Path& path1 = gifPath[GIF_PATH_1];
			if (tranType == GIF_TRANS_XGKICK)
			{ // This is on the MTVU thread
				path1.CopyGSPacketData(pMem, size, aligned);
				path1.ExecuteGSPacketMTVU();
				return size;
			}
			if (tranType == GIF_TRANS_MTVU)
			{ // This is on the EE thread
				path1.mtvu.fakePackets++;
				if (CanDoGif())
					Execute(false, true);
				return 0;
			}
		}

		if (size == 0)
			return 0;
		lastTranType = tranType;

		if (tranType == GIF_TRANS_DMA)
		{
			if (!CanDoPath3())
			{
				if (!Path3Masked())
					stat.P3Q = 1;
				return 0;
			} // DMA Stall
		}
		if (tranType == GIF_TRANS_XGKICK)
		{
			// We always buffer path1 packets
			if (!CanDoPath1())
				stat.P1Q = 1;
		}
		if (tranType == GIF_TRANS_DIRECT)
		{
			if (!CanDoPath2())
			{
				stat.P2Q = 1;
				return 0;
			} // Direct Stall
		}
		if (tranType == GIF_TRANS_DIRECTHL)
		{
			if (!CanDoPath2HL())
			{
				stat.P2Q = 1;
				return 0;
			} // DirectHL Stall
		}

		gifPath[tranType & 3].CopyGSPacketData(pMem, size, aligned);
		size -= Execute(tranType == GIF_TRANS_DMA, false);
		return size;
	}

	// Checks path activity for the given paths
	// Returns an int with a bit enabled if the corresponding
	// path is not finished (needs more data/processing for an EOP)
	__fi int checkPaths(bool p1, bool p2, bool p3, bool checkQ = false)
	{
		int ret = 0;
		ret |= (p1 && !gifPath[GIF_PATH_1].isDone()) << 0;
		ret |= (p2 && !gifPath[GIF_PATH_2].isDone()) << 1;
		ret |= (p3 && !gifPath[GIF_PATH_3].isDone()) << 2;
		return ret | (checkQ ? checkQueued(p1, p2, p3) : 0);
	}

	__fi int checkQueued(bool p1, bool p2, bool p3)
	{
		int ret = 0;
		ret |= (p1 && stat.P1Q) << 0;
		ret |= (p2 && stat.P2Q) << 1;
		ret |= (p3 && stat.P3Q) << 2;
		return ret;
	}

	// Send processed GS Primitive(s) to the MTGS thread
	// Note: Only does so if current path fully completed all
	// of its given gs primitives (but didn't upload them yet)
	void FlushToMTGS()
	{
		if (!stat.APATH)
			return;
		Gif_Path& path = gifPath[stat.APATH - 1];
		if (path.gsPack.size && !path.gifTag.isValid)
		{
			AddCompletedGSPacket(path.gsPack, (GIF_PATH)(stat.APATH - 1));
			path.gsPack.offset = path.curOffset;
			path.gsPack.size = 0;
		}
	}

	// Processes gif packets and performs path arbitration
	// on EOPs or on Path 3 Images when IMT is set.
	int Execute(bool isPath3, bool isResume)
	{
		if (!CanDoGif())
			return 0;
		bool didPath3 = false;
		int curPath = stat.APATH > 0 ? stat.APATH - 1 : 0; //Init to zero if no path is already set.
		gifPath[2].dmaRewind = 0;
		stat.OPH = 1;

		for (;;)
		{
			if (stat.APATH)
			{ // Some Transfer is happening
				Gif_Path& path = gifPath[stat.APATH - 1];
				bool done = false;
				GS_Packet gsPack = path.ExecuteGSPacket(done);
				if (!done)
				{
					if (stat.APATH == 3 && CanDoP3Slice() && !gsSIGNAL.queued)
					{
						if (!didPath3 && /*!Path3Masked() &&*/ checkPaths(1, 1, 0))
						{ // Path3 slicing
							didPath3 = true;
							stat.APATH = 0;
							stat.IP3 = 1;
							if (gsPack.size > 16)
							{                                                 // Packet had other tags which we already processed
								u32 subOffset = path.gifTag.isValid ? 16 : 0; // if isValid, image-primitive not finished
								gsPack.size -= subOffset;                     // Remove the image-tag (should be last thing read)
								AddCompletedGSPacket(gsPack, GIF_PATH_3);     // Consider current packet complete
								path.gsPack.Reset();                          // Reset gs packet info
								path.curOffset -= subOffset;                  // Start the next GS packet at the image-tag
								path.gsPack.offset = path.curOffset;          // Set to image-tag
								path.gifTag.isValid = false;                  // Reload tag next ExecuteGSPacket()
							}
							continue;
						}
					}
					//FlushToMTGS();
					break; // Not finished with GS packet
				}
				if (gifPath[curPath].state == GIF_PATH_WAIT || gifPath[curPath].state == GIF_PATH_IDLE)
					AddCompletedGSPacket(gsPack, (GIF_PATH)(stat.APATH - 1));
			}
			if (!gsSIGNAL.queued && !gifPath[0].isDone())
			{
				stat.APATH = 1;
				stat.P1Q = 0;
				curPath = 0;
			} 			
			else if (!gsSIGNAL.queued && !gifPath[1].isDone())
			{
				stat.APATH = 2;
				stat.P2Q = 0;
				curPath = 1;
			} 			
			else if (!gsSIGNAL.queued && !gifPath[2].isDone() && !Path3Masked())
			{
				stat.APATH = 3;
				stat.P3Q = 0;
				stat.IP3 = 0;
				curPath = 2;
			} 			
			else
			{
				if (isResume || curPath == 0)
				{
					stat.APATH = 0;
					stat.OPH = 0;
				}

				break;
			}
		}
		//Some loaders/Refresh Rate selectors and things dont issue "End of Packet" commands
		//So we look and see if the end of the last tag is all there, if so, stick it in the buffer for the GS :)
		//(Invisible Screens on Terminator 3 and Growlanser 2/3)
		if (gifPath[curPath].curOffset == gifPath[curPath].curSize)
		{
			FlushToMTGS();
		}

		Gif_FinishIRQ();

		//Path3 can rewind the DMA, so we send back the amount we go back!
		if (isPath3)
			return gifPath[2].dmaRewind;
		return 0;
	}

	// XGkick
	bool CanDoPath1() const
	{
		return (stat.APATH == 0 || stat.APATH == 1 || (stat.APATH == 3 && CanDoP3Slice())) && CanDoGif();
	}
	// Direct
	bool CanDoPath2() const
	{
		return (stat.APATH == 0 || stat.APATH == 2 || (stat.APATH == 3 && CanDoP3Slice())) && CanDoGif();
	}
	// DirectHL
	bool CanDoPath2HL() const
	{
		return (stat.APATH == 0 || stat.APATH == 2) && CanDoGif();
	}
	// Gif DMA
	bool CanDoPath3() const
	{
		return ((stat.APATH == 0 && !Path3Masked()) || stat.APATH == 3) && CanDoGif();
	}

	bool CanDoP3Slice() const { return stat.IMT == 1 && gifPath[GIF_PATH_3].state == GIF_PATH_IMAGE; }
	bool CanDoGif() const { return stat.PSE == 0 && stat.DIR == 0 && gsSIGNAL.queued == 0; }
	//Mask stops the next packet which hasnt started from transferring
	bool Path3Masked() const { return ((stat.M3R || stat.M3P) && (gifPath[GIF_PATH_3].state == GIF_PATH_IDLE || gifPath[GIF_PATH_3].state == GIF_PATH_WAIT)); }
};

extern Gif_Unit gifUnit;
