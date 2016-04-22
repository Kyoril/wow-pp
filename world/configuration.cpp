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
	const UInt32 Configuration::WorldConfigVersion = 0x02;

	Configuration::Configuration()
#if defined(WIN32) || defined(_WIN32)
		: dataPath("data")
#else
		: dataPath("/etc/wow-pp/data")
#endif
		, mysqlPort(wowpp::constants::DefaultMySQLPort)
		, mysqlHost("127.0.0.1")
		, mysqlUser("wow-pp")
		, mysqlPassword("test")
		, mysqlDatabase("wowpp_world")
		, isLogActive(true)
		, logFileName("wowpp_world.log")
		, isLogFileBuffering(false)
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
				fileVersion != WorldConfigVersion)
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

			// Clear realm entries
			realms.clear();

			if (const sff::read::tree::Array<String::const_iterator> *const realmsArray = global.getArray("realms"))
			{
				for (size_t i = 0, d2 = realmsArray->getSize(); i < d2; ++i)
				{
					RealmConfiguration realmEntry;
					if (const Table *const realmTable = realmsArray->getTable(i))
					{
						realmEntry.realmAddress = realmTable->getString("address", "127.0.0.1");
						realmEntry.realmPort = realmTable->getInteger("port", constants::DefaultRealmWorldPort);
						realmEntry.maxPlayers = realmTable->getInteger("maxCount", std::numeric_limits<size_t>::max());

						if (const sff::read::tree::Array<String::const_iterator> * mapsArray = realmTable->getArray("hosted_maps"))
						{
							// Reallocate
							realmEntry.hostedMaps.resize(mapsArray->getSize());
							for (size_t j = 0, d = mapsArray->getSize(); j < d; ++j)
							{
								realmEntry.hostedMaps[j] = mapsArray->getInteger(j, 0);
							}

							// Unify vector
							std::unique(realmEntry.hostedMaps.begin(), realmEntry.hostedMaps.end());
						}
					}

					// Add the realm entry
					realms.push_back(std::move(realmEntry));
				}
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
		global.addKey("version", WorldConfigVersion);
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

		//TODO
// 		global.writer.newLine();
// 
// 		{
// 			sff::write::Table<Char> realmConnector(global, "realmConnector", sff::write::MultiLine);
// 			realmConnector.addKey("address", realmAddress);
// 			realmConnector.addKey("port", realmPort);
// 			realmConnector.addKey("maxCount", maxPlayers);
// 			
// 			sff::write::Array<char> mapsArray(realmConnector, "hosted_maps", sff::write::Comma);
// 			{
// 				for (const auto &mapId : hostedMaps)
// 				{
// 					mapsArray.addElement(mapId);
// 				}
// 			}
// 			mapsArray.finish();
// 
// 			realmConnector.finish();
// 		}

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
}
