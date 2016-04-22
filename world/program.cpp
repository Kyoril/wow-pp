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
#include "program.h"
#include "common/constants.h"
#include "wowpp_protocol/wowpp_protocol.h"
#include "wowpp_protocol/wowpp_connector.h"
#include "realm_connector.h"
#include "player_manager.h"
#include "player.h"
#include "game/world_instance_manager.h"
#include "common/background_worker.h"
#include "log/log_std_stream.h"
#include "log/log_entry.h"
#include "log/default_log_levels.h"
#include "mysql_database.h"
#include "proto_data/project.h"
#include "game/universe.h"
#include "trigger_handler.h"
#include "common/timer_queue.h"
#include "common/id_generator.h"
#include "common/make_unique.h"
#include "common/crash_handler.h"
#include "version.h"

namespace wowpp
{
	Program::Program()
		: m_shouldRestart(false)
	{
	}

	bool Program::run(const String &configFileName)
	{
		// Header
		ILOG("Starting WoW++ World Node");
		ILOG("Version " << Major << "." << Minor << "." << Build << "." << Revisision << " (Commit: " << GitCommit << ")");
		ILOG("Last Change: " << GitLastChange);

#if defined(DEBUG) || defined(_DEBUG)
		ILOG("Debug build enabled");
#endif

		// Load the configuration
		if (!m_configuration.load(configFileName))
		{
			// Shutdown for now
			return false;
		}

		// No realms set up
		if (m_configuration.realms.empty())
		{
			WLOG("No realms to connect to!");
			return false;
		}

		// Create a timer queue
		TimerQueue timer(m_ioService);

		// Create universe
		Universe universe(m_ioService, timer);

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
		proto::Project project;
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
		IdGenerator<UInt64> itemIdGenerator(0x01);
		IdGenerator<UInt64> worldObjectIdGenerator(0x01);

		// Set database instance
		m_database = std::move(db);

		// Create the player manager
		std::unique_ptr<wowpp::PlayerManager> PlayerManager(new wowpp::PlayerManager(std::numeric_limits<size_t>::max()));	//TODO: Max player count
		std::unique_ptr<TriggerHandler> triggerHandler = make_unique<TriggerHandler>(project, *PlayerManager, timer);

		// Create world instance manager
		auto worldInstanceManager =
			std::make_shared<wowpp::WorldInstanceManager>(m_ioService, universe, *triggerHandler, instanceIdGenerator, objectIdGenerator, project, 0, m_configuration.dataPath);

		std::vector<std::shared_ptr<RealmConnector>> realmConnectors;
		std::map<UInt32, RealmConnector*> realmConnectorByMap;

		// Create the realm connector
		size_t realmIndex = 0;
		for (auto &realm : m_configuration.realms)
		{
			auto realmConnector =
				std::make_shared<wowpp::RealmConnector>(m_ioService, *worldInstanceManager, *PlayerManager, m_configuration, realmIndex++, project, timer);

			// Remember the realm connector
			realmConnectors.push_back(std::move(realmConnector));
		}
		
		auto const createPlayer = [&PlayerManager, &worldInstanceManager, &project](RealmConnector &connector, DatabaseId characterId, std::shared_ptr<GameCharacter> character, WorldInstance &instance)
		{
			// Create the player instance
			std::unique_ptr<wowpp::Player> player(
				new wowpp::Player(
					*PlayerManager, 
					connector, 
					*worldInstanceManager, 
					characterId, 
					std::move(character),
					instance,
					project
					)
				);

			// Add the player to the manager
			PlayerManager->addPlayer(std::move(player));
		};

		std::map<RealmConnector*, boost::signals2::scoped_connection> enterConnections;
		for (auto &realm : realmConnectors)
		{
			enterConnections[realm.get()] = realm->worldInstanceEntered.connect(createPlayer);
		}

		//when the application terminates unexpectedly
		const auto crashFlushConnection =
			wowpp::CrashHandler::get().onCrash.connect(
				[&PlayerManager, &logFile]()
		{
			ELOG("Application crashed - saving players");
			const auto &players = PlayerManager->getPlayers();
			for (auto &player : players)
			{
				player->saveCharacterData();
			}

			ILOG("Player character saved. Shutting down.");
			if (logFile) logFile.flush();
		});


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
