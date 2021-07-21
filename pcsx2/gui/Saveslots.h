/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2018  PCSX2 Dev Team
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

#include "PS2Edefs.h"
#include "System.h"
#include "Elfheader.h"
#include "App.h"
#include <array>

// Uncomment to turn on the new saveslot UI.
#define USE_NEW_SAVESLOTS_UI

// Uncomment to turn on the extra UI updates *without* the UI.
//#define USE_SAVESLOT_UI_UPDATES

#ifdef USE_NEW_SAVESLOTS_UI
// Should always be enabled if the new saveslots are.
#define USE_SAVESLOT_UI_UPDATES
#endif

#ifdef USE_SAVESLOT_UI_UPDATES
// Uncomment to add saveslot logging and comment to turn off.
//#define SAVESLOT_LOGS
#endif

extern wxString DiscSerial;
static const int StateSlotsCount = 10;

class Saveslot
{
public:
	u32 slot_num;
	bool empty;
	wxDateTime updated;
	u32 crc;
	wxString serialName;
	bool menu_update, invalid_cache;
	int load_item_id, save_item_id;

	Saveslot() = delete;

	Saveslot(int i)
	{
		slot_num = i;
		empty = true;
		updated = wxInvalidDateTime;
		crc = 0;
		serialName =  L"";
		menu_update = false;
		invalid_cache = true;
		load_item_id = MenuId_State_Load01 + i + 1;
		save_item_id = MenuId_State_Save01 + i + 1;
	}

	bool isUsed()
	{
		return wxFileExists(SaveStateBase::GetFilename(slot_num));
	}

	wxDateTime GetTimestamp()
	{
		if (!isUsed()) return wxInvalidDateTime;

		return wxDateTime(wxFileModificationTime(SaveStateBase::GetFilename(slot_num)));
	}

	void UpdateCache()
	{
		empty = !isUsed();
		updated = GetTimestamp();
		crc = ElfCRC;
		serialName = DiscSerial;
		invalid_cache = false;
	}

	wxString SlotName()
	{
		if (empty) return wxsFormat(_("Slot %d - Empty"), slot_num);

		if (updated.IsValid()) return wxsFormat(_("Slot %d - %s %s"), slot_num, updated.FormatDate(), updated.FormatTime());

		return wxsFormat(_("Slot %d - Unknown Time"), slot_num);
	}

	void Used()
	{
		// Update the saveslot cache with the new saveslot, and give it the current timestamp, 
		// Because we aren't going to be able to get the real timestamp from disk right now.
		empty = false;
		updated = wxDateTime::Now();
		crc = ElfCRC;

		// Update the slot next time we run through the UI update.
		menu_update = true;
	}
};

extern std::array<Saveslot,10> saveslot_cache;
extern void States_DefrostCurrentSlotBackup();
extern void States_DefrostCurrentSlot();
extern void States_FreezeCurrentSlot();
extern void States_CycleSlotForward();
extern void States_CycleSlotBackward();
extern void States_SetCurrentSlot(int slot);
extern int States_GetCurrentSlot();
extern void Sstates_updateLoadBackupMenuItem(bool isBeforeSave);
