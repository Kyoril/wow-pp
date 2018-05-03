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
#include <QApplication>
#include <QTextStream>
#include <QFile>
#include "common/background_worker.h"
#include "common/crash_handler.h"
#include "log/log_std_stream.h"
#include "log/log_entry.h"
#include "log/default_log_levels.h"
#include "editor_application.h"
#include "windows/main_window.h"
#include "ui_main_window.h"
#include "windows/object_editor.h"
#include "ui_object_editor.h"
#include "windows/trigger_editor.h"
#include "ui_trigger_editor.h"
#include "common/timer_queue.h"
#include <QMessageBox>
using namespace wowpp::editor;

namespace
{
	std::string previousExecutableToBeRemoved;
	bool doRetryRemovePreviousExecutable = false;

	void removePreviousExecutable()
	{
		try
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			ILOG("REMOVING OLD FILE... " << previousExecutableToBeRemoved);
			boost::filesystem::remove(previousExecutableToBeRemoved);
		}
		catch (const std::exception &e)
		{
			WLOG("Could not delete old file: " << e.what());

			//this error is not very important, so we do not exit here
			doRetryRemovePreviousExecutable = true;
		}
	}
}

/// Procedural entry point of the application.
int main(int argc, char *argv[])
{
	namespace po = boost::program_options;

	po::options_description desc("Available options");
	desc.add_options()
		("remove-previous", po::value<std::string>(&previousExecutableToBeRemoved), "Tries to remove a specified file")
		;

	po::variables_map vm;
	try
	{
		po::store(
			po::command_line_parser(argc, argv).options(desc).run(),
			vm
		);
		po::notify(vm);
	}
	catch (const boost::program_options::error &e)
	{
		std::cerr << e.what() << "\n";
	}

	if (!previousExecutableToBeRemoved.empty())
	{
		removePreviousExecutable();
	}

	//constructor enables error handling
	wowpp::CrashHandler::get().enableDumpFile("EditorCrash.dmp");

	// Create the qt application instance and set it all up
	QApplication app(argc, argv);

	// Setup io service object
	boost::asio::io_service ioService;

	// Setup timer queue
	wowpp::TimerQueue timers(ioService);

	// Load stylesheet
	QFile f(":qdarkstyle/style.qss");
	if (f.exists())
	{
		f.open(QFile::ReadOnly | QFile::Text);
		QTextStream ts(&f);
		app.setStyleSheet(ts.readAll());
	}

	// The log files are written to in a special background thread
	std::ofstream logFile;
	wowpp::BackgroundWorker backgroundLogger;
	wowpp::LogStreamOptions logFileOptions = wowpp::g_DefaultFileLogOptions;
	logFileOptions.alwaysFlush = true;

	simple::scoped_connection genericLogConnection;
	logFile.open("wowpp_editor.log", std::ios::app);
	if (logFile)
	{
		genericLogConnection = wowpp::g_DefaultLog.signal().connect(
			[&logFile, &backgroundLogger, &logFileOptions](const wowpp::LogEntry & entry)
		{
			backgroundLogger.addWork(std::bind(
				wowpp::printLogEntry,
				std::ref(logFile),
				entry,
				std::cref(logFileOptions)));
		});
	}

	// Create and load our own application class, since inheriting QApplication
	// seems to cause problems.
	EditorApplication editorAppInstance(ioService, timers);
	if (!editorAppInstance.initialize())
	{
		// There was an error during the initialization process
		return 1;
	}

	return app.exec();
}
