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
#include "log/log_std_stream.h"
#include "log/log_entry.h"
#include "log/default_log_levels.h"
#include "simple_file_format/sff_write_file.h"
#include "simple_file_format/sff_write_array.h"
#include "simple_file_format/sff_write_array.h"
#include "simple_file_format/sff_write.h"
#include "simple_file_format/sff_read_tree.h"
#include "simple_file_format/sff_load_file.h"
#include "data/project.h"
#include "mysql_wrapper/mysql_connection.h"
#include "mysql_wrapper/mysql_row.h"
#include "mysql_wrapper/mysql_select.h"
#include "mysql_wrapper/mysql_statement.h"
#include "common/make_unique.h"
#include "common/constants.h"
#include <fstream>
using namespace wowpp;


namespace wowpp
{
	/// Manages the realm server configuration.
	struct Configuration
	{
		/// Config file version: used to detect new configuration files
		static const UInt32 ImporterConfigVersion = 0x01;

		/// Path to the client data
		String dataPath;

		/// The port to be used for a mysql connection.
		NetPort mysqlPort;
		/// The mysql server host address (ip or dns).
		String mysqlHost;
		/// The mysql user to be used.
		String mysqlUser;
		/// The mysql user password to be used.
		String mysqlPassword;
		/// The mysql database to be used.
		String mysqlDatabase;

		/// Indicates whether or not file logging is enabled.
		bool isLogActive;
		/// File name of the log file.
		String logFileName;
		/// If enabled, the log contents will be buffered before they are written to
		/// the file, which could be more efficient..
		bool isLogFileBuffering;

		/// Initializes a new instance of the Configuration class using the default
		/// values.
		explicit Configuration()
#if defined(WIN32) || defined(_WIN32)
			: dataPath("data")
#else
			: dataPath("/etc/wow-pp/data")
#endif
			, mysqlPort(wowpp::constants::DefaultMySQLPort)
			, mysqlHost("127.0.0.1")
			, mysqlUser("username")
			, mysqlPassword("password")
			, mysqlDatabase("database")
			, isLogActive(true)
			, logFileName("wowpp_extractor.log")
			, isLogFileBuffering(false)
		{
		}

		/// Loads the configuration values from a specific file.
		/// @param fileName Name of the configuration file.
		/// @returns true if the config file was loaded successfully, false otherwise.
		bool load(const String &fileName)
		{
			typedef String::const_iterator Iterator;
			typedef sff::read::tree::Table<Iterator> Table;

			Table global;
			std::string fileContent;

			std::ifstream file(fileName, std::ios::binary);
			if (!file)
			{
				if (save(fileName))
				{
					ILOG("Saved default settings as " << fileName);
				}
				else
				{
					ELOG("Could not save default settings as " << fileName);
				}

				return false;
			}

			try
			{
				sff::loadTableFromFile(global, fileContent, file);

				// Read config version
				UInt32 fileVersion = 0;
				if (!global.tryGetInteger("version", fileVersion) ||
					fileVersion != ImporterConfigVersion)
				{
					file.close();

					if (save(fileName))
					{
						ILOG("Saved updated settings with default values as " << fileName);
					}
					else
					{
						ELOG("Could not save updated default settings as " << fileName);
					}

					return false;
				}

				if (const Table *const mysqlDatabaseTable = global.getTable("mysqlDatabase"))
				{
					mysqlPort = mysqlDatabaseTable->getInteger("port", mysqlPort);
					mysqlHost = mysqlDatabaseTable->getString("host", mysqlHost);
					mysqlUser = mysqlDatabaseTable->getString("user", mysqlUser);
					mysqlPassword = mysqlDatabaseTable->getString("password", mysqlPassword);
					mysqlDatabase = mysqlDatabaseTable->getString("database", mysqlDatabase);
				}

				if (const Table *const log = global.getTable("log"))
				{
					isLogActive = log->getInteger("active", static_cast<unsigned>(isLogActive)) != 0;
					logFileName = log->getString("fileName", logFileName);
					isLogFileBuffering = log->getInteger("buffering", static_cast<unsigned>(isLogFileBuffering)) != 0;
				}

				if (const Table *const game = global.getTable("game"))
				{
					dataPath = game->getString("dataPath", dataPath);
				}
			}
			catch (const sff::read::ParseException<Iterator> &e)
			{
				const auto line = std::count<Iterator>(fileContent.begin(), e.position.begin, '\n');
				ELOG("Error in config: " << e.what());
				ELOG("Line " << (line + 1) << ": " << e.position.str());
				return false;
			}

			return true;
		}
		/// Saves the configuration values to a specific file. Overwrites the whole
		/// file!
		/// @param fileName Name of the configuration file.
		/// @returns true if the config file was saved successfully, false otherwise.
		bool save(const String &fileName)
		{
			std::ofstream file(fileName.c_str());
			if (!file)
			{
				return false;
			}

			typedef char Char;
			sff::write::File<Char> global(file, sff::write::MultiLine);

			// Save file version
			global.addKey("version", ImporterConfigVersion);
			global.writer.newLine();

			{
				sff::write::Table<Char> mysqlDatabaseTable(global, "mysqlDatabase", sff::write::MultiLine);
				mysqlDatabaseTable.addKey("port", mysqlPort);
				mysqlDatabaseTable.addKey("host", mysqlHost);
				mysqlDatabaseTable.addKey("user", mysqlUser);
				mysqlDatabaseTable.addKey("password", mysqlPassword);
				mysqlDatabaseTable.addKey("database", mysqlDatabase);
				mysqlDatabaseTable.finish();
			}

			global.writer.newLine();

			{
				sff::write::Table<Char> log(global, "log", sff::write::MultiLine);
				log.addKey("active", static_cast<unsigned>(isLogActive));
				log.addKey("fileName", logFileName);
				log.addKey("buffering", isLogFileBuffering);
				log.finish();
			}

			global.writer.newLine();

			{
				sff::write::Table<Char> game(global, "game", sff::write::MultiLine);
				game.addKey("dataPath", dataPath);
				game.finish();
			}

			return true;
		}
	};
}


bool importCreatureLoot(Project &project, MySQL::Connection &connection)
{
	// Clear unit loot
	ILOG("Deleting old loot entries...");
	for (auto &unit : project.units.getTemplates())
	{
		unit->unitLootEntry = nullptr;
	}
	project.unitLoot.clear();

	UInt32 lastEntry = 0;
	UInt32 lastGroup = 0;
	UInt32 groupIndex = 0;

	wowpp::MySQL::Select select(connection, "(SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount`, `active` FROM `wowpp_FERTIG_creatureloot` WHERE `lootcondition` = 0) "
		"UNION "
		"(SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount`, `active` FROM `wowpp_creature_loot_template` WHERE `lootcondition` = 0) "
		"ORDER BY `entry`, `groupid`;");
	if (select.success())
	{
		wowpp::MySQL::Row row(select);
		while (row)
		{
			UInt32 entry = 0, itemId = 0, groupId = 0, minCount = 0, maxCount = 0, active = 1;
			float dropChance = 0.0f;
			row.getField(0, entry);
			row.getField(1, itemId);
			row.getField(2, dropChance);
			row.getField(3, groupId);
			row.getField(4, minCount);
			row.getField(5, maxCount);
			row.getField(6, active);

			// Find referenced item
			const auto *itemEntry = project.items.getById(itemId);
			if (!itemEntry)
			{
				ELOG("Could not find referenced item " << itemId << " (referenced in creature loot entry " << entry << " - group " << groupId << ")");
				row = row.next(select);
				continue;
			}

			// Create a new loot entry
			bool created = false;
			if (entry > lastEntry)
			{
				std::unique_ptr<LootEntry> ptr = make_unique<LootEntry>();
				ptr->id = entry;
				project.unitLoot.add(std::move(ptr));

				lastEntry = entry;
				lastGroup = groupId;
				groupIndex = 0;
				created = true;
			}

			auto *lootEntry = project.unitLoot.getEditableById(entry);
			if (!lootEntry)
			{
				// Error
				ELOG("Loot entry " << entry << " found, but no creature to assign found");
				row = row.next(select);
				continue;
			}

			if (created)
			{
				auto *unitEntry = project.units.getEditableById(entry);
				if (!unitEntry)
				{
					WLOG("No unit with entry " << entry << " found - creature loot template will not be assigned!");
				}
				else
				{
					unitEntry->unitLootEntry = lootEntry;
				}
			}

			// If there are no loot groups yet, create a new one
			if (lootEntry->lootGroups.empty() || groupId > lastGroup)
			{
				lootEntry->lootGroups.push_back(LootGroup());
				if (groupId > lastGroup)
				{
					lastGroup = groupId;
					groupIndex++;
				}
			}

			if (lootEntry->lootGroups.empty())
			{
				ELOG("Error retrieving loot group");
				row = row.next(select);
				continue;
			}

			LootGroup &group = lootEntry->lootGroups[groupIndex];
			LootDefinition def;
			def.item = itemEntry;
			def.minCount = minCount;
			def.maxCount = maxCount;
			def.dropChance = dropChance;
			def.isActive = (active != 0);
			group.emplace_back(std::move(def));

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

	// Load the configuration
	wowpp::Configuration configuration;
	if (!configuration.load("wowpp_realm.cfg"))
	{
		// Shutdown for now
		return false;
	}

	// The log files are written to in a special background thread
	std::ofstream logFile;
	LogStreamOptions logFileOptions = g_DefaultFileLogOptions;
	logFileOptions.alwaysFlush = !configuration.isLogFileBuffering;

	boost::signals2::scoped_connection genericLogConnection;
	if (configuration.isLogActive)
	{
		const String fileName = configuration.logFileName;
		logFile.open(fileName.c_str(), std::ios::app);
		if (logFile)
		{
			genericLogConnection = g_DefaultLog.signal().connect(
				[&logFile, &logFileOptions](const LogEntry & entry)
			{
				printLogEntry(
					logFile,
					entry,
					logFileOptions);
			});
		}
		else
		{
			ELOG("Could not open log file '" << fileName << "'");
		}
	}

	// Database connection
	MySQL::DatabaseInfo connectionInfo(configuration.mysqlHost, configuration.mysqlPort, configuration.mysqlUser, configuration.mysqlPassword, configuration.mysqlDatabase);
	MySQL::Connection connection;
	if (!connection.connect(connectionInfo))
	{
		ELOG("Could not connect to the database");
		ELOG(connection.getErrorMessage());
		return 0;
	}

	Project proj;
	if (!proj.load(configuration.dataPath))
	{
		return 1;
	}
	
	if (!importCreatureLoot(proj, connection))
	{
		return 1;
	}

	proj.save(configuration.dataPath);

	return 0;
}
