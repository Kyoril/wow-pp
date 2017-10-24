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
#include "wowpp_protocol/wowpp_server.h"
#include "game_protocol/game_protocol.h"
#include "game_protocol/game_server.h"
#include "player_manager.h"
#include "player.h"
#include "player_social.h"
#include "player_group.h"
#include "world_manager.h"
#include "world.h"
#include "login_connector.h"
#include "common/background_worker.h"
#include "log/log_std_stream.h"
#include "log/log_entry.h"
#include "log/default_log_levels.h"
#include "mysql_database.h"
#include "web_service.h"
#include "common/timer_queue.h"
#include "common/id_generator.h"
#include "proto_data/project.h"
#include "version.h"

namespace wowpp
{
	Program::Program(bool asService)
		: m_isService(asService)
		, m_shouldRestart(false)
	{
	}

	bool Program::run()
	{
		auto start = getCurrentTime();

		// Header
		ILOG("Starting WoW++ Realm");
		ILOG("Version " << Major << "." << Minor << "." << Build << "." << Revisision << " (Commit: " << GitCommit << ")");
		ILOG("Last Change: " << GitLastChange);

#ifdef WOWPP_WITH_DEV_COMMANDS
		ILOG("[Developer Commands Enabled]");
#endif
#if defined(DEBUG) || defined(_DEBUG)
		DLOG("[Debug build enabled]");
#endif

		// Load the configuration
		if (!m_configuration.load("wowpp_realm.cfg"))
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

		simple::scoped_connection genericLogConnection;
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
				project,
				MySQL::DatabaseInfo(
					m_configuration.mysqlHost,
					m_configuration.mysqlPort,
					m_configuration.mysqlUser,
					m_configuration.mysqlPassword,
					m_configuration.mysqlDatabase
				)
			));
		if (!db->load())
		{
			// Could not load MySQL database
			return false;
		}

		// Set database instance
		m_database = std::move(db);

		// Setup async database requester
		boost::asio::io_service databaseWorkQueue;
		boost::asio::io_service::work databaseWork(databaseWorkQueue);
		(void)databaseWork;
		const auto async = [&databaseWorkQueue](Action action)
		{
			databaseWorkQueue.post(std::move(action));
		};
		const auto sync = [this](Action action)
		{
			m_ioService.post(std::move(action));
		};
		AsyncDatabase asyncDatabase(*m_database, async, sync);
		
		auto end = getCurrentTime();
		DLOG("Realm started in " << (end - start) << " ms");

		// TODO: Use async database requests so no blocking occurs

		// Create the player manager
		std::unique_ptr<wowpp::PlayerManager> PlayerManager(new wowpp::PlayerManager(timer, m_configuration.realmID, m_configuration.maxPlayers));

		// Create the world manager
		std::unique_ptr<wowpp::WorldManager> WorldManager(new wowpp::WorldManager(m_configuration.maxWorlds));

		// Create the login connector
		auto loginConnector = 
			std::make_shared<wowpp::LoginConnector>(m_ioService, *PlayerManager, m_configuration, timer);
		
		// Create the world server
		std::unique_ptr<wowpp::pp::Server> worldServer;
		try
		{
			worldServer.reset(new wowpp::pp::Server(std::ref(m_ioService), m_configuration.worldPort, std::bind(&wowpp::pp::Connection::create, std::ref(m_ioService), nullptr)));
		}
		catch (const wowpp::BindFailedException &)
		{
			ELOG("Could not use world port " << m_configuration.worldPort << "! Maybe there is another server instance running on this port?");
			return false;
		}

		String &realmName = m_configuration.internalName;
		auto const createWorld = [&WorldManager, &realmName, &PlayerManager, &project, this](std::shared_ptr<wowpp::World::Client> connection)
		{
			connection->startReceiving();
			boost::asio::ip::address address;

			try
			{
				address = connection->getRemoteAddress();
			}
			catch (const boost::system::system_error &error)
			{
				//getRemoteAddress calls remote_endpoint on a socket which can throw if the socket is closed.
				std::cout << error.what() << std::endl;
				return;
			}

			auto world = std::make_shared<World>(*WorldManager, *PlayerManager, project, *m_database, std::move(connection), address.to_string(), realmName);

			DLOG("Incoming world connection from " << address);
			WorldManager->addWorld(world);
		};

		const simple::scoped_connection worldConnected(worldServer->connected().connect(createWorld));
		worldServer->startAccept();

		// Create the player server
		std::unique_ptr<wowpp::game::Server> playerServer;
		try
		{
			playerServer.reset(new wowpp::game::Server(std::ref(m_ioService), m_configuration.playerPort, std::bind(&wowpp::game::Connection::create, std::ref(m_ioService), nullptr)));
		}
		catch (const wowpp::BindFailedException &)
		{
			ELOG("Could not use player port " << m_configuration.playerPort << "! Maybe there is another server instance running on this port?");
			return false;
		}

		std::unique_ptr<WebService> webService;
		webService.reset(new WebService(
			m_ioService,
			m_configuration.webPort,
			m_configuration.webPassword,
			*PlayerManager,
			*WorldManager,
			*m_database,
			project
			));

		IdGenerator<UInt64> groupIdGenerator(0x01);

		IDatabase &database = *m_database;
		Configuration &config = m_configuration;

		// Restore groups
		auto groupIds = database.listGroups();
		if (groupIds)
		{
			for (auto &groupId : *groupIds)
			{
				// Create a new group
				auto group = std::make_shared<PlayerGroup>(groupId, *PlayerManager, database);
				if (!group->createFromDatabase())
				{
					ELOG("Could not restore group " << groupId);
				}

				// Notify the generator about the new group id to avoid overlaps
				groupIdGenerator.notifyId(groupId);
			}
		}
		else
		{
			ELOG("Could not restore group ids!");
		}

		auto const createPlayer = [&PlayerManager, &loginConnector, &WorldManager, &database, &project, &config, &groupIdGenerator](std::shared_ptr<wowpp::Player::Client> connection)
		{
			connection->startReceiving();
			boost::asio::ip::address address;

			try
			{
				address = connection->getRemoteAddress();
			}
			catch (const boost::system::system_error &error)
			{
				//getRemoteAddress calls remote_endpoint on a socket which can throw if the socket is closed.
				std::cout << error.what() << std::endl;
				return;
			}

			auto player = std::make_shared<Player>(config, groupIdGenerator, *PlayerManager, *loginConnector, *WorldManager, database, project, std::move(connection), address.to_string());

			DLOG("Incoming player connection from " << address);
			PlayerManager->addPlayer(player);
		};

		const simple::scoped_connection playerConnected(playerServer->connected().connect(createPlayer));
		playerServer->startAccept();

		// Create database worker thread
		std::thread databaseWorkThread([&databaseWorkQueue] { databaseWorkQueue.run(); });

		// Run IO service
		m_ioService.run();

		// Stop the database worker queue and wait for the thread to finish
		databaseWorkQueue.stop();
		databaseWorkThread.join();

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
