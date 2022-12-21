/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "GameDatabase.h"

#include "AppConfig.h"

class AppGameDatabase : public YamlGameDatabaseImpl
{
public:
	AppGameDatabase() {}
	virtual ~AppGameDatabase() { }
	AppGameDatabase& Load();
};

static wxString compatToStringWX(GameDatabaseSchema::Compatibility compat)
{
	switch (compat)
	{
	case GameDatabaseSchema::Compatibility::Perfect:
		return L"Perfect";
	case GameDatabaseSchema::Compatibility::Playable:
		return L"Playable";
	case GameDatabaseSchema::Compatibility::InGame:
		return L"In-Game";
	case GameDatabaseSchema::Compatibility::Menu:
		return L"Menu";
	case GameDatabaseSchema::Compatibility::Intro:
		return L"Intro";
	case GameDatabaseSchema::Compatibility::Nothing:
		return L"Nothing";
	default:
		return L"Unknown";
	}
}
