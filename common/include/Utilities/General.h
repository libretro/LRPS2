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

// --------------------------------------------------------------------------------------
//  PageProtectionMode
// --------------------------------------------------------------------------------------
class PageProtectionMode
{
protected:
    bool m_read;
    bool m_write;
    bool m_exec;

public:
    PageProtectionMode()
    {
        All(false);
    }

    PageProtectionMode &Read(bool allow = true)
    {
        m_read = allow;
        return *this;
    }

    PageProtectionMode &Write(bool allow = true)
    {
        m_write = allow;
        return *this;
    }

    PageProtectionMode &Execute(bool allow = true)
    {
        m_exec = allow;
        return *this;
    }

    PageProtectionMode &All(bool allow = true)
    {
        m_read = m_write = m_exec = allow;
        return *this;
    }

    bool CanRead() const { return m_read; }
    bool CanWrite() const { return m_write; }
    bool CanExecute() const { return m_exec && m_read; }
    bool IsNone() const { return !m_read && !m_write; }
};

static __fi PageProtectionMode PageAccess_None(void)
{
    return PageProtectionMode();
}

static __fi PageProtectionMode PageAccess_ReadOnly(void)
{
    return PageProtectionMode().Read();
}

static __fi PageProtectionMode PageAccess_ReadWrite(void)
{
    return PageAccess_ReadOnly().Write();
}

static __fi PageProtectionMode PageAccess_ExecOnly(void)
{
    return PageAccess_ReadOnly().Execute();
}

static __fi PageProtectionMode PageAccess_Any(void)
{
    return PageProtectionMode().All();
}

// --------------------------------------------------------------------------------------
//  HostSys
// --------------------------------------------------------------------------------------
// (this namespace name sucks, and is a throw-back to an older attempt to make things cross
// platform prior to wxWidgets .. it should prolly be removed -- air)
namespace HostSys
{
void *MmapReserve(uptr base, size_t size);

bool MmapCommitPtr(void *base, size_t size, const PageProtectionMode &mode);
void MmapResetPtr(void *base, size_t size);

// Unmaps a block allocated by SysMmap
extern void Munmap(uptr base, size_t size);

extern void MemProtect(void *baseaddr, size_t size, const PageProtectionMode &mode);

extern void Munmap(void *base, size_t size);

template <uint size>
void MemProtectStatic(u8 (&arr)[size], const PageProtectionMode &mode)
{
    MemProtect(arr, size, mode);
}

}
