/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014 David Quintana [gigaherz]
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

#include <stdio.h>

#ifndef __LIBRETRO__
#include <gtk/gtk.h>
#endif

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>

#include "Config.h"
#include "../DEV9.h"
#include "pcap.h"
#include "pcap_io.h"
#include "net.h"


void OnInitDialog() {
    static int initialized = 0;

    LoadConf();

    if( initialized )
        return;

    initialized = 1;
}

void OnOk() {

    SaveConf();

}

EXPORT_C_(void)
DEV9configure() {

  LoadConf();
  SaveConf();

}

NetAdapter* GetNetAdapter()
{
    NetAdapter* na;
    na = new PCAPAdapter();

    if (!na->isInitialised())
    {
            delete na;
            return 0;
    }
    return na;
}
s32  _DEV9open()
{
    NetAdapter* na=GetNetAdapter();
    if (!na)
    {
        emu_printf("Failed to GetNetAdapter()\n");
        config.ethEnable = false;
    }
    else
    {
        InitNet(na);
    }
    return 0;
}

void _DEV9close() {
    TermNet();
}
