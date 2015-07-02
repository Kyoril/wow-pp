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

#include <QObject>
#include "data/project.h"
#include "template_list_model.h"
#include "configuration.h"
#include <memory>

namespace wowpp
{
	namespace editor
	{
		class MainWindow;		// main_window.h
		class ObjectEditor;		// object_editor.h

		/// Manages and contains all major application objects.
		class EditorApplication final : public QObject
		{
			Q_OBJECT

		public:

			typedef TemplateListModel<SpellEntryManager> SpellListModel;
			typedef TemplateListModel<MapEntryManager> MapListModel;
			typedef TemplateListModel<UnitEntryManager> UnitListModel;

		public:

			explicit EditorApplication();

			/// Initializes our editor application (loads settings and sets everything up properly).
			/// @returns True if everything went okay, false otherwise.
			bool initialize();
			/// Tries to shutdown the running editor application. This will check for unsaved changes
			/// and asks the user to save them.
			/// @returns True, if the shutdown request was accepted, false otherwise.
			bool shutdown();

			SpellListModel *getSpellListModel() { return m_spellListModel.get(); }
			MapListModel *getMapListModel() { return m_mapListModel.get(); }
			UnitListModel *getUnitListModel() { return m_unitListModel.get(); }

		public slots:

			/// Displays the object editor window (and activates it if it is in the background).
			void showObjectEditor();

		signals:

			/// Fired when the object editor window was showed or activated.
			void objectEditorShowed();

		private:

			Configuration m_configuration;
			std::unique_ptr<MainWindow> m_mainWindow;
			std::unique_ptr<ObjectEditor> m_objectEditor;
			Project m_project;
			std::unique_ptr<SpellListModel> m_spellListModel;
			std::unique_ptr<MapListModel> m_mapListModel;
			std::unique_ptr<UnitListModel> m_unitListModel;
		};
	}
}
