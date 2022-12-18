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

#include <algorithm>
#include <cmath>

#include "Global.h"

static float Gfl = 0, Gfr = 0;
static float LMax = 0, RMax = 0;

static float AccL = 0;
static float AccR = 0;

const float Scale = 4294967296.0f; // tweak this value to change the overall output volume

const float GainL = 0.80f * Scale;
const float GainR = 0.80f * Scale;

const float GainC = 0.75f * Scale;

const float GainSL = 0.90f * Scale;
const float GainSR = 0.90f * Scale;

const float GainLFE = 0.90f * Scale;

const float AddCLR = 0.20f * Scale; // Stereo expansion

void ResetDplIIDecoder(void)
{
	Gfl = 0;
	Gfr = 0;
	LMax = 0;
	RMax = 0;
	AccL = 0;
	AccR = 0;
}

void ProcessDplIISample32(const StereoOut32& src, Stereo51Out32DplII* s)
{
	float IL = src.Left / (float)(1 << (SndOutVolumeShift + 16));
	float IR = src.Right / (float)(1 << (SndOutVolumeShift + 16));

	// Calculate center channel and LFE
	float C = (IL + IR) * 0.5f;
	float SUB = C; // no need to lowpass, the speaker amplifier should take care of it

	float L = IL - C; // Effective L/R data
	float R = IR - C;

	// Peak L/R
	float PL = std::abs(L);
	float PR = std::abs(R);

	AccL += (PL - AccL) * 0.1f;
	AccR += (PR - AccR) * 0.1f;

	// Calculate power balance
	float Balance = (AccR - AccL); // -1 .. 1

	// If the power levels are different, then the audio is meant for the front speakers
	float Frontness = std::abs(Balance);
	float Rearness = 1 - Frontness; // And the other way around

	// Equalize the power levels for L/R
	float B = std::min(0.9f, std::max(-0.9f, Balance));

	float VL = L / (1 - B); // if B>0, it means R>L, so increase L, else decrease L
	float VR = R / (1 + B); // vice-versa

	// 1.73+1.22 = 2.94; 2.94 = 0.34 = 0.9996; Close enough.
	// The range for VL/VR is approximately 0..1,
	// But in the cases where VL/VR are > 0.5, Rearness is 0 so it should never overflow.
	const float RearScale = 0.34f * 2;

	float SL = (VR * 1.73f - VL * 1.22f) * RearScale * Rearness;
	float SR = (VR * 1.22f - VL * 1.73f) * RearScale * Rearness;
	// Possible experiment: Play with stereo expension levels on rear

	// Adjust the volume of the front speakers based on what we calculated above
	L *= Frontness;
	R *= Frontness;

	s32 CX = (s32)(C * AddCLR);

	s->Left = (s32)(L * GainL) + CX;
	s->Right = (s32)(R * GainR) + CX;
	s->Center = (s32)(C * GainC);
	s->LFE = (s32)(SUB * GainLFE);
	s->LeftBack = (s32)(SL * GainSL);
	s->RightBack = (s32)(SR * GainSR);
}

void ProcessDplIISample16(const StereoOut32& src, Stereo51Out16DplII* s)
{
	Stereo51Out32DplII ss;
	ProcessDplIISample32(src, &ss);

	s->Left = ss.Left >> 16;
	s->Right = ss.Right >> 16;
	s->Center = ss.Center >> 16;
	s->LFE = ss.LFE >> 16;
	s->LeftBack = ss.LeftBack >> 16;
	s->RightBack = ss.RightBack >> 16;
}

void ProcessDplSample32(const StereoOut32& src, Stereo51Out32Dpl* s)
{
	float ValL = src.Left / (float)(1 << (SndOutVolumeShift + 16));
	float ValR = src.Right / (float)(1 << (SndOutVolumeShift + 16));

	float C = (ValL + ValR) * 0.5f; //+15.8
	float S = (ValL - ValR) * 0.5f;

	float L = ValL - C; //+15.8
	float R = ValR - C;

	float SUB = C;

	s32 CX = (s32)(C * AddCLR); // +15.16

	s->Left = (s32)(L * GainL) + CX; // +15.16 = +31, can grow to +32 if (GainL + AddCLR)>255
	s->Right = (s32)(R * GainR) + CX;
	s->Center = (s32)(C * GainC); // +15.16 = +31
	s->LFE = (s32)(SUB * GainLFE);
	s->LeftBack = (s32)(S * GainSL);
	s->RightBack = (s32)(S * GainSR);
}

void ProcessDplSample16(const StereoOut32& src, Stereo51Out16Dpl* s)
{
	Stereo51Out32Dpl ss;
	ProcessDplSample32(src, &ss);

	s->Left = ss.Left >> 16;
	s->Right = ss.Right >> 16;
	s->Center = ss.Center >> 16;
	s->LFE = ss.LFE >> 16;
	s->LeftBack = ss.LeftBack >> 16;
	s->RightBack = ss.RightBack >> 16;
}
