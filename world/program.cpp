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

#include "program.h"
#include "common/constants.h"
#include "wowpp_protocol/wowpp_protocol.h"
#include "wowpp_protocol/wowpp_connector.h"
#include "realm_connector.h"
#include "player_manager.h"
#include "player.h"
#include "world_instance_manager.h"
#include "common/background_worker.h"
#include "log/log_std_stream.h"
#include "log/log_entry.h"
#include "log/default_log_levels.h"
#include "mysql_database.h"
#include "data/project.h"
#include "common/timer_queue.h"
#include "common/id_generator.h"
#include <iostream>
#include <memory>
#include <fstream>

namespace wowpp
{
	Program::Program()
		: m_shouldRestart(false)
	{
	}

	bool Program::run()
	{
		// Load the configuration
		if (!m_configuration.load("wowpp_world.cfg"))
		{
			// Shutdown for now
			return false;
		}

		// Create a timer queue
		TimerQueue timer(m_ioService);

		// The log files are written to in a special background thread
		std::ofstream logFile;
		BackgroundWorker backgroundLogger;
		LogStreamOptions logFileOptions = g_DefaultFileLogOptions;
		logFileOptions.alwaysFlush = !m_configuration.isLogFileBuffering;

		boost::signals2::scoped_connection genericLogConnection;
		if (m_configuration.isLogActive)
		{
			const String fileName = m_configuration.logFileName;
			logFile.open(fileName.c_str(), std::ios::app);
			if (logFile)
			{
				genericLogConnection = g_DefaultLog.signal().connect(
					[&logFile, &backgroundLogger, &logFileOptions](const LogEntry & entry)
				{
					backgroundLogger.addWork(std::bind(
						printLogEntry,
						std::ref(logFile),
						entry,
						std::cref(logFileOptions)));
				});
			}
			else
			{
				ELOG("Could not open log file '" << fileName << "'");
			}
		}

		// Load project
		Project project;
		if (!project.load(m_configuration.dataPath))
		{
			ELOG("Could not load data project!");
			return false;
		}

		// Setup MySQL database
		std::unique_ptr<MySQLDatabase> db(
			new MySQLDatabase(
			MySQL::DatabaseInfo(m_configuration.mysqlHost,
			m_configuration.mysqlPort,
			m_configuration.mysqlUser,
			m_configuration.mysqlPassword,
			m_configuration.mysqlDatabase)));
		if (!db->load())
		{
			// Could not load MySQL database
			return false;
		}

		// Setup id generator
		IdGenerator<UInt32> instanceIdGenerator;			
		IdGenerator<UInt64> objectIdGenerator(0x01);		// Object guid 0 is invalid and will lead to a client crash!

		// Set database instance
		m_database = std::move(db);

		// Create the player manager
		std::unique_ptr<wowpp::PlayerManager> PlayerManager(new wowpp::PlayerManager(m_configuration.maxPlayers));

		// Create world instance manager
		auto worldInstanceManager =
			std::make_shared<wowpp::WorldInstanceManager>(m_ioService, instanceIdGenerator, objectIdGenerator, project);

		// Create the realm connector
		auto realmConnector = 
			std::make_shared<wowpp::RealmConnector>(m_ioService, *worldInstanceManager, *PlayerManager, m_configuration, project, timer);

		auto const createPlayer = [&PlayerManager, &realmConnector, &worldInstanceManager](DatabaseId characterId, std::shared_ptr<GameCharacter> character)
		{
			// Create the player instance
			std::unique_ptr<wowpp::Player> player(
				new wowpp::Player(
					*PlayerManager, 
					*realmConnector, 
					*worldInstanceManager, 
					characterId, 
					std::move(character)
					)
				);

			// Add the player to the manager
			PlayerManager->addPlayer(std::move(player));
		};

		boost::signals2::scoped_connection worldInstanceEntered(realmConnector->worldInstanceEntered.connect(createPlayer));

		// Run IO service
		m_ioService.run();

		// Do not restart but shutdown after this
		return m_shouldRestart;
	}

	void Program::shutdown(bool restart /*= false*/)
	{
		if (restart)
		{
			m_shouldRestart = true;
		}

		// Stop IO service if running
		if (!m_ioService.stopped())
		{
			ILOG("Shutting down server...");
			m_ioService.stop();
		}
	}
}
