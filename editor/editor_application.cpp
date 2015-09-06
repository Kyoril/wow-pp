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
		template<>
		QVariant TemplateListModel<MapEntryManager>::data(const QModelIndex &index, int role) const
		{
			if (!index.isValid())
				return QVariant();

			if (index.row() >= m_entries.getTemplates().size())
				return QVariant();

			if (role == Qt::DisplayRole)
			{
				const auto &templates = m_entries.getTemplates();
				const auto &tpl = templates[index.row()];

				if (index.column() == 0)
				{
					return QString("%1 %2").arg(QString::number(tpl->id), 3, QLatin1Char('0')).arg(tpl->name.c_str());
				}
				else
				{
					return QString(constant_literal::mapInstanceType.getName(tpl->instanceType).c_str());
				}
			}

			return QVariant();
		}

		template<>
		int TemplateListModel<MapEntryManager>::columnCount(const QModelIndex &parent) const
		{
			return 2;
		}

		template<>
		QVariant TemplateListModel<MapEntryManager>::headerData(int section, Qt::Orientation orientation, int role) const
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
			, m_changed(false)
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
			m_itemListModel.reset(new ItemListModel(m_project.items));
			m_triggerListModel.reset(new TriggerListModel(m_project.triggers));

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