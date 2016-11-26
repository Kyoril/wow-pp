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

#include "pch.h"
#include "configuration.h"
#include "simple_file_format/sff_write.h"
#include "simple_file_format/sff_read_tree.h"
#include "simple_file_format/sff_load_file.h"
#include "common/constants.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	const UInt32 Configuration::LoginConfigVersion = 0x02;

	Configuration::Configuration()
		: playerPort(wowpp::constants::DefaultLoginPlayerPort)
        , realmPort(wowpp::constants::DefaultLoginRealmPort)
		, teamPort(wowpp::constants::DefaultLoginTeamPort)
		, maxPlayers((std::numeric_limits<decltype(maxPlayers)>::max)())
		, maxRealms((std::numeric_limits<decltype(maxRealms)>::max)())
		, maxTeamServers((std::numeric_limits<decltype(maxTeamServers)>::max)())
		, mysqlPort(wowpp::constants::DefaultMySQLPort)
		, mysqlHost("127.0.0.1")
		, mysqlUser("wow-pp")
		, mysqlPassword("test")
		, mysqlDatabase("wowpp_login")
		, isLogActive(true)
		, logFileName("wowpp_login.log")
		, isLogFileBuffering(false)
		, webPort(8090)
		, webSSLPort(8091)
		, webUser("wowpp-web")
		, webPassword("test")
	{
	}

	bool Configuration::load(const String &fileName)
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
				fileVersion != LoginConfigVersion)
			{
				file.close();

				if (save(fileName + ".updated"))
				{
					ILOG("Saved updated settings with default values as " << fileName << ".updated");
					ILOG("Please insert values from the old setting file manually and rename the file.");
				}
				else
				{
					ELOG("Could not save updated default settings as " << fileName << ".updated");
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

			if (const Table *const mysqlDatabaseTable = global.getTable("webServer"))
			{
				webPort = mysqlDatabaseTable->getInteger("port", webPort);
				webSSLPort = mysqlDatabaseTable->getInteger("ssl_port", webSSLPort);
				webUser = mysqlDatabaseTable->getString("user", webUser);
				webPassword = mysqlDatabaseTable->getString("password", webPassword);
			}

			if (const Table *const playerManager = global.getTable("playerManager"))
			{
				playerPort = playerManager->getInteger("port", playerPort);
				maxPlayers = playerManager->getInteger("maxCount", maxPlayers);
			}

			if (const Table *const realmManager = global.getTable("realmManager"))
			{
				realmPort = realmManager->getInteger("port", realmPort);
				maxRealms = realmManager->getInteger("maxCount", maxRealms);
			}

			if (const Table *const teamManager = global.getTable("teamManager"))
			{
				teamPort = teamManager->getInteger("port", teamPort);
				maxTeamServers = teamManager->getInteger("maxCount", maxTeamServers);
			}

			if (const Table *const log = global.getTable("log"))
			{
				isLogActive = log->getInteger("active", static_cast<unsigned>(isLogActive)) != 0;
				logFileName = log->getString("fileName", logFileName);
				isLogFileBuffering = log->getInteger("buffering", static_cast<unsigned>(isLogFileBuffering)) != 0;
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


	bool Configuration::save(const String &fileName)
	{
		std::ofstream file(fileName.c_str());
		if (!file)
		{
			return false;
		}

		typedef char Char;
		sff::write::File<Char> global(file, sff::write::MultiLine);

		// Save file version
		global.addKey("version", LoginConfigVersion);
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
			sff::write::Table<Char> mysqlDatabaseTable(global, "webServer", sff::write::MultiLine);
			mysqlDatabaseTable.addKey("port", webPort);
			mysqlDatabaseTable.addKey("ssl_port", webSSLPort);
			mysqlDatabaseTable.addKey("user", webUser);
			mysqlDatabaseTable.addKey("password", webPassword);
			mysqlDatabaseTable.finish();
		}

		global.writer.newLine();

		{
			sff::write::Table<Char> playerManager(global, "playerManager", sff::write::MultiLine);
			playerManager.addKey("port", playerPort);
			playerManager.addKey("maxCount", maxPlayers);
			playerManager.finish();
		}

		global.writer.newLine();

		{
			sff::write::Table<Char> realmManager(global, "realmManager", sff::write::MultiLine);
			realmManager.addKey("port", realmPort);
			realmManager.addKey("maxCount", maxRealms);
			realmManager.finish();
		}

		global.writer.newLine();

		{
			sff::write::Table<Char> teamManager(global, "teamManager", sff::write::MultiLine);
			teamManager.addKey("port", teamPort);
			teamManager.addKey("maxCount", maxTeamServers);
			teamManager.finish();
		}

		global.writer.newLine();

		{
			sff::write::Table<Char> log(global, "log", sff::write::MultiLine);
			log.addKey("active", static_cast<unsigned>(isLogActive));
			log.addKey("fileName", logFileName);
			log.addKey("buffering", isLogFileBuffering);
			log.finish();
		}

		return true;
	}
}
