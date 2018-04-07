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
#include "simple_file_format/sff_datatypes.h"
#include "common/constants.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace editor
	{
		const UInt32 Configuration::EditorConfigVersion = 0x02;

		Configuration::Configuration()
			: dataPath("")
			, wowGamePath("C:\\Program Files (x86)\\World of Warcraft\\")
			, teamAddress("127.0.0.1")
			, teamPort(constants::DefaultTeamEditorPort)
			, isLogActive(true)
			, logFileName("wowpp_editor.log")
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
					fileVersion != EditorConfigVersion)
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

				if (const Table *const log = global.getTable("log"))
				{
					isLogActive = log->getInteger("active", static_cast<unsigned>(isLogActive)) != 0;
					logFileName = log->getString("fileName", logFileName);
					isLogFileBuffering = log->getInteger("buffering", static_cast<unsigned>(isLogFileBuffering)) != 0;
				}

				projects.clear();

				// Read projects
				if (const auto *const projectsArray = global.getArray("projects"))
				{
					projects.reserve(projectsArray->getSize());

					for (size_t i = 0; i < projectsArray->getSize(); ++i)
					{
						const auto* projectTable = projectsArray->getTable(i);

						DataProject project;
						project.name = projectTable->getString("name", "Unnamed");
						project.exportPath = projectTable->getString("export_path");
						
						const auto* mysqlDatabaseTable = projectTable->getTable("mysqlDatabase");
						if (mysqlDatabaseTable)
						{
							project.databaseInfo.mysqlPort = mysqlDatabaseTable->getInteger("port", project.databaseInfo.mysqlPort);
							project.databaseInfo.mysqlHost = mysqlDatabaseTable->getString("host", project.databaseInfo.mysqlHost);
							project.databaseInfo.mysqlUser = mysqlDatabaseTable->getString("user", project.databaseInfo.mysqlUser);
							project.databaseInfo.mysqlPassword = mysqlDatabaseTable->getString("password", project.databaseInfo.mysqlPassword);
							project.databaseInfo.mysqlDatabase = mysqlDatabaseTable->getString("database", project.databaseInfo.mysqlDatabase);
						}

						projects.emplace_back(project);
					}
				}

				if (const Table *const game = global.getTable("game"))
				{
					dataPath = game->getString("dataPath", dataPath);
					wowGamePath = game->getString("wowGamePath", wowGamePath);
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
			global.addKey("version", EditorConfigVersion);
			global.writer.newLine();

			// Add a sample project
			{
				sff::write::Array<Char> projectsArray(global, "projects", sff::write::MultiLine);
				{
					sff::write::Table<Char> projectTable(projectsArray, sff::write::Comma);
					{
						projectTable.addKey("name", "Sample project");
						projectTable.addKey("export_path", "C:/wow-pp-data");
						
						sff::write::Table<Char> mysqlDatabaseTable(projectTable, "mysqlDatabase", sff::write::MultiLine);
						mysqlDatabaseTable.addKey("port", constants::DefaultMySQLPort);
						mysqlDatabaseTable.addKey("host", "127.0.0.1");
						mysqlDatabaseTable.addKey("user", "username");
						mysqlDatabaseTable.addKey("password", "password");
						mysqlDatabaseTable.addKey("database", "database");
						mysqlDatabaseTable.finish();
					}
					projectTable.finish();
				}
				projectsArray.finish();
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
				game.addKey("wowGamePath", wowGamePath);
				game.finish();
			}

			return true;
		}
	}
}
