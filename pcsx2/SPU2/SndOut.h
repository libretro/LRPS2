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

#pragma once

// Number of stereo samples per SndOut block.
// All drivers must work in units of this size when communicating with
// SndOut.
#define SndOutPacketSize 64

// Overall master volume shift; this is meant to be a precision value and does not affect
// actual output volumes.  It converts SPU2 16 bit volumes to 32-bit volumes, and likewise
// downsamples 32 bit samples to 16 bit sound driver output (this way timestretching and
// DSP effects get better precision results)
#define SndOutVolumeShift 12
#define SndOutVolumeShift32 4 // shift up, not down, (formula = 16 - SndOutVolumeShift)

struct Stereo51Out16DplII;
struct Stereo51Out32DplII;

struct Stereo51Out16Dpl; // similar to DplII but without rear balancing
struct Stereo51Out32Dpl;

extern void ResetDplIIDecoder();
extern void ProcessDplIISample16(const StereoOut32& src, Stereo51Out16DplII* s);
extern void ProcessDplIISample32(const StereoOut32& src, Stereo51Out32DplII* s);
extern void ProcessDplSample16(const StereoOut32& src, Stereo51Out16Dpl* s);
extern void ProcessDplSample32(const StereoOut32& src, Stereo51Out32Dpl* s);

struct StereoOut16
{
	s16 Left;
	s16 Right;

	StereoOut16()
		: Left(0)
		, Right(0)
	{
	}

	StereoOut16(const StereoOut32& src)
		: Left((s16)src.Left)
		, Right((s16)src.Right)
	{
	}

	StereoOut16(s16 left, s16 right)
		: Left(left)
		, Right(right)
	{
	}

	StereoOut32 UpSample() const;
};

struct StereoOutFloat
{
	float Left;
	float Right;

	StereoOutFloat()
		: Left(0)
		, Right(0)
	{
	}

	explicit StereoOutFloat(const StereoOut32& src)
		: Left(src.Left / 2147483647.0f)
		, Right(src.Right / 2147483647.0f)
	{
	}

	explicit StereoOutFloat(s32 left, s32 right)
		: Left(left / 2147483647.0f)
		, Right(right / 2147483647.0f)
	{
	}

	StereoOutFloat(float left, float right)
		: Left(left)
		, Right(right)
	{
	}
};

struct Stereo21Out16
{
	s16 Left;
	s16 Right;
	s16 LFE;
};

struct Stereo40Out16
{
	s16 Left;
	s16 Right;
	s16 LeftBack;
	s16 RightBack;
};

struct Stereo40Out32
{
	s32 Left;
	s32 Right;
	s32 LeftBack;
	s32 RightBack;
};

struct Stereo41Out16
{
	s16 Left;
	s16 Right;
	s16 LFE;
	s16 LeftBack;
	s16 RightBack;
};

struct Stereo51Out16
{
	s16 Left;
	s16 Right;
	s16 Center;
	s16 LFE;
	s16 LeftBack;
	s16 RightBack;

	// Implementation Note: Center and Subwoofer/LFE -->
	// This method is simple and sounds nice.  It relies on the speaker/soundcard
	// systems do to their own low pass / crossover.  Manual lowpass is wasted effort
	// and can't match solid state results anyway.
};

struct Stereo51Out16DplII
{
	s16 Left;
	s16 Right;
	s16 Center;
	s16 LFE;
	s16 LeftBack;
	s16 RightBack;
};

struct Stereo51Out32DplII
{
	s32 Left;
	s32 Right;
	s32 Center;
	s32 LFE;
	s32 LeftBack;
	s32 RightBack;
};

struct Stereo51Out16Dpl
{
	s16 Left;
	s16 Right;
	s16 Center;
	s16 LFE;
	s16 LeftBack;
	s16 RightBack;
};

struct Stereo51Out32Dpl
{
	s32 Left;
	s32 Right;
	s32 Center;
	s32 LFE;
	s32 LeftBack;
	s32 RightBack;
};

struct Stereo71Out16
{
	s16 Left;
	s16 Right;
	s16 Center;
	s16 LFE;
	s16 LeftBack;
	s16 RightBack;
	s16 LeftSide;
	s16 RightSide;
};

struct Stereo71Out32
{
	s32 Left;
	s32 Right;
	s32 Center;
	s32 LFE;
	s32 LeftBack;
	s32 RightBack;
	s32 LeftSide;
	s32 RightSide;
};

struct Stereo20Out32
{
	s32 Left;
	s32 Right;
};

struct Stereo21Out32
{
	s32 Left;
	s32 Right;
	s32 LFE;
};

struct Stereo41Out32
{
	s32 Left;
	s32 Right;
	s32 LFE;
	s32 LeftBack;
	s32 RightBack;
};

struct Stereo51Out32
{
	s32 Left;
	s32 Right;
	s32 Center;
	s32 LFE;
	s32 LeftBack;
	s32 RightBack;
};
