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
			void showEffectDialog(const proto::SpellEffect &effect);

		private slots:

			void on_unitFilter_editingFinished();
			void on_spellFilter_editingFinished();
			void on_skillFilter_editingFinished();
			void on_itemFilter_editingFinished();
			void on_questFilter_editingFinished();
			void on_objectFilter_editingFinished();
			void on_unitAddTriggerBtn_clicked();
			void on_unitRemoveTriggerBtn_clicked();

			//questAcceptTriggerBox
			void on_questAcceptAddTriggerBtn_clicked();
			void on_questAcceptRemoveTriggerBtn_clicked();
			//questFailureTriggerBox
			void on_questFailureAddTriggerBtn_clicked();
			void on_questFailureRemoveTriggerBtn_clicked();
			//questRewardTriggerBox
			void on_questRewardAddTriggerBtn_clicked();
			void on_questRewardRemoveTriggerBtn_clicked();

			void on_objectAddTriggerBtn_clicked();
			void on_objectRemoveTriggerBtn_clicked();
			void onUnitSelectionChanged(const QItemSelection& selection, const QItemSelection& old);
			void on_unitPropertyWidget_doubleClicked(QModelIndex index);
			void on_objectPropertyWidget_doubleClicked(QModelIndex index);
			void on_treeWidget_doubleClicked(QModelIndex index);
			void onSpellSelectionChanged(const QItemSelection& selection, const QItemSelection& old);
			void onSkillSelectionChanged(const QItemSelection& selection, const QItemSelection& old);
			void onItemSelectionChanged(const QItemSelection& selection, const QItemSelection& old);
			void onQuestSelectionChanged(const QItemSelection& selection, const QItemSelection& old);
			void onObjectSelectionChanged(const QItemSelection& selection, const QItemSelection& old);
			void on_lootSimulatorButton_clicked();
			void on_objectLootSimulatorButton_clicked();
			void on_itemLootSimulatorButton_clicked();
			void on_skinLootSimulatorButton_clicked();
			void on_actionImport_triggered();
			void on_actionImport_Vendors_triggered();
			void on_actionImport_Trainers_triggered();
			void on_actionImport_Quests_triggered();
			void on_actionImport_Object_Loot_triggered();
			void on_actionImportSkinningLoot_triggered();
			void on_addUnitSpellButton_clicked();
			void on_removeUnitSpellButton_clicked();
			void on_actionImport_Object_Spawns_triggered();
			void on_actionImport_Item_Loot_triggered();
			void on_actionImport_Units_triggered();
			void on_actionImport_Gold_Loot_triggered();
			void on_actionImport_Items_triggered();
			void on_effectButton1_clicked();
			void on_effectButton2_clicked();
			void on_effectButton3_clicked();
			void on_unitAddVarBtn_clicked();
			void on_objectAddVarBtn_clicked();
			void onRaceClassChanged(int state);
			void onMechanicImmunityChanged(int state);
			void onSchoolImmunityChanged(int state);
			void on_spellPositiveBox_stateChanged(int state);

		private:
				
			EditorApplication &m_application;
			Ui::ObjectEditor *m_ui;
			QSortFilterProxyModel *m_unitFilter;
			QSortFilterProxyModel *m_spellFilter;
			QSortFilterProxyModel *m_skillFilter;
			QSortFilterProxyModel *m_itemFilter;
			QSortFilterProxyModel *m_questFilter;
			QSortFilterProxyModel *m_objectFilter;
			Properties m_properties;
			PropertyViewModel *m_viewModel;
			Properties m_objectProperties;
			PropertyViewModel *m_objectViewModel;
			Properties m_itemProperties;
			PropertyViewModel *m_itemViewModel;
			proto::UnitEntry *m_selectedUnit;
			proto::SpellEntry *m_selectedSpell;
			proto::SkillEntry *m_selectedSkill;
			proto::QuestEntry *m_selectedQuest;
			proto::ObjectEntry *m_selectedObject;
			proto::ItemEntry *m_selectedItem;
			bool m_selectionChanging;
		};
	}
}
