/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "Utilities/Dependencies.h"
#include "Utilities/MemcpyFast.h"
#include "Utilities/Exceptions.h"
#include "Utilities/SafeArray.h"
#include "Utilities/General.h"
#include "Utilities/PageFaultSource.h"

#include "../../libretro/retro_messager.h"

#if defined(_WIN32)
#include "Utilities/RedtapeWindows.h"
#include <winnt.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#include <signal.h>
#include <unistd.h>
#endif

// Apple uses the MAP_ANON define instead of MAP_ANONYMOUS, but they mean
// the same thing.
#if defined(__APPLE__) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif

#if defined(_WIN32)
static long DoSysPageFaultExceptionFilter(EXCEPTION_POINTERS *eps)
{
    if (eps->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;

    // Note: This exception can be accessed by the EE or MTVU thread
    // Source_PageFault is a global variable with its own state information
    // so for now we lock this exception code unless someone can fix this better...
    Threading::ScopedLock lock(PageFault_Mutex);
    Source_PageFault->Dispatch(PageFaultInfo((uptr)eps->ExceptionRecord->ExceptionInformation[1]));
    return Source_PageFault->WasHandled() ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}

long __stdcall SysPageFaultExceptionFilter(EXCEPTION_POINTERS *eps)
{
    // Prevent recursive exception filtering by catching the exception from the filter here.
    // In the event that the filter causes an access violation (happened during shutdown
    // because Source_PageFault was deallocated), this will allow the debugger to catch the
    // exception.
    // TODO: find a reliable way to debug the filter itself, I've come up with a few ways that
    // work but I don't fully understand why some do and some don't.
    __try {
        return DoSysPageFaultExceptionFilter(eps);
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

void _platform_InstallSignalHandler(void)
{
    AddVectoredExceptionHandler(true, SysPageFaultExceptionFilter);
}

static DWORD ConvertToWinApi(const PageProtectionMode &mode)
{
    bool can_write = mode.CanWrite();
    // Windows has some really bizarre memory protection enumeration that uses bitwise
    // numbering (like flags) but is in fact not a flag value.  *Someone* from the early
    // microsoft days wasn't a very good coder, me thinks.  --air
    if      (mode.CanExecute())
        return can_write ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    else if (mode.CanRead())
        return can_write ? PAGE_READWRITE : PAGE_READONLY;
    return PAGE_NOACCESS;
}
#elif defined(__unix__) || defined(__APPLE__)
static const uptr m_pagemask = getpagesize() - 1;

// Unix implementation of SIGSEGV handler.  Bind it using sigaction().
static void SysPageFaultSignalFilter(int signal, siginfo_t *siginfo, void *)
{
    Threading::ScopedLock lock(PageFault_Mutex);

    Source_PageFault->Dispatch(PageFaultInfo((uptr)siginfo->si_addr & ~m_pagemask));

    // resumes execution right where we left off (re-executes instruction that
    // caused the SIGSEGV).
    if (Source_PageFault->WasHandled())
        return;

    // Bad mojo!  Completely invalid address.
    // Instigate a trap if we're in a debugger, and if not then do a SIGKILL.
    raise(SIGTRAP);
    raise(SIGKILL);
}

void _platform_InstallSignalHandler(void)
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = SysPageFaultSignalFilter;
#ifdef __APPLE__
    // MacOS uses SIGBUS for memory permission violations
    sigaction(SIGBUS, &sa, NULL);
#else
    sigaction(SIGSEGV, &sa, NULL);
#endif
}

// returns false if the mprotect call fails with an ENOMEM.
static bool _memprotect(void *baseaddr, size_t size, const PageProtectionMode &mode)
{
    uint lnxmode = 0;

    if (mode.CanWrite())
        lnxmode |= PROT_WRITE;
    if (mode.CanRead())
        lnxmode |= PROT_READ;
    if (mode.CanExecute())
        lnxmode |= PROT_EXEC | PROT_READ;

    return (mprotect(baseaddr, size, lnxmode) == 0);
}
#endif

bool HostSys::MmapCommitPtr(void *base, size_t size, const PageProtectionMode &mode)
{
#if defined(_WIN32)
    void *result = VirtualAlloc(base, size, MEM_COMMIT, ConvertToWinApi(mode));
    if (result)
        return true;

    const DWORD errcode = GetLastError();
    if (errcode == ERROR_COMMITMENT_MINIMUM)
       Sleep(1000); // Cut windows some time to rework its memory...
    else if (errcode != ERROR_NOT_ENOUGH_MEMORY && errcode != ERROR_OUTOFMEMORY)
        return false;
#elif defined(__unix__) || defined(__APPLE__)
    // In Unix, reserved memory is automatically committed when its permissions are
    // changed to something other than PROT_NONE.  If the user is committing memory
    // as PROT_NONE, then just ignore this call (memory will be committed automatically
    // later when the user changes permissions to something useful via calls to MemProtect).
    if (mode.IsNone())
        return false;
    if (_memprotect(base, size, mode))
        return true;
#endif

    if (!pxDoOutOfMemory)
        return false;
    pxDoOutOfMemory(size);
#if defined(_WIN32)
    return VirtualAlloc(base, size, MEM_COMMIT, ConvertToWinApi(mode)) != NULL;
#elif defined(__unix__) || defined(__APPLE__)
    return _memprotect(base, size, mode);
#endif
}

void HostSys::MmapResetPtr(void *base, size_t size)
{
#if defined(_WIN32)
    VirtualFree(base, size, MEM_DECOMMIT);
#elif defined(__unix__) || defined(__APPLE__)
    mmap(base, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
#endif
}

void *HostSys::MmapReserve(uptr base, size_t size)
{
#if defined(_WIN32)
    return VirtualAlloc((void*)base, size, MEM_RESERVE, PAGE_NOACCESS);
#elif defined(__unix__) || defined(__APPLE__)
    // On linux a reserve-without-commit is performed by using mmap on a read-only
    // or anonymous source, with PROT_NONE (no-access) permission.  Since the mapping
    // is completely inaccessible, the OS will simply reserve it and will not put it
    // against the commit table.
    return mmap((void*)base, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
}

void HostSys::Munmap(uptr base, size_t size)
{
    if (base)
    {
#if defined(_WIN32)
	    VirtualFree((void *)base, 0, MEM_RELEASE);
#elif defined(__unix__) || defined(__APPLE__)
	    munmap((void *)base, size);
#endif
    }
}

void HostSys::MemProtect(void *baseaddr, size_t size, const PageProtectionMode &mode)
{
#if defined(_WIN32)
    DWORD OldProtect;
    VirtualProtect(baseaddr, size, ConvertToWinApi(mode), &OldProtect);
#elif defined(__unix__) || defined(__APPLE__)
    _memprotect(baseaddr, size, mode);
#endif
}
