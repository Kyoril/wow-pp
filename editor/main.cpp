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

#include <QApplication>
#include <QTextStream>
#include <QFile>
#include "common/background_worker.h"
#include "log/log_std_stream.h"
#include "log/log_entry.h"
#include "log/default_log_levels.h"
#include "editor_application.h"
#include "main_window.h"
#include "ui_main_window.h"
#include "object_editor.h"
#include "ui_object_editor.h"
#include "trigger_editor.h"
#include "ui_trigger_editor.h"

using namespace wowpp::editor;

/// Procedural entry point of the application.
int main(int argc, char *argv[])
{
	// Create the qt application instance and set it all up
	QApplication app(argc, argv);
	
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

	boost::signals2::scoped_connection genericLogConnection;
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
	EditorApplication editorAppInstance;
	if (!editorAppInstance.initialize())
	{
		// There was an error during the initialization process
		return 1;
	}

	return app.exec();
}
