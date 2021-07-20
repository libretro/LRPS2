/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2018 PCSX2 Dev Team
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

#include "PS2Eext.h"
#if defined(_WIN32)
#include <windows.h>
#include "resource.h"
#elif defined(__unix__) || defined(__APPLE__)
#include <wx/wx.h>
#endif
#include <string>

static PluginLog g_plugin_log;

static void SaveConfig(const std::string &pathname)
{
    PluginConf ini;
    if (!ini.Open(pathname, WRITE_FILE)) {
        g_plugin_log.WriteLn("Failed to open %s", pathname.c_str());
        return;
    }

    ini.WriteInt("write_to_console", g_plugin_log.WriteToConsole);
    ini.WriteInt("write_to_file", g_plugin_log.WriteToFile);
    ini.Close();
}

static void LoadConfig(const std::string &pathname)
{
    PluginConf ini;
    if (!ini.Open(pathname, READ_FILE)) {
        g_plugin_log.WriteLn("Failed to open %s", pathname.c_str());
        SaveConfig(pathname);
        return;
    }

    g_plugin_log.WriteToConsole = ini.ReadInt("write_to_console", 0) != 0;
    g_plugin_log.WriteToFile = ini.ReadInt("write_to_file", 0) != 0;
    ini.Close();
}
