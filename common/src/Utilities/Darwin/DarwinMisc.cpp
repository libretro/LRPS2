/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
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

#include "../PrecompiledHeader.h"

#include <cstring>
#include <cstdlib>

#include <sys/types.h>
#include <sys/sysctl.h>

#include <mach/mach_time.h>

#define NELEM(x) \
    ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

// Darwin (OSX) is a bit different from Linux when requesting properties of
// the OS because of its BSD/Mach heritage. Helpfully, most of this code
// should translate pretty well to other *BSD systems. (e.g.: the sysctl(3)
// interface).
//
// For an overview of all of Darwin's sysctls, check:
// https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/sysctl.3.html

// Return the total physical memory on the machine, in bytes. Returns 0 on
// failure (not supported by the operating system).
u64 GetPhysicalMemory()
{
    static u64 mem = 0;

    // fetch the total memory only once, as its an expensive system call and
    // doesn't change during the course of the program. Thread-safety is
    // ensured by atomic operations with full-barriers (usually compiled
    // down to XCHG on x86).
    if (__atomic_load_n(&mem, __ATOMIC_SEQ_CST) == 0) {
        u64 getmem = 0;
        size_t len = sizeof(getmem);
        int mib[] = {CTL_HW, HW_MEMSIZE};
        if (sysctl(mib, NELEM(mib), &getmem, &len, NULL, 0) < 0) {
            perror("sysctl:");
        }
        __atomic_store_n(&mem, getmem, __ATOMIC_SEQ_CST);
    }

    return mem;
}

#if 0
void InitCPUTicks()
{
}

// returns the performance-counter frequency: ticks per second (Hz)
//
// usage:
//   u64 seconds_passed = GetCPUTicks() / GetTickFrequency();
//   u64 millis_passed = (GetCPUTicks() * 1000) / GetTickFrequency();
//
// NOTE: multiply, subtract, ... your ticks before dividing by
// GetTickFrequency() to maintain good precision.
u64 GetTickFrequency()
{
    static u64 freq = 0;

    // by the time denom is not 0, the structure will have been fully
    // updated and no more atomic accesses are necessary.
    if (__atomic_load_n(&freq, __ATOMIC_SEQ_CST) == 0) {
        mach_timebase_info_data_t info;

        // mach_timebase_info() is a syscall, very slow, that's why we take
        // pains to only do it once. On x86(-64), the result is guaranteed
        // to be info.denom == info.numer == 1 (i.e.: the frequency is 1e9,
        // which means GetCPUTicks is just nanoseconds).
        if (mach_timebase_info(&info) != KERN_SUCCESS) {
            abort();
        }

        // store the calculated value atomically
        __atomic_store_n(&freq, (u64)1e9 * (u64)info.denom / (u64)info.numer, __ATOMIC_SEQ_CST);
    }

    return freq;
}
#endif
