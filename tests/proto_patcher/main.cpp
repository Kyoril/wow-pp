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
#include "common/clock.h"
#include "common/constants.h"
#include "simple_file_format/sff_write_file.h"
#include "simple_file_format/sff_write_array.h"
#include "simple_file_format/sff_write_array.h"
#include "simple_file_format/sff_write.h"
#include "simple_file_format/sff_read_tree.h"
#include "simple_file_format/sff_load_file.h"
#include "log/default_log.h"
#include "log/log_std_stream.h"
#include "log/default_log_levels.h"
#include "mysql_wrapper/mysql_connection.h"
#include "mysql_wrapper/mysql_row.h"
#include "mysql_wrapper/mysql_select.h"
#include "mysql_wrapper/mysql_statement.h"
#include "common/make_unique.h"
#include "common/linear_set.h"
#include "virtual_directory/file_system_reader.h"
#include "proto_data/project_loader.h"
#include "proto_data/project_saver.h"
#include "proto_data/project.h"
using namespace wowpp;

namespace wowpp
{
	/// Manages the realm server configuration.
	struct Configuration
	{
		/// Config file version: used to detect new configuration files
		static const UInt32 PatcherConfigVersion;

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
			: dataPath("./")
			, mysqlPort(wowpp::constants::DefaultMySQLPort)
			, mysqlHost("127.0.0.1")
			, mysqlUser("username")
			, mysqlPassword("password")
			, mysqlDatabase("dbc")
			, isLogActive(true)
			, logFileName("wowpp_patcher.log")
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
					fileVersion != PatcherConfigVersion)
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
			global.addKey("version", PatcherConfigVersion);
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

	const UInt32 Configuration::PatcherConfigVersion = 0x01;
}

namespace wowpp
{
	static bool addSpellLinks(proto::Project &project)
	{
		auto *spellEntries = project.spells.getTemplates().mutable_entry();
		if (spellEntries)
		{
			auto it = spellEntries->begin();
			while (it != spellEntries->end())
			{
				// Remove all additional spell entries
				it->clear_additionalspells();

				// Check this spell
				switch (it->id())
				{
					//////////////////////////////////////////////////////////////////////////
					// Power Word: Shield
					case 17:	// Rank 1
					case 592:	// Rank 2
					case 600:	// Rank 3
					case 3747:	// Rank 4
					case 6065:	// Rank 5
					case 6066:	// Rank 6
					case 10898:	// Rank 7
					case 10899:	// Rank 8
					case 10900: // Rank 9
					case 10901:	// Rank 10
					case 25217:	// Rank 11
					case 25218:	// Rank 12
						it->add_additionalspells(6788);	// Weakened Soul
						break;
					//////////////////////////////////////////////////////////////////////////
					// Holy Nova
					case 15237:	// Rank 1
						it->add_additionalspells(23455);
						break;	
					case 15430:	// Rank 2
						it->add_additionalspells(23458);
						break;	
					case 15431:	// Rank 3
						it->add_additionalspells(23459);
						break;	
					case 27799:	// Rank 4
						it->add_additionalspells(27803);
						break;	
					case 27800:	// Rank 5
						it->add_additionalspells(27804);
						break;	
					case 27801:	// Rank 6
						it->add_additionalspells(27805);
						break;	
					case 25331:	// Rank 7
						it->add_additionalspells(25329);
						break;
					//////////////////////////////////////////////////////////////////////////
					// Iceblock
					case 45438:
						it->add_additionalspells(41425);
						break;
					//////////////////////////////////////////////////////////////////////////
					// Bandages
					case 746:
					case 1159:
					case 3267:
					case 3268:
					case 7926:
					case 7927:
					case 10838:
					case 10839:
					case 18608:
					case 18610:
					case 23567:
					case 23568:
					case 23569:
					case 23696:
					case 24412:
					case 24413:
					case 24414:
					case 27030:
					case 27031:
					case 30020:
						it->add_additionalspells(11196);
						break;
					//////////////////////////////////////////////////////////////////////////
					// Blood Fury
					case 20572:
					case 33697:
					case 33702:
						it->add_additionalspells(23230);
						break;
					//////////////////////////////////////////////////////////////////////////
					// Paladin bubble debuff
					// Divine Protection
					case 498:	// Rank 1
					case 5573:	// Rank 2
					// Divine Shield
					case 642:	// Rank 1
					case 1020:	// Rank 2
					// Hand of Protection
					case 1022:	// Rank 1
					case 5599:	// Rank 2
					case 10278:	// Rank 3
						it->add_additionalspells(25771);
						break;
					//////////////////////////////////////////////////////////////////////////
					// OTHER SPELLS GO HERE
				}

				it++;
			}
		}

		return true;
	}

	static bool importSpellMechanics(proto::Project &project, MySQL::Connection &conn)
	{
		wowpp::MySQL::Select select(conn, "SELECT `Id`, `Mechanic` FROM `dbc_spell`;");
		if (select.success())
		{
			wowpp::MySQL::Row row(select);
			while (row)
			{
				// Get row data
				UInt32 id = 0, mechanic = 0;
				row.getField(0, id);
				row.getField(1, mechanic);

				// 0 = default mechanic
				if (mechanic != 0)
				{
					// Find spell by id
					auto * spell = project.spells.getById(id);
					if (spell)
					{
						spell->set_mechanic(mechanic);
					}
					else
					{
						WLOG("Unable to find spell by id: " << id);
					}
				}
				
				// Next row
				row = row.next(select);
			}
		}

		return true;
	}
}

/// Procedural entry point of the application.
int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// Add cout to the list of log output streams
	wowpp::g_DefaultLog.signal().connect(std::bind(
		wowpp::printLogEntry,
		std::ref(std::cout), std::placeholders::_1, wowpp::g_DefaultConsoleLogOptions));
	
	// Load the configuration
	wowpp::Configuration configuration;
	if (!configuration.load("proto_patcher.cfg"))
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

	// Load existing project
	wowpp::proto::Project protoProject;
	if (!protoProject.load(configuration.dataPath))
	{
		return 1;
	}

	// Load all spell mechanics
	MySQL::DatabaseInfo connectionInfo(configuration.mysqlHost, configuration.mysqlPort, configuration.mysqlUser, configuration.mysqlPassword, configuration.mysqlDatabase);
	MySQL::Connection connection;
	if (!connection.connect(connectionInfo))
	{
		ELOG("Could not connect to the database");
		ELOG(connection.getErrorMessage());
		return 0;
	}
	else
	{
		ILOG("MySQL connection established!");
	}

	if (!importSpellMechanics(protoProject, connection))
	{
		ELOG("Failed to import spell mechanics");
		return 1;
	}

	if (!addSpellLinks(protoProject))
	{
		ELOG("Failed to add spell links");
		return 1;
	}

	// Save project
	if (!protoProject.save(configuration.dataPath))
	{
		return 1;
	}

	// Wait for user input to finish
	std::cin.get();

	// Shutdown protobuf and free all memory (optional)
	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}
