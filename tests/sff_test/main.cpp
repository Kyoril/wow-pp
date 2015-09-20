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
#endif


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
	if (!proj.load("C:/Source/wowpp/data"))
	{
		return 1;
	}
	
	if (!importCreatureTypes(proj, connection))
	{
		return 1;
	}

	proj.save("./test-data");

	return 0;
}
