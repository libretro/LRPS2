/*  OnePAD - author: arcum42(@gmail.com)
 *  Copyright (C) 2009
 *
 *  Based on ZeroPAD, author zerofrog@gmail.com
 *  Copyright (C) 2006-2007
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>

#include "onepad.h"
#include "state_management.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

const u32 version = PS2E_PAD_VERSION;
const u32 revision = 2;
const u32 build = 0; // increase that with each version
#define PAD_SAVE_STATE_VERSION ((revision << 8) | (build << 0))

KeyStatus g_key_status;

EXPORT_C_(s32)
PADinit(u32 flags)
{
    Pad::reset_all();

    query.reset();

    for (int port = 0; port < 2; port++)
       slots[port] = 0;

    return 0;
}

EXPORT_C_(void)
PADshutdown()
{
}

EXPORT_C_(s32)
PADopen(void *pDsp)
{
    return _PADopen(pDsp);
}

EXPORT_C_(void)
PADsetSettingsDir(const char *dir) { }

EXPORT_C_(void)
PADsetLogDir(const char *dir) { }

EXPORT_C_(void)
PADclose()
{
    _PADclose();
}

EXPORT_C_(u32)
PADquery()
{
    return 3; // both
}

EXPORT_C_(s32)
PADsetSlot(u8 port, u8 slot)
{
    port--;
    slot--;
    if (port > 1 || slot > 3)
        return 0;
    // Even if no pad there, record the slot, as it is the active slot regardless.
    slots[port] = slot;

    return 1;
}

EXPORT_C_(s32)
PADqueryMtap(u8 port)
{
   return 0;
}

EXPORT_C_(s32)
PADfreeze(int mode, freezeData *data)
{
    if (!data)
        return -1;

    if (mode == FREEZE_SIZE)
	    data->size = sizeof(PadPluginFreezeData);
    else if (mode == FREEZE_LOAD)
    {
	    PadPluginFreezeData *pdata = (PadPluginFreezeData *)(data->data);

	    Pad::stop_vibrate_all();

	    if (data->size != sizeof(PadPluginFreezeData) || pdata->version != PAD_SAVE_STATE_VERSION ||
			    strncmp(pdata->format, "OnePad", sizeof(pdata->format)))
		    return 0;

	    query = pdata->query;
	    if (pdata->query.slot < 4) {
		    query = pdata->query;
	    }

	    // Tales of the Abyss - pad fix
	    // - restore data for both ports
	    for (int port = 0; port < 2; port++)
	    {
		    for (int slot = 0; slot < 4; slot++)
		    {
			    u8 mode = pdata->padData[port][slot].mode;

			    if (
					    mode != MODE_DIGITAL 
					    && mode != MODE_ANALOG 
					    && mode != MODE_DS2_NATIVE)
				    break;

			    memcpy(&pads[port][slot], &pdata->padData[port][slot], sizeof(PadFreezeData));
		    }

		    if (pdata->slot[port] < 4)
			    slots[port] = pdata->slot[port];
	    }

    }
    else if (mode == FREEZE_SAVE)
    {
	    if (data->size != sizeof(PadPluginFreezeData))
		    return 0;

	    PadPluginFreezeData *pdata = (PadPluginFreezeData *)(data->data);

	    // Tales of the Abyss - pad fix
	    // - PCSX2 only saves port0 (save #1), then port1 (save #2)

	    memset(pdata, 0, data->size);
	    strncpy(pdata->format, "OnePad", sizeof(pdata->format));
	    pdata->version = PAD_SAVE_STATE_VERSION;
	    pdata->query = query;

	    for (int port = 0; port < 2; port++)
	    {
		    for (int slot = 0; slot < 4; slot++)
			    pdata->padData[port][slot] = pads[port][slot];

		    pdata->slot[port] = slots[port];
	    }

    }
    else
        return -1;

    return 0;
}

EXPORT_C_(u8)
PADstartPoll(int pad)
{
    return pad_start_poll(pad);
}

EXPORT_C_(u8)
PADpoll(u8 value)
{
    return pad_poll(value);
}

// PADkeyEvent is called every vsync (return NULL if no event)
EXPORT_C_(keyEvent *)
PADkeyEvent()
{
    return NULL;
}
EXPORT_C_(void)
PADWriteEvent(keyEvent &evt)
{
}
