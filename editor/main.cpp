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

#include <QApplication>
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
