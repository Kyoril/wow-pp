//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
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

#pragma once

#include "project_loader.h"
#include "project_saver.h"
#include "proto_template.h"
#include "log/default_log_levels.h"
#include "virtual_directory/file_system_reader.h"
#include "common/clock.h"
#include "shared/proto_data/units.pb.h"
#include "shared/proto_data/spells.pb.h"
#include "shared/proto_data/unit_loot.pb.h"
#include "shared/proto_data/maps.pb.h"
#include "shared/proto_data/emotes.pb.h"
#include "shared/proto_data/objects.pb.h"
#include "shared/proto_data/skills.pb.h"
#include "shared/proto_data/talents.pb.h"
#include "shared/proto_data/vendors.pb.h"
#include "shared/proto_data/trainers.pb.h"
#include "shared/proto_data/triggers.pb.h"
#include "shared/proto_data/zones.pb.h"
#include "shared/proto_data/quests.pb.h"
#include "shared/proto_data/items.pb.h"
#include "shared/proto_data/races.pb.h"
#include "shared/proto_data/classes.pb.h"
#include "shared/proto_data/levels.pb.h"

namespace wowpp
{
	namespace proto
	{
		typedef TemplateManager<wowpp::proto::Objects, wowpp::proto::ObjectEntry> ObjectManager;
		typedef TemplateManager<wowpp::proto::Units, wowpp::proto::UnitEntry> UnitManager;
		typedef TemplateManager<wowpp::proto::Maps, wowpp::proto::MapEntry> MapManager;
		typedef TemplateManager<wowpp::proto::Emotes, wowpp::proto::EmoteEntry> EmoteManager;
		typedef TemplateManager<wowpp::proto::UnitLoot, wowpp::proto::LootEntry> UnitLootManager;
		typedef TemplateManager<wowpp::proto::Spells, wowpp::proto::SpellEntry> SpellManager;
		typedef TemplateManager<wowpp::proto::Skills, wowpp::proto::SkillEntry> SkillManager;
		typedef TemplateManager<wowpp::proto::Trainers, wowpp::proto::TrainerEntry> TrainerManager;
		typedef TemplateManager<wowpp::proto::Vendors, wowpp::proto::VendorEntry> VendorManager;
		typedef TemplateManager<wowpp::proto::Talents, wowpp::proto::TalentEntry> TalentManager;
		typedef TemplateManager<wowpp::proto::Items, wowpp::proto::ItemEntry> ItemManager;
		typedef TemplateManager<wowpp::proto::Classes, wowpp::proto::ClassEntry> ClassManager;
		typedef TemplateManager<wowpp::proto::Races, wowpp::proto::RaceEntry> RaceManager;
		typedef TemplateManager<wowpp::proto::Levels, wowpp::proto::LevelEntry> LevelManager;
		typedef TemplateManager<wowpp::proto::Triggers, wowpp::proto::TriggerEntry> TriggerManager;
		typedef TemplateManager<wowpp::proto::Zones, wowpp::proto::ZoneEntry> ZoneManager;
		typedef TemplateManager<wowpp::proto::Quests, wowpp::proto::QuestEntry> QuestManager;

		struct Project : boost::noncopyable
		{
			// Managers
			ObjectManager objects;
			UnitManager units;
			MapManager maps;
			EmoteManager emotes;
			UnitLootManager unitLoot;
			SpellManager spells;
			SkillManager skills;
			TrainerManager trainers;
			VendorManager vendors;
			TalentManager talents;
			ItemManager items;
			ClassManager classes;
			RaceManager races;
			LevelManager levels;
			TriggerManager triggers;
			ZoneManager zones;
			QuestManager quests;

		public:

			/// Loads the project.
			bool load(
				const String &directory)
			{
				ILOG("Loading data...");
				auto loadStart = getCurrentTime();

				size_t errorCount = 0;
				DataLoadContext context;
				context.onError = [&errorCount](const String & message)
				{
					ELOG(message);
					++errorCount;
				};
				context.onWarning = [](const String & message)
				{
					WLOG(message);
				};

				const boost::filesystem::path dataPath = directory;
				const auto realmDataPath = (dataPath / "wowpp");

				typedef ProjectLoader<DataLoadContext> RealmProjectLoader;
				typedef RealmProjectLoader::ManagerEntry ManagerEntry;

				RealmProjectLoader::Managers managers;
				managers.push_back(ManagerEntry("spells", spells));
				managers.push_back(ManagerEntry("units", units));
				managers.push_back(ManagerEntry("objects", objects));
				managers.push_back(ManagerEntry("maps", maps));
				managers.push_back(ManagerEntry("emotes", emotes));
				managers.push_back(ManagerEntry("unit_loot", unitLoot));
				managers.push_back(ManagerEntry("skills", skills));
				managers.push_back(ManagerEntry("trainers", trainers));
				managers.push_back(ManagerEntry("vendors", vendors));
				managers.push_back(ManagerEntry("talents", talents));
				managers.push_back(ManagerEntry("items", items));
				managers.push_back(ManagerEntry("classes", classes));
				managers.push_back(ManagerEntry("races", races));
				managers.push_back(ManagerEntry("levels", levels));
				managers.push_back(ManagerEntry("triggers", triggers));
				managers.push_back(ManagerEntry("zones", zones));
				managers.push_back(ManagerEntry("quests", quests));

				virtual_dir::FileSystemReader virtualDirectory(realmDataPath);
				if (!RealmProjectLoader::load(
					virtualDirectory,
					managers,
					context))
				{
					ELOG("Game data error count: " << errorCount << "+");
					return false;
				}

				auto loadEnd = getCurrentTime();
				ILOG("Loading finished in " << (loadEnd - loadStart) << "ms");

				return true;
			}
			/// Saves the project.
			bool save(
				const String &directory)
			{
				ILOG("Saving data...");
				auto saveStart = getCurrentTime();

				size_t errorCount = 0;

				const boost::filesystem::path dataPath = directory;
				const auto realmDataPath = (dataPath / "wowpp");

				typedef ProjectSaver RealmProjectSaver;
				typedef ProjectSaver::Manager ManagerEntry;

				RealmProjectSaver::Managers managers;
				managers.push_back(ManagerEntry("spells", "spells", spells));
				managers.push_back(ManagerEntry("units", "units", units));
				managers.push_back(ManagerEntry("objects", "objects", objects));
				managers.push_back(ManagerEntry("maps", "maps", maps));
				managers.push_back(ManagerEntry("emotes", "emotes", emotes));
				managers.push_back(ManagerEntry("unit_loot", "unit_loot", unitLoot));
				managers.push_back(ManagerEntry("skills", "skills", skills));
				managers.push_back(ManagerEntry("trainers", "trainers", trainers));
				managers.push_back(ManagerEntry("vendors", "vendors", vendors));
				managers.push_back(ManagerEntry("talents", "talents", talents));
				managers.push_back(ManagerEntry("items", "items", items));
				managers.push_back(ManagerEntry("classes", "classes", classes));
				managers.push_back(ManagerEntry("races", "races", races));
				managers.push_back(ManagerEntry("levels", "levels", levels));
				managers.push_back(ManagerEntry("triggers", "triggers", triggers));
				managers.push_back(ManagerEntry("zones", "zones", zones));
				managers.push_back(ManagerEntry("quests", "quests", quests));

				if (!RealmProjectSaver::save(realmDataPath, managers))
				{
					ELOG("Could not save data project!");
					return false;
				}

				auto saveEnd = getCurrentTime();
				ILOG("Saving finished in " << (saveEnd - saveStart) << "ms");
				return true;
			}
		};
	}
}
