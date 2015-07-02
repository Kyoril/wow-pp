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

#include "editor_application.h"
#include "main_window.h"
#include "object_editor.h"
#include "trigger_editor.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QMessageBox>
#include <cassert>

namespace wowpp
{
	namespace editor
	{
		EditorApplication::EditorApplication()
			: QObject()
		{
		}

		bool EditorApplication::initialize()
		{
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
				return false;
			}

			// TODO: Create log file

			// Show the main window
			m_mainWindow.reset(new MainWindow(*this));

			// Move this window to the center of the screen manually, since without this, there seems to be a crash
			// in QtGui somewhere...
			QRect screen = QApplication::desktop()->availableGeometry();
			QRect win = m_mainWindow->geometry();
			m_mainWindow->setGeometry(QRect(screen.center().x() - win.size().width() / 2, screen.center().y() - win.size().height() / 2,
				win.size().width(), win.size().height()));

			// Show the window
			m_mainWindow->show();

			// Load the project
			if (!m_project.load(m_configuration.dataPath))
			{
				// Display error message
				QMessageBox::critical(
					nullptr,
					"Could not load data project",
					"There was an error loading the data project.\n\n"
					"For more details, please open the editor log file (usually wowpp_editor.log).");
				return false;
			}

			// Setup the model lists
			m_mapListModel.reset(new MapListModel(m_project.maps));
			m_unitListModel.reset(new UnitListModel(m_project.units));
			m_spellListModel.reset(new SpellListModel(m_project.spells));

			// Setup the object editor
			m_objectEditor.reset(new ObjectEditor(*this));

			// Setup the trigger editor
			m_triggerEditor.reset(new TriggerEditor(*this));
			
			return true;
		}

		void EditorApplication::showObjectEditor()
		{
			assert(m_objectEditor);

			m_objectEditor->show();
			m_objectEditor->activateWindow();

			emit objectEditorShown();
		}

		void EditorApplication::showTriggerEditor()
		{
			assert(m_triggerEditor);

			m_triggerEditor->show();
			m_triggerEditor->activateWindow();

			emit triggerEditorShown();
		}

		bool EditorApplication::shutdown()
		{
			// TODO: Check for unsaved changes
			
			// Shutdown the application
			qApp->quit();
			return true;
		}
}
}