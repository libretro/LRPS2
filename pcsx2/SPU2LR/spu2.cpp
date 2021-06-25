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

#include "PrecompiledHeader.h"
#include "Global.h"
#include "spu2.h"
#include "Dma.h"
#include "R3000A.h"
#include "Utilities/pxStreams.h"
#include "AppCoreThread.h"

using namespace Threading;

MutexRecursive mtx_SPU2Status;

#include "svnrev.h"

int SampleRate = 48000;

static bool IsOpened = false;
static bool IsInitialized = false;

static u32 pClocks = 0;

u32* cyclePtr = nullptr;
u32 lClocks = 0;
//static bool cpu_detected = false;

static bool CheckSSE()
{
	return true;
}
// --------------------------------------------------------------------------------------
//  DMA 4/7 Callbacks from Core Emulator
// --------------------------------------------------------------------------------------

u16* DMABaseAddr;

u32 SPU2ReadMemAddr(int core)
{
	return Cores[core].MADR;
}
void SPU2WriteMemAddr(int core, u32 value)
{
	Cores[core].MADR = value;
}

void SPU2setDMABaseAddr(uptr baseaddr)
{
	DMABaseAddr = (u16*)baseaddr;
}

void SPU2setSettingsDir(const char* dir)
{
}

void SPU2setLogDir(const char* dir)
{
}

void SPU2readDMA4Mem(u16* pMem, u32 size) // size now in 16bit units
{
	if (cyclePtr != nullptr)
		TimeUpdate(*cyclePtr);

	Cores[0].DoDMAread(pMem, size);
}

void SPU2writeDMA4Mem(u16* pMem, u32 size) // size now in 16bit units
{
	if (cyclePtr != nullptr)
		TimeUpdate(*cyclePtr);

	Cores[0].DoDMAwrite(pMem, size);
}

void SPU2interruptDMA4()
{
	Cores[0].Regs.STATX |= 0x80;
	//Cores[0].Regs.ATTR &= ~0x30;
}

void SPU2interruptDMA7()
{
	Cores[1].Regs.STATX |= 0x80;
	//Cores[1].Regs.ATTR &= ~0x30;
}

void SPU2readDMA7Mem(u16* pMem, u32 size)
{
	if (cyclePtr != nullptr)
		TimeUpdate(*cyclePtr);

	Cores[1].DoDMAread(pMem, size);
}

void SPU2writeDMA7Mem(u16* pMem, u32 size)
{
	if (cyclePtr != nullptr)
		TimeUpdate(*cyclePtr);

	Cores[1].DoDMAwrite(pMem, size);
}

s32 SPU2reset()
{
	if (SampleRate != 48000)
	{
		SampleRate = 48000;
		SndBuffer::Cleanup();
      SndBuffer::Init();
	}
	else
		SampleRate = 48000;

	memset(spu2regs, 0, 0x010000);
	memset(_spu2mem, 0, 0x200000);
	memset(_spu2mem + 0x2800, 7, 0x10); // from BIOS reversal. Locks the voices so they don't run free.
	Cores[0].Init(0);
	Cores[1].Init(1);
	return 0;
}

s32 SPU2ps1reset()
{
	printf("RESET PS1 \n");

	if (SampleRate != 44100)
	{
		SampleRate = 44100;
		SndBuffer::Cleanup();
      SndBuffer::Init();
	}
	else
		SampleRate = 44100;

	/* memset(spu2regs, 0, 0x010000);
    memset(_spu2mem, 0, 0x200000);
    memset(_spu2mem + 0x2800, 7, 0x10); // from BIOS reversal. Locks the voices so they don't run free.
    Cores[0].Init(0);
    Cores[1].Init(1);*/
	return 0;
}

float VolumeAdjustFLdb; // decibels settings, cos audiophiles love that
float VolumeAdjustCdb;
float VolumeAdjustFRdb;
float VolumeAdjustBLdb;
float VolumeAdjustBRdb;
float VolumeAdjustSLdb;
float VolumeAdjustSRdb;
float VolumeAdjustLFEdb;
float VolumeAdjustFL; // linear coefs calcualted from decibels,
float VolumeAdjustC;
float VolumeAdjustFR;
float VolumeAdjustBL;
float VolumeAdjustBR;
float VolumeAdjustSL;
float VolumeAdjustSR;
float VolumeAdjustLFE;

void ReadSettings()
{
   Interpolation = 4;
	EffectsDisabled = false;
	postprocess_filter_dealias = false;
	VolumeAdjustCdb = 0;
	VolumeAdjustFLdb = 0;
	VolumeAdjustFRdb = 0;
	VolumeAdjustBLdb = 0;
	VolumeAdjustBRdb = 0;
	VolumeAdjustSLdb = 0;
	VolumeAdjustSRdb = 0;
	VolumeAdjustLFEdb = 0;
	delayCycles = 4;
	VolumeAdjustC = powf(10, VolumeAdjustCdb / 10);
	VolumeAdjustFL = powf(10, VolumeAdjustFLdb / 10);
	VolumeAdjustFR = powf(10, VolumeAdjustFRdb / 10);
	VolumeAdjustBL = powf(10, VolumeAdjustBLdb / 10);
	VolumeAdjustBR = powf(10, VolumeAdjustBRdb / 10);
	VolumeAdjustSL = powf(10, VolumeAdjustSLdb / 10);
	VolumeAdjustSR = powf(10, VolumeAdjustSRdb / 10);
	VolumeAdjustLFE = powf(10, VolumeAdjustLFEdb / 10);
}

s32 SPU2init()
{
	assert(regtable[0x400] == nullptr);

	if (IsInitialized)
		return 0;

	IsInitialized = true;

	ReadSettings();

	srand((unsigned)time(nullptr));

	spu2regs = (s16*)malloc(0x010000);
	_spu2mem = (s16*)malloc(0x200000);

	// adpcm decoder cache:
	//  the cache data size is determined by taking the number of adpcm blocks
	//  (2MB / 16) and multiplying it by the decoded block size (28 samples).
	//  Thus: pcm_cache_data = 7,340,032 bytes (ouch!)
	//  Expanded: 16 bytes expands to 56 bytes [3.5:1 ratio]
	//    Resulting in 2MB * 3.5.

	pcm_cache_data = (PcmCacheEntry*)calloc(pcm_BlockCount, sizeof(PcmCacheEntry));

	if ((spu2regs == nullptr) || (_spu2mem == nullptr) || (pcm_cache_data == nullptr))
	{
		return -1;
	}

	// Patch up a copy of regtable that directly maps "nullptrs" to SPU2 memory.

	memcpy(regtable, regtable_original, sizeof(regtable));

	for (uint mem = 0; mem < 0x800; mem++)
	{
		u16* ptr = regtable[mem >> 1];
		if (!ptr)
		{
			regtable[mem >> 1] = &(spu2Ru16(mem));
		}
	}

	SPU2reset();

	InitADSR();

	return 0;
}

s32 SPU2open(void* pDsp)
{
	ScopedLock lock(mtx_SPU2Status);
	if (IsOpened)
		return 0;

	IsOpened = true;
	lClocks = (cyclePtr != nullptr) ? *cyclePtr : 0;

   SndBuffer::Init();
	SPU2setDMABaseAddr((uptr)iopMem->Main);
	SPU2setClockPtr(&psxRegs.cycle);
	return 0;
}

void SPU2close()
{
	ScopedLock lock(mtx_SPU2Status);
	if (!IsOpened)
		return;
	IsOpened = false;


	SndBuffer::Cleanup();
}

void SPU2shutdown()
{
	if (!IsInitialized)
		return;
	IsInitialized = false;

	SPU2close();

	safe_free(spu2regs);
	safe_free(_spu2mem);
	safe_free(pcm_cache_data);
}

void SPU2setClockPtr(u32* ptr)
{
	cyclePtr = ptr;
}

void SPU2async(u32 cycles)
{
	if (cyclePtr != nullptr)
	{
		TimeUpdate(*cyclePtr);
	}
	else
	{
		pClocks += cycles;
		TimeUpdate(pClocks);
	}
}

u16 SPU2read(u32 rmem)
{
	u16 ret = 0xDEAD;
	u32 core = 0, mem = rmem & 0xFFFF, omem = mem;
	if (mem & 0x400)
	{
		omem ^= 0x400;
		core = 1;
	}

	if (omem == 0x1f9001AC)
	{
		ret = Cores[core].DmaRead();
	}
	else
	{
		if (cyclePtr != nullptr)
			TimeUpdate(*cyclePtr);

		if (rmem >> 16 == 0x1f80)
		{
			ret = Cores[0].ReadRegPS1(rmem);
		}
		else if (mem >= 0x800)
		{
			ret = spu2Ru16(mem);
		}
		else
		{
			ret = *(regtable[(mem >> 1)]);
		}
	}

	return ret;
}

void SPU2write(u32 rmem, u16 value)
{
	// Note: Reverb/Effects are very sensitive to having precise update timings.
	// If the SPU2 isn't in in sync with the IOP, samples can end up playing at rather
	// incorrect pitches and loop lengths.

	if (cyclePtr != nullptr)
		TimeUpdate(*cyclePtr);

	if (rmem >> 16 == 0x1f80)
		Cores[0].WriteRegPS1(rmem, value);
	else
	{
		SPU2_FastWrite(rmem, value);
	}
}

// if start is 1, starts recording spu2 data, else stops
// returns a non zero value if successful
// for now, pData is not used
int SPU2setupRecording(int start, std::wstring* filename)
{
	return 0;
}

s32 SPU2freeze(int mode, freezeData* data)
{
	pxAssume(data != nullptr);
	if (!data)
	{
		printf("SPU2 savestate null pointer!\n");
		return -1;
	}

	if (mode == FREEZE_SIZE)
	{
		data->size = SPU2Savestate::SizeIt();
		return 0;
	}

	pxAssume(mode == FREEZE_LOAD || mode == FREEZE_SAVE);

	if (data->data == nullptr)
	{
		printf("SPU2 savestate null pointer!\n");
		return -1;
	}

	SPU2Savestate::DataBlock& spud = (SPU2Savestate::DataBlock&)*(data->data);

	switch (mode)
	{
		case FREEZE_LOAD:
			return SPU2Savestate::ThawIt(spud);
		case FREEZE_SAVE:
			return SPU2Savestate::FreezeIt(spud);

			jNO_DEFAULT;
	}

	// technically unreachable, but kills a warning:
	return 0;
}

void SPU2DoFreezeOut(void* dest)
{
	ScopedLock lock(mtx_SPU2Status);

	freezeData fP = {0, (s8*)dest};
	if (SPU2freeze(FREEZE_SIZE, &fP) != 0)
		return;
	if (!fP.size)
		return;

	Console.Indent().WriteLn("Saving SPU2");

	if (SPU2freeze(FREEZE_SAVE, &fP) != 0)
		throw std::runtime_error(" * SPU2: Error saving state!\n");
}


void SPU2DoFreezeIn(pxInputStream& infp)
{
	ScopedLock lock(mtx_SPU2Status);

	freezeData fP = {0, nullptr};
	if (SPU2freeze(FREEZE_SIZE, &fP) != 0)
		fP.size = 0;

	Console.Indent().WriteLn("Loading SPU2");

	if (!infp.IsOk() || !infp.Length())
	{
		// no state data to read, but SPU2 expects some state data?
		// Issue a warning to console...
		if (fP.size != 0)
			Console.Indent().Warning("Warning: No data for SPU2 found. Status may be unpredictable.");

		return;

		// Note: Size mismatch check could also be done here on loading, but
		// some plugins may have built-in version support for non-native formats or
		// older versions of a different size... or could give different sizes depending
		// on the status of the plugin when loading, so let's ignore it.
	}

	ScopedAlloc<s8> data(fP.size);
	fP.data = data.GetPtr();

	infp.Read(fP.data, fP.size);
	if (SPU2freeze(FREEZE_LOAD, &fP) != 0)
		throw std::runtime_error(" * SPU2: Error loading state!\n");
}
