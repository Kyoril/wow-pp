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

#include "trigger_editor.h"
#include "windows/main_window.h"
#include "editor_application.h"
#include "ui_trigger_editor.h"
#include "event_dialog.h"
#include "action_dialog.h"
#include "trigger_helper.h"

namespace wowpp
{
	namespace editor
	{
		TriggerEditor::TriggerEditor(EditorApplication &app)
			: QMainWindow()
			, m_application(app)
			, m_ui(new Ui::TriggerEditor())
			, m_selectedTrigger(nullptr)
		{
			m_ui->setupUi(this);

			// Load trigger display
			m_ui->triggerView->setModel(m_application.getTriggerListModel());
			updateSelection(false);

			connect(m_ui->actionSave, SIGNAL(triggered()), &m_application, SLOT(saveUnsavedChanges()));
			connect(m_ui->triggerView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onTriggerSelectionChanged(QItemSelection, QItemSelection)));
		}

		void TriggerEditor::onTriggerSelectionChanged(const QItemSelection& selection, const QItemSelection& old)
		{
			// Get the selected unit
			if (selection.isEmpty())
			{
				updateSelection(false);
				m_selectedTrigger = nullptr;
				return;
			}

			int index = selection.indexes().first().row();
			if (index < 0)
			{
				updateSelection(false);
				m_selectedTrigger = nullptr;
				return;
			}

			// Get trigger entry
			auto *trigger = m_application.getProject().triggers.getTemplates().mutable_entry(index);
			m_selectedTrigger = trigger;
			if (!trigger)
			{
				updateSelection(false);
				return;
			}

			m_ui->triggerNameBox->setText(trigger->name().c_str());
			m_ui->triggerPathBox->setText((trigger->category().empty() ? "(Default)" : trigger->category().c_str()));
			m_ui->splitter->setEnabled(true);

			auto *rootItem = m_ui->functionView->topLevelItem(0);
			if (rootItem)
			{
				rootItem->setText(0, trigger->name().c_str());
			}

			auto *eventItem = rootItem->child(0);
			if (eventItem)
			{
				qDeleteAll(eventItem->takeChildren());

				for (const auto &e : trigger->events())
				{
					QTreeWidgetItem *item = new QTreeWidgetItem();

					item->setData(0, Qt::DisplayRole, getTriggerEventText(e));
					item->setData(0, Qt::DecorationRole, QImage(":/Units.png"));

					eventItem->addChild(item);
				}

				eventItem->setExpanded(true);
			}

			auto *conditionItem = rootItem->child(1);
			if (conditionItem)
			{
				qDeleteAll(conditionItem->takeChildren());
			}

			auto *actionItem = rootItem->child(2);
			if (actionItem)
			{
				qDeleteAll(actionItem->takeChildren());

				for (const auto &action : trigger->actions())
				{
					QTreeWidgetItem *item = new QTreeWidgetItem();

					item->setData(0, Qt::DisplayRole, getTriggerActionText(m_application.getProject(), action));
					item->setData(0, Qt::DecorationRole, QImage(":/Trade_Engineering.png"));

					actionItem->addChild(item);
				}

				actionItem->setExpanded(true);
			}

			rootItem->setExpanded(true);
			updateSelection(true);
		}

		void TriggerEditor::on_actionNewTrigger_triggered()
		{
			auto &proj = m_application.getProject();

			const UInt32 maxId = std::numeric_limits<UInt32>::max();

			// Determine highest trigger id
			UInt32 newId = 1;
			while (proj.triggers.getById(newId) && newId != maxId)
			{
				newId++;
			}

			if (newId == maxId)
			{
				// TODO
				return;
			}

			auto *newTrigger = m_application.getProject().triggers.add(newId);
			newTrigger->set_name("New Trigger");

			// This will update all views
			emit m_application.getTriggerListModel()->layoutChanged();

			// Select our new trigger
			QModelIndex last = m_application.getTriggerListModel()->index(proj.triggers.getTemplates().entry_size() - 1);
			m_ui->triggerView->setCurrentIndex(last);

			m_application.markAsChanged();
		}

		void TriggerEditor::updateSelection(bool enabled)
		{
			m_ui->actionRemove->setEnabled(enabled);
			m_ui->actionAddAction->setEnabled(enabled);
			m_ui->actionAddEvent->setEnabled(enabled);
			m_ui->frame->setEnabled(enabled);
			m_ui->frame_2->setEnabled(enabled);
		}

		void TriggerEditor::on_actionAddEvent_triggered()
		{
			if (!m_selectedTrigger)
				return;

			EventDialog dialog(m_application);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				m_selectedTrigger->mutable_events()->Add(dialog.getEvent());

				auto *rootItem = m_ui->functionView->topLevelItem(0);
				if (rootItem)
				{
					auto *eventItem = rootItem->child(0);
					if (eventItem)
					{
						QTreeWidgetItem *item = new QTreeWidgetItem();
						item->setData(0, Qt::DisplayRole, getTriggerEventText(dialog.getEvent()));
						item->setData(0, Qt::DecorationRole, QImage(":/Units.png"));
						eventItem->addChild(item);
					}
				}

				m_application.markAsChanged();
			}
		}

		void TriggerEditor::on_actionAddAction_triggered()
		{
			if (!m_selectedTrigger)
				return;

			ActionDialog dialog(m_application);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				auto *added = m_selectedTrigger->mutable_actions()->Add();
				*added = dialog.getAction();

				auto *rootItem = m_ui->functionView->topLevelItem(0);
				if (rootItem)
				{
					auto *actionItem = rootItem->child(2);
					if (actionItem)
					{
						QTreeWidgetItem *item = new QTreeWidgetItem();
						item->setData(0, Qt::DisplayRole, getTriggerActionText(m_application.getProject(), dialog.getAction()));
						item->setData(0, Qt::DecorationRole, QImage(":/Trade_Engineering.png"));
						actionItem->addChild(item);
					}
				}

				m_application.markAsChanged();
			}
		}

		void TriggerEditor::on_actionRemove_triggered()
		{
			if (!m_selectedTrigger)
				return;

			// Unlink trigger
			auto &unitEntries = m_application.getProject().units.getTemplates().entry();
			//for (auto &unit : unitEntries)
			//{
				//unit->unlinkTrigger(m_selectedTrigger->id);
			//}
			
			// Remove selected trigger
			m_application.getProject().triggers.remove(m_selectedTrigger->id());
			m_selectedTrigger = nullptr;

			// This will update all views
			emit m_application.getTriggerListModel()->layoutChanged();
			m_application.markAsChanged();

			// Update UI
			auto rows = m_ui->triggerView->selectionModel()->selectedRows();
			auto selection = m_ui->triggerView->selectionModel()->selection();
			if (rows.empty())
			{
				selection = QItemSelection();
			}

			onTriggerSelectionChanged(
				selection, 
				selection);
		}

		void TriggerEditor::on_triggerNameBox_editingFinished()
		{
			if (!m_selectedTrigger)
				return;

			// Rename
			m_selectedTrigger->set_name(m_ui->triggerNameBox->text().toStdString());

			// This will update all views
			emit m_application.getTriggerListModel()->layoutChanged();
			m_application.markAsChanged();
		}

		void TriggerEditor::on_triggerPathBox_editingFinished()
		{
			if (!m_selectedTrigger)
				return;

			// Move
			m_selectedTrigger->set_category(m_ui->triggerPathBox->text().toStdString());

			// This will update all views
			emit m_application.getTriggerListModel()->layoutChanged();
			m_application.markAsChanged();
		}

		void TriggerEditor::on_functionView_itemDoubleClicked(QTreeWidgetItem* item, int column)
		{
			if (!item)
				return;

			if (!m_selectedTrigger)
				return;

			auto *rootItem = m_ui->functionView->topLevelItem(0);
			if (!rootItem)
				return;

			auto *parent = item->parent();
			if (!parent)
				return;

			if (parent == rootItem->child(0))
			{
				int index = parent->indexOfChild(item);
				if (index >= m_selectedTrigger->events_size())
					return;

				// Event clicked
				EventDialog dialog(m_application, m_selectedTrigger->events(index));
				auto result = dialog.exec();
				if (result == QDialog::Accepted)
				{
					m_selectedTrigger->mutable_events()->Set(index, dialog.getEvent());
					item->setData(0, Qt::DisplayRole, getTriggerEventText(dialog.getEvent()));
					m_application.markAsChanged();
				}
			}
			else if (parent == rootItem->child(2))
			{
				int index = parent->indexOfChild(item);
				if (index >= m_selectedTrigger->actions_size())
					return;

				// Action clicked
				ActionDialog dialog(m_application, m_selectedTrigger->actions(index));
				auto result = dialog.exec();
				if (result == QDialog::Accepted)
				{
					*m_selectedTrigger->mutable_actions()->Mutable(index) = dialog.getAction();
					item->setData(0, Qt::DisplayRole, getTriggerActionText(m_application.getProject(), dialog.getAction()));
					m_application.markAsChanged();
				}
			}
		}
	}
}
