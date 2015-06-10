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
#include "log/log_std_stream.h"
#include "log/log_entry.h"
#include "log/default_log_levels.h"
#include <QMessageBox>

namespace wowpp
{
	namespace editor
	{
		Program::Program(int &argc, char** argv)
			: QApplication(argc, argv)
		{
			// Try to load the stylesheet
			QFile stylesheetFile("styles/Default.qss");
			if (stylesheetFile.open(QFile::ReadOnly))
			{
				// Read stylesheet content
				QString StyleSheet = QLatin1String(stylesheetFile.readAll());
				qApp->setStyleSheet(StyleSheet);
			}

			// Load the configuration
			if (!m_configuration.load("wowpp_editor.cfg"))
			{
				// Display error message
				QMessageBox::information(
					nullptr,
					"Could not load configuration file",
					"If this is your first time launching the editor, it means that the config "
					"file simply didn't exist and was created using the default values.\n\n"
					"Please check the values in wowpp_editor.cfg and try again.");
				return;
			}

			// The log files are written to in a special background thread
			std::ofstream logFile;
			LogStreamOptions logFileOptions = g_DefaultFileLogOptions;
			logFileOptions.alwaysFlush = !m_configuration.isLogFileBuffering;

			if (m_configuration.isLogActive)
			{
				const String fileName = m_configuration.logFileName;
				logFile.open(fileName.c_str(), std::ios::app);
				if (logFile)
				{
					m_genericLogConnection = g_DefaultLog.signal().connect(
						[&logFile, &logFileOptions](const LogEntry & entry)
					{
						printLogEntry(logFile, entry, logFileOptions);
					});
				}
				else
				{
					ELOG("Could not open log file '" << fileName << "'");
				}
			}

			// Load data project
			if (!m_project.load(m_configuration.dataPath))
			{
				// Display error message
				QMessageBox::critical(
					nullptr, 
					"Could not load data project", 
					"There was an error loading the data project.\n\n"
					"For more details, please open the editor log file (usually wowpp_editor.log).");
				return;
			}

			// Initialize graphics
			m_graphics.reset(new Graphics());

			// Setup application name
			setApplicationName("WoW++ Editor");

			// Create an instance of the main window of the editor application and
			// show it
			m_mainWindow.reset(new MainWindow(m_configuration, m_project));
			m_mainWindow->show();

			// Run main loop
			exec();
		}

		void Program::saveProject()
		{
			// Save data project
			if (!m_project.save(m_configuration.dataPath))
			{
				// Display error message
				QMessageBox::critical(
					nullptr,
					"Could not save data project",
					"There was an error saving the data project.\n\n"
					"For more details, please open the editor log file (usually wowpp_editor.log).");
				return;
			}
			else
			{
				QMessageBox::information(
					nullptr,
					"Data project saved",
					"The data project was successfully saved.");
			}
		}

		void Program::openMap(MapEntry &map)
		{
			m_worldEditor.reset(
				new WorldEditor(
					*m_graphics,
					*m_graphics->getSceneManager().getCamera("QOgreWidget_Cam"),
					map));
			m_worldEditor->update(0.1f);
		}


	}
}
