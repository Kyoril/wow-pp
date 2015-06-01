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
#include "wowpp_protocol/wowpp_server.h"
#include "game_protocol/game_protocol.h"
#include "game_protocol/game_server.h"
#include "player_manager.h"
#include "player.h"
#include "world_manager.h"
#include "world.h"
#include "login_connector.h"
#include "common/background_worker.h"
#include "log/log_std_stream.h"
#include "log/log_entry.h"
#include "log/default_log_levels.h"
#include "mysql_database.h"
#include "common/timer_queue.h"
#include "data/project.h"
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

		// Set database instance
		m_database = std::move(db);

		// Create the player manager
		std::unique_ptr<wowpp::PlayerManager> PlayerManager(new wowpp::PlayerManager(m_configuration.maxPlayers));

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

		auto const createWorld = [&WorldManager, &PlayerManager, &project](std::shared_ptr<wowpp::World::Client> connection)
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

			std::unique_ptr<wowpp::World> world(new wowpp::World(*WorldManager, *PlayerManager, project, std::move(connection), address.to_string()));

			DLOG("Incoming world connection from " << address);
			WorldManager->addWorld(std::move(world));
		};

		const boost::signals2::scoped_connection worldConnected(worldServer->connected().connect(createWorld));
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

		IDatabase &database = *m_database;
		Configuration &config = m_configuration;
		auto const createPlayer = [&PlayerManager, &loginConnector, &WorldManager, &database, &project, &config](std::shared_ptr<wowpp::Player::Client> connection)
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

			std::unique_ptr<wowpp::Player> player(new wowpp::Player(config, *PlayerManager, *loginConnector, *WorldManager, database, project, std::move(connection), address.to_string()));

			DLOG("Incoming player connection from " << address);
			PlayerManager->addPlayer(std::move(player));
		};

		const boost::signals2::scoped_connection playerConnected(playerServer->connected().connect(createPlayer));
		playerServer->startAccept();

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
