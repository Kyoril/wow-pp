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
#include "auth_protocol/auth_server.h"
#include "wowpp_protocol/wowpp_server.h"
#include "common/background_worker.h"
#include "common/timer_queue.h"
#include "log/log_std_stream.h"
#include "log/log_entry.h"
#include "log/default_log_levels.h"
#include "mysql_database.h"
#include "player_manager.h"
#include "player.h"
#include "realm_manager.h"
#include "realm.h"
#include "team_server_manager.h"
#include "team_server.h"
#include "web_service.h"
#include "version.h"

namespace wowpp
{
	Program::Program()
		: m_shouldRestart(false)
	{
	}

	bool Program::run()
	{
		// Header
		ILOG("Starting WoW++ Login Server");
		ILOG("Version " << Major << "." << Minor << "." << Build << "." << Revisision << " (Commit: " << GitCommit << ")");
		ILOG("Last Change: " << GitLastChange);

#if defined(DEBUG) || defined(_DEBUG)
		ILOG("Debug build enabled");
#endif

		// Load the configuration
		if (!m_configuration.load("wowpp_login.cfg"))
		{
			// Shutdown for now
			return false;
		}

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
		
		// Create the realm server
		std::unique_ptr<wowpp::pp::Server> realmServer;
		try
		{
			realmServer.reset(new wowpp::pp::Server(std::ref(m_ioService), m_configuration.realmPort, std::bind(&wowpp::pp::Connection::create, std::ref(m_ioService), nullptr)));
		}
		catch (const wowpp::BindFailedException &)
		{
			ELOG("Could not use realm port " << m_configuration.realmPort << "! Maybe there is another server instance running on this port?");
			return 1;
		}

		// Create the team server
		std::unique_ptr<wowpp::pp::Server> teamServer;
		try
		{
			teamServer.reset(new wowpp::pp::Server(std::ref(m_ioService), m_configuration.teamPort, std::bind(&wowpp::pp::Connection::create, std::ref(m_ioService), nullptr)));
		}
		catch (const wowpp::BindFailedException &)
		{
			ELOG("Could not use team server port " << m_configuration.teamPort << "! Maybe there is another server instance running on this port?");
			return 1;
		}

		IDatabase &Database = *m_database;

		// Create the player manager and the realm manager
		std::unique_ptr<wowpp::RealmManager> RealmManager(new wowpp::RealmManager(m_configuration.maxRealms));
		std::unique_ptr<wowpp::PlayerManager> PlayerManager(new wowpp::PlayerManager(m_configuration.maxPlayers));
		std::unique_ptr<wowpp::TeamServerManager> TeamServerManager(new wowpp::TeamServerManager(m_configuration.maxTeamServers));

		TimerQueue timerQueue(m_ioService);

		auto const createRealm = [&RealmManager, &PlayerManager, &Database, &timerQueue](std::shared_ptr<wowpp::Realm::Client> connection)
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

			std::unique_ptr<wowpp::Realm> realm(new wowpp::Realm(*RealmManager, *PlayerManager, Database, std::move(connection), address.to_string(), timerQueue));

			DLOG("Incoming realm connection from " << address);
			RealmManager->addRealm(std::move(realm));
		};

		const boost::signals2::scoped_connection realmConnected(realmServer->connected().connect(createRealm));
		realmServer->startAccept();

		auto const createTeamServer = [&TeamServerManager, &Database](std::shared_ptr<wowpp::TeamServer::Client> connection)
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

			std::unique_ptr<wowpp::TeamServer> teamServer(new wowpp::TeamServer(*TeamServerManager, Database, std::move(connection), address.to_string()));

			DLOG("Incoming team server connection from " << address);
			TeamServerManager->addTeamServer(std::move(teamServer));
		};

		const boost::signals2::scoped_connection teamServerConnected(teamServer->connected().connect(createTeamServer));
		teamServer->startAccept();

		// Create the player server
		std::unique_ptr<wowpp::auth::Server> playerServer;
		try
		{
			playerServer.reset(new wowpp::auth::Server(std::ref(m_ioService), m_configuration.playerPort, std::bind(&wowpp::auth::Connection::create, std::ref(m_ioService), nullptr)));
		}
		catch (const wowpp::BindFailedException &)
		{
			ELOG("Could not use player port " << m_configuration.playerPort << "! Maybe there is another server instance running on this port?");
			return 1;
		}

		const Player::SessionFactory createSession = [](Session::Key key, UInt32 userId, String userName, const BigNumber &v, const BigNumber &s) -> std::unique_ptr<Session>
		{
			return std::unique_ptr<Session>(new Session(key, userId, std::move(userName), v, s));
		};

		auto const createPlayer = [&PlayerManager, &RealmManager, &Database, createSession, &timerQueue](std::shared_ptr<wowpp::Player::Client> connection)
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

			std::unique_ptr<wowpp::Player> player(new wowpp::Player(*PlayerManager, *RealmManager, Database, createSession, std::move(connection), address.to_string(), timerQueue));

			DLOG("Incoming player connection from " << address);
			PlayerManager->addPlayer(std::move(player));
		};

		const boost::signals2::scoped_connection playerConnected(playerServer->connected().connect(createPlayer));
		playerServer->startAccept();

		std::unique_ptr<WebService> webService;
		webService.reset(new WebService(
			m_ioService,
			m_configuration.webPort,
			m_configuration.webPassword,
			*PlayerManager,
			Database
			));

		// Log start
		ILOG("Login server is ready and waiting for connections!");

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
