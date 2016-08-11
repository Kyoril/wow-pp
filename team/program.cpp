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
#include "login_connector.h"
#include "editor_manager.h"
#include "editor.h"
#include "common/background_worker.h"
#include "log/log_std_stream.h"
#include "log/log_entry.h"
#include "log/default_log_levels.h"
#include "mysql_database.h"
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
		ILOG("Starting WoW++ Team Server");
		ILOG("Version " << Major << "." << Minor << "." << Build << "." << Revisision << " (Commit: " << GitCommit << ")");
		ILOG("Last Change: " << GitLastChange);

#if defined(DEBUG) || defined(_DEBUG)
		DLOG("[Debug build enabled]");
#endif

		// Load the configuration
		if (!m_configuration.load("wowpp_team.cfg"))
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
		DLOG("Team server started in " << (end - start) << " ms");

		// TODO: Use async database requests so no blocking occurs

		// Create the editor manager
		std::unique_ptr<wowpp::EditorManager> EditorManager(new wowpp::EditorManager(1000));

		// Create the login connector
		auto loginConnector = 
			std::make_shared<wowpp::LoginConnector>(m_ioService, m_configuration, timer);
		
		// Create the editor server
		std::unique_ptr<wowpp::pp::Server> editorServer;
		try
		{
			editorServer.reset(new wowpp::pp::Server(std::ref(m_ioService), m_configuration.editorPort, std::bind(&wowpp::pp::Connection::create, std::ref(m_ioService), nullptr)));
		}
		catch (const wowpp::BindFailedException &)
		{
			ELOG("Could not use editor port " << m_configuration.editorPort << "! Maybe there is another server instance running on this port?");
			return false;
		}

		IDatabase &database = *m_database;
		Configuration &config = m_configuration;

		auto const createEditor = [&EditorManager, &loginConnector, &project](std::shared_ptr<wowpp::Editor::Client> connection)
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

			auto editor = std::make_shared<wowpp::Editor>(
				*EditorManager.get(), 
				*loginConnector, 
				project,
				std::move(connection), 
				address.to_string());

			DLOG("Incoming editor connection from " << address);
			EditorManager->addEditor(editor);
		};
		
		const boost::signals2::scoped_connection editorConnected(editorServer->connected().connect(createEditor));
		editorServer->startAccept();

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
