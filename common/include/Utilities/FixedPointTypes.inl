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

#include "FixedPointTypes.h"
#include <cmath> // for pow!

template <int Precision>
FixedInt<Precision>::FixedInt()
{
    Raw = 0;
}

template <int Precision>
FixedInt<Precision>::FixedInt(int signedval)
{
    Raw = signedval * Precision;
}

template <int Precision>
FixedInt<Precision>::FixedInt(double doubval)
{
    Raw = lround(doubval * (double)Precision);
}

template <int Precision>
FixedInt<Precision>::FixedInt(float floval)
{
    Raw = lroundf(floval * (float)Precision);
}

template <int Precision>
FixedInt<Precision> FixedInt<Precision>::operator+(const FixedInt<Precision> &right) const
{
    return FixedInt<Precision>().SetRaw(Raw + right.Raw);
}

template <int Precision>
FixedInt<Precision> FixedInt<Precision>::operator-(const FixedInt<Precision> &right) const
{
    return FixedInt<Precision>().SetRaw(Raw - right.Raw);
}

template <int Precision>
FixedInt<Precision> &FixedInt<Precision>::operator+=(const FixedInt<Precision> &right)
{
    return SetRaw(Raw + right.Raw);
}

template <int Precision>
FixedInt<Precision> &FixedInt<Precision>::operator-=(const FixedInt<Precision> &right)
{
    return SetRaw(Raw - right.Raw);
}

// Uses 64 bit internally to avoid overflows.  For more precise/optimized 32 bit math
// you'll need to use the Raw values directly.
template <int Precision>
FixedInt<Precision> FixedInt<Precision>::operator*(const FixedInt<Precision> &right) const
{
    s64 mulres = (s64)Raw * right.Raw;
    return FixedInt<Precision>().SetRaw((s32)(mulres / Precision));
}

// Uses 64 bit internally to avoid overflows.  For more precise/optimized 32 bit math
// you'll need to use the Raw values directly.
template <int Precision>
FixedInt<Precision> FixedInt<Precision>::operator/(const FixedInt<Precision> &right) const
{
    s64 divres = Raw * Precision;
    return FixedInt<Precision>().SetRaw((s32)(divres / right.Raw));
}

// Uses 64 bit internally to avoid overflows.  For more precise/optimized 32 bit math
// you'll need to use the Raw values directly.
template <int Precision>
FixedInt<Precision> &FixedInt<Precision>::operator*=(const FixedInt<Precision> &right)
{
    s64 mulres = (s64)Raw * right.Raw;
    return SetRaw((s32)(mulres / Precision));
}

// Uses 64 bit internally to avoid overflows.  For more precise/optimized 32 bit math
// you'll need to use the Raw values directly.
template <int Precision>
FixedInt<Precision> &FixedInt<Precision>::operator/=(const FixedInt<Precision> &right)
{
    s64 divres = Raw * Precision;
    return SetRaw((s32)(divres / right.Raw));
}

template <int Precision>
FixedInt<Precision> &FixedInt<Precision>::SetRaw(s32 rawsrc)
{
    Raw = rawsrc;
    return *this;
}

template <int Precision>
int FixedInt<Precision>::ToIntRounded() const
{
    return (Raw + (Precision / 2)) / Precision;
}
