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

#pragma once

#include <QMainWindow>
#ifdef __APPLE__
#   include <QtMacExtras/QMacToolBar>
#else
#	include <QToolBar>
#endif
#include "object_editor.h"
#include "data/project.h"
#include "configuration.h"
#include <memory>

// Forwards
namespace Ui
{
	class MainWindow;
}

namespace wowpp
{
	namespace editor
	{
		/// Represents the main window of the editor application.
		class MainWindow : public QMainWindow
		{
			Q_OBJECT

		public:

			/// Initializes a new instance of the main window of the editor application.
			explicit MainWindow(Configuration &config, Project &project);

		private slots:

			// on_<ACTION_NAME>_<ACTION>() will be automatically linked with
			// the matching <ACTION_NAME> actions from the .ui file
			void on_actionObjectEditor_triggered();
			void on_actionLoadMap_triggered();
			void on_actionSave_triggered();
			void on_actionExit_triggered();

		private:

			void setupToolBar();

		private:

			Configuration &m_config;
			Project &m_project;
			Ui::MainWindow *m_ui;
			std::unique_ptr<ObjectEditor> m_objectEditor;
#ifdef __APPLE__
            std::unique_ptr<QMacToolBar> m_macToolBar;
#else
			std::unique_ptr<QToolBar> m_toolBar;
#endif
		};
	}
}
