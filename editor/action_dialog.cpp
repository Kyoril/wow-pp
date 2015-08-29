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

#include "action_dialog.h"
#include "ui_action_dialog.h"
#include "data/trigger_entry.h"
#include "data/project.h"
#include "templates/basic_template.h"
#include "editor_application.h"
#include "trigger_helper.h"

namespace wowpp
{
	namespace editor
	{
		ActionDialog::ActionDialog(EditorApplication &app)
			: QDialog()
			, m_ui(new Ui::ActionDialog)
			, m_app(app)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			m_action.action = trigger_actions::Trigger;
			m_action.target = trigger_action_target::None;

			connect(m_ui->actionBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_actionBox_currentIndexChanged(int)));
			on_actionBox_currentIndexChanged(0);
		}

		void ActionDialog::on_buttonBox_accepted()
		{
			// TODO
		}

		void ActionDialog::on_actionBox_currentIndexChanged(int index)
		{
			m_action.action = index;
			m_action.texts.clear();
			m_action.data.clear();
			m_action.targetName.clear();

			m_ui->actionTextLabel->setText(getTriggerActionText(m_app.getProject(), m_action, true));
		}

	}
}
