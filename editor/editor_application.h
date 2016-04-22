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

#pragma once

#include <QObject>
#include "proto_data/project.h"
#include "template_list_model.h"
#include "trigger_list_model.h"
#include "configuration.h"
#include "selection.h"

namespace wowpp
{
	namespace editor
	{
		namespace transform_tool
		{
			enum Type
			{
				Select,
				Translate,
				Rotate,
				Scale
			};
		}

		typedef transform_tool::Type TransformTool;

		class MainWindow;		// main_window.h
		class ObjectEditor;		// object_editor.h
		class TriggerEditor;	// trigger_editor.h

		/// Manages and contains all major application objects.
		class EditorApplication final : public QObject
		{
			Q_OBJECT

		public:

			typedef TemplateListModel<proto::ItemManager> ItemListModel;
			typedef TemplateListModel<proto::SpellManager> SpellListModel;
			typedef TemplateListModel<proto::MapManager> MapListModel;
			typedef TemplateListModel<proto::UnitManager> UnitListModel;
			typedef TemplateListModel<proto::QuestManager> QuestListModel;
			typedef TemplateListModel<proto::ObjectManager> ObjectListModel;

		public:

			boost::signals2::signal<void(TransformTool)> transformToolChanged;

		public:

			explicit EditorApplication();
            ~EditorApplication();

			/// Initializes our editor application (loads settings and sets everything up properly).
			/// @returns True if everything went okay, false otherwise.
			bool initialize();
			/// Tries to shutdown the running editor application. This will check for unsaved changes
			/// and asks the user to save them.
			/// @returns True, if the shutdown request was accepted, false otherwise.
			bool shutdown();

			ItemListModel *getItemListModel() { return m_itemListModel.get(); }
			SpellListModel *getSpellListModel() { return m_spellListModel.get(); }
			MapListModel *getMapListModel() { return m_mapListModel.get(); }
			UnitListModel *getUnitListModel() { return m_unitListModel.get(); }
			TriggerListModel *getTriggerListModel() { return m_triggerListModel.get(); }
			QuestListModel *getQuestListModel() { return m_questListModel.get(); }
			ObjectListModel *getObjectListModel() { return m_objectListModel.get(); }
			proto::Project &getProject() { return m_project; }
			Configuration &getConfiguration() { return m_configuration; }
			Selection &getSelection() { return m_selection; }
			const TransformTool &getTransformTool() const { return m_transformTool; }
			void setTransformTool(TransformTool tool);

		public slots:

			/// Displays the object editor window (and activates it if it is in the background).
			void showObjectEditor();
			/// 
			void showTriggerEditor();
			/// 
			void markAsChanged();
			/// 
			void saveUnsavedChanges();

		signals:

			/// Fired when the object editor window was showed or activated.
			void objectEditorShown();
			void triggerEditorShown();

		private:

			Selection m_selection;
			Configuration m_configuration;
			MainWindow *m_mainWindow;
			ObjectEditor *m_objectEditor;
			TriggerEditor *m_triggerEditor;
			proto::Project m_project;
			std::unique_ptr<ItemListModel> m_itemListModel;
			std::unique_ptr<SpellListModel> m_spellListModel;
			std::unique_ptr<MapListModel> m_mapListModel;
			std::unique_ptr<UnitListModel> m_unitListModel;
			std::unique_ptr<TriggerListModel> m_triggerListModel;
			std::unique_ptr<QuestListModel> m_questListModel;
			std::unique_ptr<ObjectListModel> m_objectListModel;
			TransformTool m_transformTool;
			bool m_changed;
		};
	}
}
