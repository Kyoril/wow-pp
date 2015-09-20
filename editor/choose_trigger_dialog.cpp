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

#include "choose_trigger_dialog.h"
#include "ui_choose_trigger_dialog.h"
#include "data/trigger_entry.h"
#include "data/project.h"
#include "templates/basic_template.h"
#include "editor_application.h"

namespace wowpp
{
	namespace editor
	{
		ChooseTriggerDialog::ChooseTriggerDialog(EditorApplication &app)
			: QDialog()
			, m_ui(new Ui::ChooseTriggerDialog)
			, m_app(app)
			, m_selected(nullptr)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);
			m_ui->triggerView->setModel(m_app.getTriggerListModel());

			connect(m_ui->triggerView->selectionModel(),
				SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
				this, SLOT(onTriggerSelectionChanged(QItemSelection, QItemSelection)));
		}

		void ChooseTriggerDialog::on_buttonBox_accepted()
		{
			// TODO
		}

		void ChooseTriggerDialog::onTriggerSelectionChanged(const QItemSelection& selection, const QItemSelection& old)
		{
			// Get the selected unit
			if (selection.isEmpty())
			{
				m_selected = nullptr;
				return;
			}

			int index = selection.indexes().first().row();
			if (index < 0)
			{
				m_selected = nullptr;
				return;
			}

			// Get trigger entry
			const auto *trigger = m_app.getProject().triggers.getTemplates()[index].get();
			m_selected = trigger;
		}

	}
}
