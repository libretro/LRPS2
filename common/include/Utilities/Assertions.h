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

#ifndef __pxFUNCTION__
#if defined(__GNUG__)
#define __pxFUNCTION__ __PRETTY_FUNCTION__
#else
#define __pxFUNCTION__ __FUNCTION__
#endif
#endif

#ifndef wxNullChar
#define wxNullChar ((wxChar *)NULL)
#endif

// --------------------------------------------------------------------------------------
//  DiagnosticOrigin
// --------------------------------------------------------------------------------------
struct DiagnosticOrigin
{
    DiagnosticOrigin()
    {
    }
};

// ----------------------------------------------------------------------------------------
//  pxAssert / pxAssertDev
// ----------------------------------------------------------------------------------------
// Standard "nothrow" assertions.  All assertions act as valid conditional statements that
// return the result of the specified conditional; useful for handling failed assertions in
// a "graceful" fashion when utilizing the "ignore" feature of assertion debugging.
// These macros are mostly intended for "pseudo-weak" assumptions within code, most often for
// testing threaded user interface code (threading of the UI is a prime example since often
// even very robust assertions can fail in very rare conditions, due to the complex variety
// of ways the user can invoke UI events).
//
// All macros return TRUE if the assertion succeeds, or FALSE if the assertion failed
// (thus matching the condition of the assertion itself).
//
// pxAssertDev is an assertion tool for Devel builds, intended for sanity checking and/or
// bounds checking variables in areas which are not performance critical.  Another common
// use is for checking thread affinity on utility functions.
//
// Credits: These macros are based on a combination of wxASSERT, MSVCRT's assert and the
// ATL's Assertion/Assumption macros.  the best of all worlds!

// --------------------------------------------------------------------------------------
//  pxAssume / pxAssumeDev / pxFail / pxFailDev
// --------------------------------------------------------------------------------------
// Assumptions are hints to the compiler that the condition will always be true,
// the condition should never fail under any circumstance in release builds
// or else you might get incorrect compiler generated code.
//
// Performance: All assumption/fail  types optimize into __assume()/likely() directives in
// Release builds (non-dev varieties optimize as such in Devel builds as well).
// __assume(0) is a special form of __assume() which tells the compiler that the code path
// is not reachable and will cause undefined results if it is reachable...
//
// Having pxFail and pxFailDev translate into __assume statements is very dangerous, since
// it can lead to the compiler optimizing out code and leading to crashes in dev/release
// builds. To have code optimized, explicitly use pxAssume(false) or pxAssumeDev(false,msg);

#define pxDiagSpot DiagnosticOrigin()
#define pxAssertSpot(cond) DiagnosticOrigin()

// pxAssertRel ->
// Special release-mode assertion.  Limited use since stack traces in release mode builds
// (especially with LTCG) are highly suspect.  But when troubleshooting crashes that only
// rear ugly heads in optimized builds, this is one of the few tools we have.

#define pxAssertRel(cond, msg) ((likely(cond)) || (pxOnAssert(pxAssertSpot(cond), msg), false))
#define pxAssumeRel(cond, msg) ((void)((!likely(cond)) && (pxOnAssert(pxAssertSpot(cond), msg), false)))

// Release Builds just use __assume as an optimization, and return the conditional
// as a result (which is optimized to nil if unused).

#define pxAssertMsg(cond, msg) (likely(cond))
#define pxAssertDev(cond, msg) (likely(cond))

#define pxAssumeMsg(cond, msg) (__assume(cond))
#define pxAssumeDev(cond, msg) (__assume(cond))

#define pxAssert(cond) pxAssertMsg(cond, wxNullChar)
