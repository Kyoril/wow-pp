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
#include <QApplication>
#include <cassert>

namespace wowpp
{
	namespace editor
	{
		EditorApplication::EditorApplication()
			: QObject()
		{
			m_mainWindow.reset(new MainWindow(*this));
			m_objectEditor.reset(new ObjectEditor(*this));
		}

		bool EditorApplication::initialize()
		{
			// Show the main window
			m_mainWindow->show();

			return true;
		}

		void EditorApplication::showObjectEditor()
		{
			assert(m_objectEditor);

			m_objectEditor->show();
			m_objectEditor->activateWindow();

			emit objectEditorShowed();
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