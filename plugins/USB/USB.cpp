/*  USBnull
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

#include <stdlib.h>
#include <string>
#include "svnrev.h"
#include "USB.h"

USBcallback USBirq;

s8 *usbregs, *ram;

EXPORT_C_(void)
USBconfigure()
{
}

EXPORT_C_(s32)
USBinit()
{
    // Initialize memory structures here.
    usbregs = (s8 *)calloc(0x10000, 1);

    if (usbregs == NULL)
        return -1;

    return 0;
}

EXPORT_C_(void)
USBshutdown()
{
    free(usbregs);
    usbregs = NULL;
}

EXPORT_C_(s32)
USBopen()
{
    // Take care of anything else we need on opening, other then initialization.
    return 0;
}

EXPORT_C_(void)
USBclose()
{
}

// Note: actually uncommenting the read/write functions I provided here
// caused uLauncher.elf to hang on startup, so careful when experimenting.
EXPORT_C_(u8)
USBread8(u32 addr)
{
    u8 value = 0;

    switch (addr) {
        // Handle any appropriate addresses here.
        case 0x1f801600:
#if 0
            g_plugin_log.WriteLn("(USBnull) 8 bit read at address %lx", addr);
#endif
            break;

        default:
            //value = usbRu8(addr);
#if 0
            g_plugin_log.WriteLn("*(USBnull) 8 bit read at address %lx", addr);
#endif
            break;
    }
    return value;
}

EXPORT_C_(u16)
USBread16(u32 addr)
{
    u16 value = 0;

    switch (addr) {
        // Handle any appropriate addresses here.
        case 0x1f801600:
#if 0
            g_plugin_log.WriteLn("(USBnull) 16 bit read at address %lx", addr);
#endif
            break;

        default:
            //value = usbRu16(addr);
#if 0
            g_plugin_log.WriteLn("(USBnull) 16 bit read at address %lx", addr);
#endif
            break;
    }
    return value;
}

EXPORT_C_(u32)
USBread32(u32 addr)
{
    u32 value = 0;

    switch (addr) {
        // Handle any appropriate addresses here.
        case 0x1f801600:
#if 0
            g_plugin_log.WriteLn("(USBnull) 32 bit read at address %lx", addr);
#endif
            break;

        default:
            //value = usbRu32(addr);
#if 0
            g_plugin_log.WriteLn("(USBnull) 32 bit read at address %lx", addr);
#endif
            break;
    }
    return value;
}

EXPORT_C_(void)
USBwrite8(u32 addr, u8 value)
{
    switch (addr) {
        // Handle any appropriate addresses here.
        case 0x1f801600:
#if 0
            g_plugin_log.WriteLn("(USBnull) 8 bit write at address %lx value %x", addr, value);
#endif
            break;

        default:
            //usbRu8(addr) = value;
#if 0
            g_plugin_log.WriteLn("(USBnull) 8 bit write at address %lx value %x", addr, value);
#endif
            break;
    }
}

EXPORT_C_(void)
USBwrite16(u32 addr, u16 value)
{
    switch (addr) {
        // Handle any appropriate addresses here.
        case 0x1f801600:
#if 0
            g_plugin_log.WriteLn("(USBnull) 16 bit write at address %lx value %x", addr, value);
#endif
            break;

        default:
            //usbRu16(addr) = value;
#if 0
            g_plugin_log.WriteLn("(USBnull) 16 bit write at address %lx value %x", addr, value);
#endif
            break;
    }
}

EXPORT_C_(void)
USBwrite32(u32 addr, u32 value)
{
    switch (addr) {
        // Handle any appropriate addresses here.
        case 0x1f801600:
#if 0
            g_plugin_log.WriteLn("(USBnull) 16 bit write at address %lx value %x", addr, value);
#endif
            break;

        default:
            //usbRu32(addr) = value;
#if 0
            g_plugin_log.WriteLn("(USBnull) 32 bit write at address %lx value %x", addr, value);
#endif
            break;
    }
}

EXPORT_C_(void)
USBirqCallback(USBcallback callback)
{
    // Register USBirq, so we can trigger an interrupt with it later.
    // It will be called as USBirq(cycles); where cycles is the number
    // of cycles before the irq is triggered.
    USBirq = callback;
}

EXPORT_C_(int)
_USBirqHandler(void)
{
    // This is our USB irq handler, so if an interrupt gets triggered,
    // deal with it here.
    return 0;
}

EXPORT_C_(USBhandler)
USBirqHandler(void)
{
    // Pass our handler to pcsx2.
    return (USBhandler)_USBirqHandler;
}

EXPORT_C_(void)
USBsetRAM(void *mem)
{
    ram = (s8 *)mem;
}

// extended funcs

EXPORT_C_(void)
USBkeyEvent(keyEvent *ev)
{
}

EXPORT_C_(s32)
USBfreeze(int mode, freezeData *data)
{
    // This should store or retrieve any information, for if emulation
    // gets suspended, or for savestates.
    switch (mode) {
        case FREEZE_LOAD:
            // Load previously saved data.
            break;
        case FREEZE_SAVE:
            // Save data.
            break;
        case FREEZE_SIZE:
            // return the size of the data.
            break;
    }
    return 0;
}

EXPORT_C_(void) USBasync(u32 cycles)
{
	// Optional function: Called in IopCounter.cpp.
}
