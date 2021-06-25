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

#include "Global.h"

#ifdef __LIBRETRO__
static __forceinline bool MsgToConsole() { return false; }

static __forceinline bool MsgKeyOnOff() { return false; }
static __forceinline bool MsgVoiceOff() { return false; }
static __forceinline bool MsgDMA() { return false; }
static __forceinline bool MsgAutoDMA() { return false; }
static __forceinline bool MsgOverruns() { return false; }
static __forceinline bool MsgCache() { return false; }

static __forceinline bool AccessLog() { return false; }
static __forceinline bool DMALog() { return false; }
static __forceinline bool WaveLog() { return false; }

static __forceinline bool CoresDump() { return false; }
static __forceinline bool MemDump() { return false; }
static __forceinline bool RegDump() { return false; }
static __forceinline bool VisualDebug() { return false; }
#else
#ifdef _WIN32
#include <soundtouch\soundtouch\SoundTouch.h>
#endif

extern bool DebugEnabled;

extern bool _MsgToConsole;
extern bool _MsgKeyOnOff;
extern bool _MsgVoiceOff;
extern bool _MsgDMA;
extern bool _MsgAutoDMA;
extern bool _MsgOverruns;
extern bool _MsgCache;

extern bool _AccessLog;
extern bool _DMALog;
extern bool _WaveLog;

extern bool _CoresDump;
extern bool _MemDump;
extern bool _RegDump;
extern bool _visual_debug_enabled;

static __forceinline bool MsgToConsole() { return _MsgToConsole & DebugEnabled; }

static __forceinline bool MsgKeyOnOff() { return _MsgKeyOnOff & MsgToConsole(); }
static __forceinline bool MsgVoiceOff() { return _MsgVoiceOff & MsgToConsole(); }
static __forceinline bool MsgDMA() { return _MsgDMA & MsgToConsole(); }
static __forceinline bool MsgAutoDMA() { return _MsgAutoDMA & MsgToConsole(); }
static __forceinline bool MsgOverruns() { return _MsgOverruns & MsgToConsole(); }
static __forceinline bool MsgCache() { return _MsgCache & MsgToConsole(); }

static __forceinline bool AccessLog() { return _AccessLog & DebugEnabled; }
static __forceinline bool DMALog() { return _DMALog & DebugEnabled; }
static __forceinline bool WaveLog() { return _WaveLog & DebugEnabled; }

static __forceinline bool CoresDump() { return _CoresDump & DebugEnabled; }
static __forceinline bool MemDump() { return _MemDump & DebugEnabled; }
static __forceinline bool RegDump() { return _RegDump & DebugEnabled; }
static __forceinline bool VisualDebug() { return _visual_debug_enabled & DebugEnabled; }

extern wxString AccessLogFileName;
extern wxString DMA4LogFileName;
extern wxString DMA7LogFileName;
extern wxString CoresDumpFileName;
extern wxString MemDumpFileName;
extern wxString RegDumpFileName;

#ifndef __POSIX__
extern wchar_t dspPlugin[];
extern int dspPluginModule;

extern bool dspPluginEnabled;
#endif

namespace SoundtouchCfg
{
	extern void ApplySettings(soundtouch::SoundTouch& sndtouch);
}

//////

extern void ReadSettings();
extern void WriteSettings();
extern void configure();
#endif

extern int Interpolation;
extern int numSpeakers;
extern bool EffectsDisabled;
extern float FinalVolume; // Global / pre-scale
extern bool AdvancedVolumeControl;
extern float VolumeAdjustFLdb;
extern float VolumeAdjustCdb;
extern float VolumeAdjustFRdb;
extern float VolumeAdjustBLdb;
extern float VolumeAdjustBRdb;
extern float VolumeAdjustSLdb;
extern float VolumeAdjustSRdb;
extern float VolumeAdjustLFEdb;
extern bool postprocess_filter_enabled;
extern bool postprocess_filter_dealias;

extern int dplLevel;

extern u32 OutputModule;
extern int SndOutLatencyMS;
extern int SynchMode;

