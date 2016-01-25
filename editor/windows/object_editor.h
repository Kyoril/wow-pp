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

#include <QMainWindow>
#include <QSortFilterProxyModel>
#include <QAbstractTableModel>
#include <QItemSelection>
#include <QTreeWidgetItem>
#include "proto_data/project.h"
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
		class EditorApplication;

		/// 
		class ObjectEditor final : public QMainWindow
		{
			Q_OBJECT

		public:

			explicit ObjectEditor(EditorApplication &app);

		private:

			void addLootItem(const proto::LootDefinition &def, QTreeWidgetItem *parent);
			void addSpellEntry(const proto::UnitSpellEntry &creatureSpell);

		private slots:

			void on_unitFilter_editingFinished();
			void on_spellFilter_editingFinished();
			void on_itemFilter_editingFinished();
			void on_unitAddTriggerBtn_clicked();
			void on_unitRemoveTriggerBtn_clicked();
			void onUnitSelectionChanged(const QItemSelection& selection, const QItemSelection& old);
			void on_unitPropertyWidget_doubleClicked(QModelIndex index);
			void onSpellSelectionChanged(const QItemSelection& selection, const QItemSelection& old);
			void onItemSelectionChanged(const QItemSelection& selection, const QItemSelection& old);
			void on_lootSimulatorButton_clicked();
			void on_actionImport_triggered();
			void on_actionImport_Vendors_triggered();
			void on_actionImport_Trainers_triggered();
			void on_addUnitSpellButton_clicked();
			void on_removeUnitSpellButton_clicked();

		private:
				
			EditorApplication &m_application;
			Ui::ObjectEditor *m_ui;
			QSortFilterProxyModel *m_unitFilter;
			QSortFilterProxyModel *m_spellFilter;
			QSortFilterProxyModel *m_itemFilter;
			Properties m_properties;
			PropertyViewModel *m_viewModel;
			proto::UnitEntry *m_selected;
		};
	}
}
