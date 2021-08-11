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


#ifndef __PLUGINCALLBACKS_H__
#define __PLUGINCALLBACKS_H__

#ifndef BOOL
typedef int BOOL;
#endif

// Use fastcall by default, since under most circumstances the object-model approach of the
// API will benefit considerably from it.  (Yes, this means that compilers that do not
// support fastcall are basically unusable for plugin creation.  Too bad. :p
#define PS2E_CALLBACK __fastcall

#ifndef __cplusplus
extern "C" {
#endif

// ------------------------------------------------------------------------------------
//  Plugin Type / Version Enumerations
// ------------------------------------------------------------------------------------

enum PluginLibVersion {
    PS2E_VER_GS = 0x1000,
    PS2E_VER_PAD = 0x1000,
    PS2E_VER_DEV9 = 0x1000,
    PS2E_VER_USB = 0x1000,
    PS2E_VER_SIO = 0x1000
};

#ifndef __cplusplus
}
#endif

#endif // __PLUGINCALLBACKS_H__
