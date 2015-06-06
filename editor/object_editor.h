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

#include "common/typedefs.h"
#include <QMainWindow>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QAbstractTableModel>
#include "data/project.h"
#include "property_view_model.h"
#include <memory>

// Forwards
namespace Ui
{
	class ObjectEditor;
}

namespace wowpp
{
	namespace editor
	{
		class MainWindow;

		/// Represents the main window of the editor application.
		class ObjectEditor : public QMainWindow
		{
			Q_OBJECT

		public:

			/// Initializes a new instance of the main window of the editor application.
			explicit ObjectEditor(Project &project);

		private slots:

			void on_unitsTreeWidget_currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*);
			void on_unitPropertyWidget_doubleClicked(QModelIndex);
			void on_actionCreateUnit_triggered();
			void on_actionSave_triggered();

			void on_lineEdit_textChanged(const QString &);

		private:

			Project &m_project;
			Ui::ObjectEditor *ui;
			Properties m_properties;
			std::unique_ptr<PropertyViewModel> m_viewModel;
		};
	}
}
