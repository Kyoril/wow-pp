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
#include "editor_application.h"
#include "windows/main_window.h"
#include "windows/object_editor.h"
#include "windows/trigger_editor.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QMessageBox>
#include <cassert>

namespace wowpp
{
	namespace editor
	{
		template<>
		QVariant TemplateListModel<proto::ItemManager>::data(const QModelIndex &index, int role) const
		{
			if (!index.isValid())
				return QVariant();

			if (index.row() >= m_entries.getTemplates().entry_size())
				return QVariant();

			const auto &templates = m_entries.getTemplates();
			const auto &tpl = templates.entry(index.row());

			if (role == Qt::DisplayRole)
			{
				if (index.column() == 0)
				{
					return QString("%1 %2").arg(QString::number(tpl.id()), 5, QLatin1Char('0')).arg(tpl.name().c_str());
				}
			}
			else if (role == Qt::ForegroundRole)
			{
				switch (tpl.quality())
				{
					case 0:
						return QColor(Qt::gray);
					case 1:
						return QColor(Qt::white);
					case 2:
						return QColor(Qt::green);
					case 3:
						return QColor(0, 114, 198);
					case 4:
						return QColor(Qt::magenta);
					case 5:
						return QColor(Qt::yellow);
					default:
						return QColor(Qt::red);
				}
			}

			return QVariant();
		}

		template<>
		QVariant TemplateListModel<proto::MapManager>::data(const QModelIndex &index, int role) const
		{
			if (!index.isValid())
				return QVariant();

			if (index.row() >= m_entries.getTemplates().entry_size())
				return QVariant();

			if (role == Qt::DisplayRole)
			{
				const auto &templates = m_entries.getTemplates();
				const proto::MapManager::EntryType &tpl = templates.entry(index.row());

				if (index.column() == 0)
				{
					return QString("%1 %2").arg(QString::number(tpl.id()), 3, QLatin1Char('0')).arg(tpl.name().c_str());
				}
				else
				{
					return QString("TODO"); //constant_literal::mapInstanceType.getName(static_cast<MapInstanceType>(tpl.instancetype())).c_str());
				}
			}

			return QVariant();
		}

		template<>
		int TemplateListModel<proto::MapManager>::columnCount(const QModelIndex &parent) const
		{
			return 2;
		}

		template<>
		QVariant TemplateListModel<proto::MapManager>::headerData(int section, Qt::Orientation orientation, int role) const
		{
			if (role != Qt::DisplayRole)
				return QVariant();

			if (orientation == Qt::Horizontal)
			{
				switch (section)
				{
				case 0:
					return QString("Name");
				default:
					return QString("Type");
				}
			}
			else
			{
				return QString("Row %1").arg(section);
			}
		}

		EditorApplication::EditorApplication()
			: QObject()
            , m_mainWindow(nullptr)
            , m_objectEditor(nullptr)
            , m_triggerEditor(nullptr)
			, m_changed(false)
			, m_transformTool(transform_tool::Select)
		{
		}

        EditorApplication::~EditorApplication()
        {
            delete m_triggerEditor;
            delete m_objectEditor;
            delete m_mainWindow;
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
			m_itemListModel.reset(new ItemListModel(m_project.items));
			m_triggerListModel.reset(new TriggerListModel(m_project.triggers));
			m_questListModel.reset(new QuestListModel(m_project.quests));
			m_objectListModel.reset(new ObjectListModel(m_project.objects));

			// Show the main window (will be deleted when this class is deleted by QT)
			m_mainWindow = new MainWindow(*this);

			// Move this window to the center of the screen manually, since without this, there seems to be a crash
			// in QtGui somewhere...
			QRect screen = QApplication::desktop()->availableGeometry();
			QRect win = m_mainWindow->geometry();
			m_mainWindow->setGeometry(QRect(screen.center().x() - win.size().width() / 2, screen.center().y() - win.size().height() / 2,
				win.size().width(), win.size().height()));

			// Show the window
			m_mainWindow->show();

			// Setup the object editor
			m_objectEditor = new ObjectEditor(*this);

			// Setup the trigger editor
			m_triggerEditor = new TriggerEditor(*this);
			
			return true;
		}

		void EditorApplication::showObjectEditor()
		{
			assert(m_objectEditor);

			m_objectEditor->show();
			m_objectEditor->activateWindow();

			emit objectEditorShown();
		}

		void EditorApplication::setTransformTool(TransformTool tool)
		{
			m_transformTool = tool;
			transformToolChanged(tool);
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
			// Check for unsaved changes
			if (m_changed)
			{
				int result = QMessageBox::warning(
					nullptr,
					"You have unsaved changes",
					"Do you want to save them now? If not, you will loose all changes!",
					QMessageBox::Save, QMessageBox::No, QMessageBox::Cancel);
				if (result == QMessageBox::Cancel)
				{
					// Cancelled by user
					return false;
				}
				else if (result == QMessageBox::Save)
				{
					// Save project
					if (!m_project.save(m_configuration.dataPath))
					{
						// Display error message
						QMessageBox::critical(
							nullptr,
							"Could not save data project",
							"There was an error saving the data project.\n\n"
							"For more details, please open the editor log file (usually wowpp_editor.log).");

						// Cancel shutdown since there was an error
						return false;
					}
				}
			}
			
			// Shutdown the application
			qApp->quit();
			return true;
		}

		void EditorApplication::markAsChanged()
		{
			m_changed = true;
		}

		void EditorApplication::saveUnsavedChanges()
		{
			// Optimization
			/*if (!m_changed)
				return;*/

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

			// No more unsaved changes
			m_changed = false;
		}
}
}