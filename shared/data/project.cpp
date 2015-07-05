//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software 
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
// 

#include "project.h"
#include "data_load_context.h"
#include "templates/project_loader.h"
#include "templates/project_saver.h"
#include "virtual_directory/file_system_reader.h"

namespace wowpp
{
	Project::Project()
	{
	}

	bool Project::load(
	    const String &directory)
	{
		ILOG("Loading data...");

		size_t errorCount = 0;

		const boost::filesystem::path dataPath = directory;
		const auto realmDataPath = (dataPath / "wowpp");

		DataLoadContext context(realmDataPath);
		context.dataPath = realmDataPath;

		context.onError = [&errorCount](const String & message)
		{
			ELOG(message);
			++errorCount;
		};

		context.getMap = std::bind(&MapEntryManager::getById, &maps, std::placeholders::_1);
		context.getRace = std::bind(&RaceEntryManager::getById, &races, std::placeholders::_1);
		context.getClass = std::bind(&ClassEntryManager::getById, &classes, std::placeholders::_1);
		context.getCreatureType = std::bind(&CreatureTypeEntryManager::getById, &creaturetypes, std::placeholders::_1);
		context.getUnit = std::bind(&UnitEntryManager::getById, &units, std::placeholders::_1);

		typedef ProjectLoader<DataLoadContext> RealmProjectLoader;
		typedef RealmProjectLoader::ManagerEntry ManagerEntry;

		RealmProjectLoader::Managers managers;
		managers.push_back(ManagerEntry("cast_times", castTimes, std::bind(&CastTimeEntry::load, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
		managers.push_back(ManagerEntry("spells", spells, std::bind(&SpellEntry::load, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
		managers.push_back(ManagerEntry("races", races, std::bind(&RaceEntry::load, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
		managers.push_back(ManagerEntry("classes", classes, std::bind(&ClassEntry::load, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
		managers.push_back(ManagerEntry("levels", levels, std::bind(&LevelEntry::load, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
		managers.push_back(ManagerEntry("creature_types", creaturetypes, std::bind(&CreatureTypeEntry::load, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
		managers.push_back(ManagerEntry("units", units, std::bind(&UnitEntry::load, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
		managers.push_back(ManagerEntry("maps", maps, std::bind(&MapEntry::load, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));

		virtual_dir::FileSystemReader virtualDirectory(context.dataPath);
		if (!RealmProjectLoader::load(
		            virtualDirectory,
		            managers,
		            context))
		{
			ELOG("Game data error count: " << errorCount << "+");
			return false;
		}

		ILOG("Game data loaded");
		return true;
	}

	bool Project::save(const String &directory)
	{
		ILOG("Saving data...");

		size_t errorCount = 0;

		const boost::filesystem::path dataPath = directory;
		const auto realmDataPath = (dataPath / "wowpp");

		typedef ProjectSaver RealmProjectSaver;
		typedef ProjectSaver::Manager ManagerEntry;

		RealmProjectSaver::Managers managers;
		managers.push_back(ManagerEntry("cast_times", "cast_times", castTimes, &CastTimeEntry::save));
		managers.push_back(ManagerEntry("spells", "spells", spells, &SpellEntry::save));
		managers.push_back(ManagerEntry("maps", "maps", maps, &MapEntry::save));
		managers.push_back(ManagerEntry("levels", "levels", levels, &LevelEntry::save));
		managers.push_back(ManagerEntry("races", "races", races, &RaceEntry::save));
		managers.push_back(ManagerEntry("classes", "classes", classes, &ClassEntry::save));
		managers.push_back(ManagerEntry("creature_types", "creature_types", creaturetypes, &CreatureTypeEntry::save));
		managers.push_back(ManagerEntry("units", "units", units, &UnitEntry::save));

		if (!RealmProjectSaver::save(realmDataPath, managers))
		{
			ELOG("Could not save data project!");
			return false;
		}

		ILOG("Game data saved");
		return true;
	}

}
