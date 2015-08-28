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
namespace wowpp
{
	namespace editor
	{
		TriggerEditor::TriggerEditor(EditorApplication &app)
			: QMainWindow()
			, m_application(app)
			, m_ui(new Ui::TriggerEditor())
		{
			m_ui->setupUi(this);

			// Load trigger display
			m_listModel = new TriggerListModel(m_application.getProject().triggers);
			m_ui->triggerView->setModel(m_listModel);

			connect(m_ui->actionSave, SIGNAL(triggered()), &m_application, SLOT(saveUnsavedChanges()));

			connect(m_ui->triggerView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onTriggerSelectionChanged(QItemSelection, QItemSelection)));
		}

		void TriggerEditor::onTriggerSelectionChanged(const QItemSelection& selection, const QItemSelection& old)
		{
			// Get the selected unit
			if (selection.isEmpty())
				return;

			int index = selection.indexes().first().row();
			if (index < 0)
			{
				return;
			}

			// Get trigger entry
			auto *trigger = m_application.getProject().triggers.getTemplates().at(index).get();
			if (!trigger)
				return;

			auto *rootItem = m_ui->functionView->topLevelItem(0);
			if (rootItem)
			{
				rootItem->setText(0, trigger->name.c_str());
			}

			auto *eventItem = rootItem->child(0);
			if (eventItem)
			{
				qDeleteAll(eventItem->takeChildren());
			}

			{
				QTreeWidgetItem *item = new QTreeWidgetItem();
				item->setData(0, Qt::DisplayRole, QString("Unit %1 %2")
					.arg("(Triggering unit)").arg("(is entering combat)").arg(5774));
				item->setData(0, Qt::DecorationRole, QImage(":/Units.png"));
				eventItem->addChild(item);
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
			}

			{
				QTreeWidgetItem *item = new QTreeWidgetItem();
				item->setData(0, Qt::DisplayRole, QString("Unit - Make %1 yell %2 and play sound %3")
					.arg("(Triggering unit)").arg("\"VanCleef pay big for your heads!\"").arg(5774));
				item->setData(0, Qt::DecorationRole, QImage(":/Trade_Engineering.png"));
				actionItem->addChild(item);
			}
		}

	}
}
