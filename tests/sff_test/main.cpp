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

#include <iostream>
#include <fstream>
#include <string>
#include "common/typedefs.h"
#include "log/default_log.h"
#include "log/log_std_stream.h"
#include "log/default_log_levels.h"
#include "simple_file_format/sff_write_file.h"
#include "simple_file_format/sff_write_array.h"
#include "simple_file_format/sff_write_array.h"
#include "data/project.h"
#include "mysql_wrapper/mysql_connection.h"
#include "mysql_wrapper/mysql_row.h"
#include "mysql_wrapper/mysql_select.h"
#include "mysql_wrapper/mysql_statement.h"
using namespace wowpp;

#if 0
struct EquipmentEntry
{
	const ItemEntry *mainHand, *offHand, *ranged;

	EquipmentEntry()
		: mainHand(nullptr)
		, offHand(nullptr)
		, ranged(nullptr)
	{
	}
};

bool importEquipment(Project &project, MySQL::Connection &connection)
{
	// Load equipment entries
	std::map<UInt32, EquipmentEntry> equipmentEntries;
	{
		wowpp::MySQL::Select select(connection, "SELECT * FROM `creature_equip_template` ORDER BY entry;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				UInt32 entryId = 0;
				row.getField(0, entryId);

				EquipmentEntry &entry = equipmentEntries[entryId];

				UInt32 equipEntry = 0;
				row.getField(1, equipEntry);
				entry.mainHand = proj.items.getById(equipEntry);
				if (equipEntry != 0 && entry.mainHand == nullptr) WLOG("Could not find item with entry " << equipEntry);

				row.getField(2, equipEntry);
				entry.offHand = proj.items.getById(equipEntry);
				if (equipEntry != 0 && entry.offHand == nullptr) WLOG("Could not find item with entry " << equipEntry);

				row.getField(3, equipEntry);
				entry.ranged = proj.items.getById(equipEntry);
				if (equipEntry != 0 && entry.ranged == nullptr) WLOG("Could not find item with entry " << equipEntry);

				row = row.next(select);
			}
		}
		else
		{
			// There was an error
			ELOG(connection.getErrorMessage());
			return false;
		}
	}

	// TODO: Handle raw entries

	// Load unit equipment
	{
		wowpp::MySQL::Select select2(connection, "SELECT entry, equipment_id FROM `creature_template` ORDER BY entry;");
		if (select2.success())
		{
			wowpp::MySQL::Row row2(select2);
			while (row2)
			{
				UInt32 entryId = 0;
				row2.getField(0, entryId);

				UInt32 equipmentId = 0;
				row2.getField(1, equipmentId);

				auto *unit = proj.units.getEditableById(entryId);
				if (unit)
				{
					if (equipmentId == 0)
					{
						unit->mainHand = nullptr;
						unit->offHand = nullptr;
						unit->ranged = nullptr;
					}
					else
					{
						const auto &equip = equipmentEntries[equipmentId];
						unit->mainHand = equip.mainHand;
						unit->offHand = equip.offHand;
						unit->ranged = equip.ranged;
					}
				}

				row2 = row2.next(select2);
			}
		}
		else
		{
			// There was an error
			ELOG(connection.getErrorMessage());
			return false;
		}
	}

	return true;
}

bool importExplorationBaseXP(Project &project, MySQL::Connection &connection)
{
	wowpp::MySQL::Select select(connection, "SELECT `level`, `basexp` FROM `exploration_basexp` WHERE `level` > 0 AND `level` < 71 ORDER BY `level`;");
	if (select.success())
	{
		wowpp::MySQL::Row row(select);
		while (row)
		{
			UInt32 level = 0, baseXp = 0;
			row.getField(0, level);
			row.getField(1, baseXp);

			// Find level
			auto *levelEntry = project.levels.getEditableById(level);
			if (!levelEntry)
			{
				WLOG("Could not find entry for level " << level << " - skipping");
			}
			else
			{
				levelEntry->explorationBaseXP = baseXp;
			}

			row = row.next(select);
		}
	}
	else
	{
		// There was an error
		ELOG(connection.getErrorMessage());
		return false;
	}

	return true;
}

bool importCreatureTypes(Project &project, MySQL::Connection &connection)
{
	wowpp::MySQL::Select select(connection, "SELECT `entry`, `type` FROM `creature_template`;");
	if (select.success())
	{
		wowpp::MySQL::Row row(select);
		while (row)
		{
			UInt32 entry = 0, type = 0;
			row.getField(0, entry);
			row.getField(1, type);

			// Find unit
			auto *unitEntry = project.units.getEditableById(entry);
			if (!unitEntry)
			{
				WLOG("Could not find entry for unit " << entry << " - skipping");
			}
			else
			{
				unitEntry->type = type;
			}

			row = row.next(select);
		}
	}
	else
	{
		// There was an error
		ELOG(connection.getErrorMessage());
		return false;
	}

	return true;
}
#endif


bool importCreatureAttackPower(Project &project, MySQL::Connection &connection)
{
	wowpp::MySQL::Select select(connection, "SELECT `entry`, `attackpower`, `rangedattackpower` FROM `creature_template`;");
	if (select.success())
	{
		wowpp::MySQL::Row row(select);
		while (row)
		{
			UInt32 entry = 0, atk = 0, rng_atk = 0;
			row.getField(0, entry);
			row.getField(1, atk);
			row.getField(2, rng_atk);

			// Find unit
			auto *unitEntry = project.units.getEditableById(entry);
			if (!unitEntry)
			{
				WLOG("Could not find entry for unit " << entry << " - skipping");
			}
			else
			{
				unitEntry->attackPower = atk;
				unitEntry->rangedAttackPower = rng_atk;
			}

			row = row.next(select);
		}
	}
	else
	{
		// There was an error
		ELOG(connection.getErrorMessage());
		return false;
	}

	return true;
}

bool fixCreatureDamage(Project &project)
{
	static std::vector<UInt32> excluded = { 16876, 19295, 16978, 19408, 19527, 16933, 16928, 16932, 16977, 16901, 19457, 19443, 17058, 16903, 19413, 19419, 26223, 16954, 26216, 19442, 19420, 18945, 18944, 16974, 19305, 16844, 19188, 19397, 22323, 19298, 19299, 21504, 18678, 16964, 19264, 19415, 19422, 16867, 19349, 16880, 19410, 16871, 16947, 16906, 18978, 19263, 16904, 20599, 16950, 16967, 16905, 19312, 16937, 17014, 16975, 19190, 19424, 16973, 19282, 19399, 19398, 16972, 18976, 18952, 16907, 18981, 16929, 18827, 17053, 16863, 19460, 16857, 17035, 17000, 19005, 24919, 16870, 16878, 19423, 16925, 17034, 19414, 18679, 18706, 17039, 19136, 19335, 19640, 19192, 16966, 16968, 19458, 19411, 17084, 16873, 19701, 22259, 19440, 16951, 20798, 22454, 19354, 16934, 16938, 20145, 18977, 21134, 16946, 22273, 18975, 16879, 19434, 19350, 19191, 20160, 16959, 19261, 18677, 19259, 17057, 18946, 24918, 22461, 16960, 19459, 18728, 19400, 26222, 19189, 16927, 17281, 17270, 17307, 17308, 17478, 17264, 17306, 17540, 17280, 17455, 17271, 17309, 17517, 17537, 17269, 17259 };
	for (auto &unit : project.units.getTemplates())
	{
		bool isExcluded = false;
		for (UInt32 &index : excluded)
		{
			if (unit->id == index)
			{
				isExcluded = true;
				break;;
			}
		}

		if (isExcluded)
		{
			continue;
		}

		unit->minMeleeDamage *= 1.1f;
		unit->maxMeleeDamage *= 1.1f;
	}

	return true;
}

/// Procedural entry point of the application.
int main(int argc, char* argv[])
{
	// Add cout to the list of log output streams
	wowpp::g_DefaultLog.signal().connect(std::bind(
		wowpp::printLogEntry,
		std::ref(std::cout), std::placeholders::_1, wowpp::g_DefaultConsoleLogOptions));

	// Database connection
	MySQL::DatabaseInfo connectionInfo("127.0.0.1", 3306, "root", "", "tbcdb");
	MySQL::Connection connection;
	if (!connection.connect(connectionInfo))
	{
		ELOG("Could not connect to the database");
		ELOG(connection.getErrorMessage());
		return 0;
	}

	Project proj;
	if (!proj.load("C:/Source/wowpp-data"))
	{
		return 1;
	}
	
	fixCreatureDamage(proj);
	/*if (!importCreatureAttackPower(proj, connection))
	{
		return 1;
	}*/

	proj.save("./test-data");

	return 0;
}
