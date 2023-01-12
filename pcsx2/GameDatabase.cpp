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

#include "GameDatabase.h"

#include "yaml-cpp/yaml.h"
#include <algorithm>
#include <cctype>
#include <wx/string.h>

#include "Config.h"
#include "../libretro/options_tools.h"

extern const wxChar* tbl_SpeedhackNames[];
extern const wxChar *tbl_GamefixNames[];

static std::string strToLower(std::string str)
{
	std::transform(str.begin(), str.end(), str.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return str;
}

static std::string join(const std::vector<std::string> & v, const std::string & delimiter = "/")
{
	std::string out;
	/* TODO/FIXME - C++17 - downgrade this to C++11 */
	if (auto i = v.begin(), e = v.end(); i != e) {
		out += *i++;
		for (; i != e; ++i) out.append(delimiter).append(*i);
	}
	return out;
}

std::string GameDatabaseSchema::GameEntry::memcardFiltersAsString() const
{
	return join(memcardFilters);
}

bool GameDatabaseSchema::GameEntry::findPatch(const std::string crc, Patch& patch) const
{
	std::string crcLower = strToLower(crc);
	log_cb(RETRO_LOG_INFO, "[GameDB] Searching for patch with CRC '%s'\n", crc.c_str());
	if (patches.count(crcLower) == 1)
	{
		log_cb(RETRO_LOG_INFO, "[GameDB] Found patch with CRC '%s'\n", crc.c_str());
		patch = patches.at(crcLower);
		return true;
	} else if (patches.count("default") == 1)
	{
		log_cb(RETRO_LOG_INFO, "[GameDB] Found and falling back to default patch\n");
		patch = patches.at("default");
		return true;
	}
	log_cb(RETRO_LOG_INFO, "[GameDB] No CRC-specific patch or default patch found\n");
	return false;
}

static std::vector<std::string> convertMultiLineStringToVector(const std::string multiLineString)
{
	std::vector<std::string> lines;
	std::istringstream stream(multiLineString);
	std::string line;
	while (std::getline(stream, line))
		lines.push_back(line);
	return lines;
}

static GameDatabaseSchema::GameEntry entryFromYaml(const std::string serial, const YAML::Node& node)
{
	GameDatabaseSchema::GameEntry gameEntry;
	try
	{
		gameEntry.name = node["name"].as<std::string>("");
		gameEntry.region = node["region"].as<std::string>("");
		gameEntry.compat = static_cast<GameDatabaseSchema::Compatibility>(node["compat"].as<int>(enum_cast(gameEntry.compat)));
		// Safely grab round mode and clamp modes from the YAML, otherwise use defaults
		if (YAML::Node roundModeNode = node["roundModes"])
		{
			gameEntry.eeRoundMode = static_cast<GameDatabaseSchema::RoundMode>(roundModeNode["eeRoundMode"].as<int>(enum_cast(gameEntry.eeRoundMode)));
			gameEntry.vuRoundMode = static_cast<GameDatabaseSchema::RoundMode>(roundModeNode["vuRoundMode"].as<int>(enum_cast(gameEntry.vuRoundMode)));
		}
		if (YAML::Node clampModeNode = node["clampModes"])
		{
			gameEntry.eeClampMode = static_cast<GameDatabaseSchema::ClampMode>(clampModeNode["eeClampMode"].as<int>(enum_cast(gameEntry.eeClampMode)));
			gameEntry.vuClampMode = static_cast<GameDatabaseSchema::ClampMode>(clampModeNode["vuClampMode"].as<int>(enum_cast(gameEntry.vuClampMode)));
		}

		// Validate game fixes, invalid ones will be dropped!
		for (std::string& fix : node["gameFixes"].as<std::vector<std::string>>(std::vector<std::string>()))
		{
			bool fixValidated = false;
			for (GamefixId id = GamefixId_FIRST; id < pxEnumEnd; id++)
			{
				const wxChar *str    = tbl_GamefixNames[id];
				std::string validFix = wxString(str).ToStdString() + "Hack";
				if (validFix == fix)
				{
					fixValidated = true;
					break;
				}
			}
			if (fixValidated)
			{
				gameEntry.gameFixes.push_back(fix);
			} else
			{
				log_cb(RETRO_LOG_ERROR, "[GameDB] Invalid gamefix: '%s', specified for serial: '%s'. Dropping!\n", fix.c_str(), serial.c_str());
			}
		}

		// Validate speed hacks, invalid ones will be dropped!
		if (YAML::Node speedHacksNode = node["speedHacks"])
		{
			for (const auto& entry : speedHacksNode)
			{
				std::string speedHack = entry.first.as<std::string>();
				bool speedHackValidated = false;
				for (SpeedhackId id = SpeedhackId_FIRST; id < pxEnumEnd; id++)
				{
					const wxChar *str         = tbl_SpeedhackNames[id];
					std::string validSpeedHack = wxString(str).ToStdString() + "SpeedHack";
					if (validSpeedHack == speedHack)
					{
						speedHackValidated = true;
						break;
					}
				}
				if (speedHackValidated)
				{
					gameEntry.speedHacks[speedHack] = entry.second.as<int>();
				} else
				{
					log_cb(RETRO_LOG_ERROR, "[GameDB] Invalid speedhack: '%s', specified for serial: '%s'. Dropping!\n", speedHack.c_str(), serial.c_str());
				}
			}
		}

		gameEntry.memcardFilters = node["memcardFilters"].as<std::vector<std::string>>(std::vector<std::string>());

		if (YAML::Node patches = node["patches"])
		{
			for (const auto& entry : patches)
			{
				std::string crc = strToLower(entry.first.as<std::string>());
				if (gameEntry.patches.count(crc) == 1)
				{
					log_cb(RETRO_LOG_ERROR, "[GameDB] Duplicate CRC '{}' found for serial: '{}'. Skipping, CRCs are case-insensitive!\n", crc.c_str(), serial.c_str());
					continue;
				}
				YAML::Node patchNode = entry.second;

				GameDatabaseSchema::Patch patchCol;

				patchCol.author = patchNode["author"].as<std::string>("");
				patchCol.patchLines = convertMultiLineStringToVector(patchNode["content"].as<std::string>(""));
				gameEntry.patches[crc] = patchCol;
			}
		}
	} catch (const YAML::RepresentationException& e)
	{
		log_cb(RETRO_LOG_ERROR, "[GameDB] Invalid GameDB syntax detected on serial: '%s'. Error Details - %s\n", serial.c_str(), e.msg.c_str());
		gameEntry.isValid = false;
	} catch (const std::exception& e)
	{
		log_cb(RETRO_LOG_ERROR, "[GameDB] Unexpected error occurred when reading serial: '%s'. Error Details - %s\n", serial.c_str(), e.what());
		gameEntry.isValid = false;
	}
	return gameEntry;
}

GameDatabaseSchema::GameEntry YamlGameDatabaseImpl::findGame(const std::string serial)
{
	std::string serialLower = strToLower(serial);
	log_cb(RETRO_LOG_INFO, "[GameDB] Searching for '%s' in GameDB\n", serialLower.c_str());
	if (gameDb.count(serialLower) == 1)
	{
		log_cb(RETRO_LOG_INFO, "[GameDB] Found '%s' in GameDB\n", serialLower.c_str());
		return gameDb[serialLower];
	}

	log_cb(RETRO_LOG_ERROR, "[GameDB] Could not find '%s' in GameDB\n", serialLower.c_str());
	GameDatabaseSchema::GameEntry entry;
	entry.isValid = false;
	return entry;
}

bool YamlGameDatabaseImpl::initDatabase(std::istream& stream)
{
	try
	{
		if (!stream)
		{
			log_cb(RETRO_LOG_ERROR, "[GameDB] Unable to open GameDB file.\n");
			return false;
		}
		// yaml-cpp has memory leak issues if you persist and modify a YAML::Node
		// convert to a map and throw it away instead!
		YAML::Node data = YAML::Load(stream);
		for (const auto& entry : data)
		{
			// we don't want to throw away the entire GameDB file if a single entry is made incorrectly,
			// but we do want to yell about it so it can be corrected
			try
			{
				// Serials and CRCs must be inserted as lower-case, as that is how they are retrieved
				// this is because the application may pass a lowercase CRC or serial along
				//
				// However, YAML's keys are as expected case-sensitive, so we have to explicitly do our own duplicate checking
				std::string serial = strToLower(entry.first.as<std::string>());
				if (gameDb.count(serial) == 1)
				{
					log_cb(RETRO_LOG_ERROR, "[GameDB] Duplicate serial '%s' found in GameDB. Skipping, Serials are case-insensitive!\n", serial.c_str());
					continue;
				}
				gameDb[serial] = entryFromYaml(serial, entry.second);
			} catch (const YAML::RepresentationException& e)
			{
				log_cb(RETRO_LOG_ERROR, "[GameDB] Invalid GameDB syntax detected. Error Details - %s\n", e.msg.c_str());
			}
		}
	} catch (const std::exception& e)
	{
		log_cb(RETRO_LOG_ERROR, "[GameDB] Error occured when initializing GameDB: %s\n", e.what());
		return false;
	}

	return true;
}
