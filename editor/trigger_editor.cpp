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

#include "trigger_editor.h"
#include "main_window.h"	// Needed because of forward declaration with unique_ptr in EditorApplication
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
				m_selectedTrigger = nullptr;
				return;
			}

			int index = selection.indexes().first().row();
			if (index < 0)
			{
				m_selectedTrigger = nullptr;
				return;
			}

			// Get trigger entry
			auto *trigger = m_application.getProject().triggers.getTemplates()[index].get();
			m_selectedTrigger = trigger;
			if (!trigger)
				return;

			m_ui->triggerNameBox->setText(trigger->name.c_str());
			m_ui->triggerPathBox->setText((trigger->path.empty() ? "(Default)" : trigger->path.c_str()));
			m_ui->splitter->setEnabled(true);

			auto *rootItem = m_ui->functionView->topLevelItem(0);
			if (rootItem)
			{
				rootItem->setText(0, trigger->name.c_str());
			}

			auto *eventItem = rootItem->child(0);
			if (eventItem)
			{
				qDeleteAll(eventItem->takeChildren());

				for (const auto &e : trigger->events)
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

				for (const auto &action : trigger->actions)
				{
					QTreeWidgetItem *item = new QTreeWidgetItem();

					item->setData(0, Qt::DisplayRole, getTriggerActionText(m_application.getProject(), action));
					item->setData(0, Qt::DecorationRole, QImage(":/Trade_Engineering.png"));

					actionItem->addChild(item);
				}

				actionItem->setExpanded(true);
			}

			rootItem->setExpanded(true);
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

			std::unique_ptr<TriggerEntry> newTrigger(new TriggerEntry());
			newTrigger->id = newId;
			newTrigger->name = "New Trigger";

			m_application.getProject().triggers.add(
				std::move(newTrigger));

			// This will update all views
			emit m_application.getTriggerListModel()->layoutChanged();

			// Select our new trigger
			QModelIndex last = m_application.getTriggerListModel()->index(proj.triggers.getTemplates().size() - 1);
			m_ui->triggerView->setCurrentIndex(last);

			m_application.markAsChanged();
		}

		void TriggerEditor::on_actionAddEvent_triggered()
		{
			if (!m_selectedTrigger)
				return;

			EventDialog dialog(m_application);
			auto result = dialog.exec();
			if (result == QDialog::Accepted)
			{
				// TODO: Add event
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
				// TODO: Add event
			}
		}

		void TriggerEditor::on_triggerNameBox_editingFinished()
		{
			if (!m_selectedTrigger)
				return;

			// Rename
			m_selectedTrigger->name = m_ui->triggerNameBox->text().toStdString();

			// This will update all views
			emit m_application.getTriggerListModel()->layoutChanged();
			m_application.markAsChanged();
		}

		void TriggerEditor::on_triggerPathBox_editingFinished()
		{
			if (!m_selectedTrigger)
				return;

			// Move
			m_selectedTrigger->path = m_ui->triggerPathBox->text().toStdString();

			// This will update all views
			emit m_application.getTriggerListModel()->layoutChanged();
			m_application.markAsChanged();
		}

	}
}
