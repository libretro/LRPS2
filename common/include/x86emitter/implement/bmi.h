/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2015  PCSX2 Dev Team
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

// Implement BMI1/BMI2 instruction set

namespace x86Emitter
{

struct xImplBMI_RVM
{
    u8 Prefix;
    u8 MbPrefix;
    u8 Opcode;

    // RVM
    // MULX 	Unsigned multiply without affecting flags, and arbitrary destination registers
    // PDEP 	Parallel bits deposit
    // PEXT 	Parallel bits extract
    // ANDN 	Logical and not 	~x & y
    void operator()(const xRegisterInt &to, const xRegisterInt &from1, const xRegisterInt &from2) const;
    void operator()(const xRegisterInt &to, const xRegisterInt &from1, const xIndirectVoid &from2) const;

};
}
