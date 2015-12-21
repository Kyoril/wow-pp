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

#include "action_dialog.h"
#include "ui_action_dialog.h"
#include "data/trigger_entry.h"
#include "data/project.h"
#include "templates/basic_template.h"
#include "editor_application.h"
#include "trigger_helper.h"
#include "text_dialog.h"
#include "target_dialog.h"
#include "data_dialog.h"
#include <QMessageBox>

namespace wowpp
{
	namespace editor
	{
		ActionDialog::ActionDialog(EditorApplication &app, proto::TriggerAction action/* = TriggerEntry::TriggerAction()*/)
			: QDialog()
			, m_ui(new Ui::ActionDialog)
			, m_app(app)
			, m_action(std::move(action))
		{
			// Setup auto generated ui
			m_ui->setupUi(this);
			m_ui->actionBox->setCurrentIndex(m_action.action());

			connect(m_ui->actionBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_actionBox_currentIndexChanged(int)));
			m_ui->actionTextLabel->setText(getTriggerActionText(m_app.getProject(), m_action, true));
		}

		void ActionDialog::on_buttonBox_accepted()
		{
			// TODO
		}

		void ActionDialog::on_actionBox_currentIndexChanged(int index)
		{
			if (m_action.action() != index)
			{
				m_action.set_action(index);
				m_action.clear_texts();
				m_action.clear_data();
				m_action.clear_targetname();

				m_ui->actionTextLabel->setText(getTriggerActionText(m_app.getProject(), m_action, true));
			}
		}

		void ActionDialog::on_actionTextLabel_linkActivated(const QString &link)
		{
			if (link == "target")
			{
				TargetDialog dialog(m_action.target(), m_action.targetname().c_str());
				auto result = dialog.exec();
				if (result == QDialog::Accepted)
				{
					m_action.set_target(dialog.getTarget());
					m_action.set_targetname(dialog.getTargetName().toStdString());
					m_ui->actionTextLabel->setText(getTriggerActionText(m_app.getProject(), m_action, true));
				}
			}
			else if (link.startsWith("data-"))
			{
				QString numString = link.right(link.size() - 5);
				int index = numString.toInt();

				if (m_action.data_size() <= index) m_action.mutable_data()->Resize(index + 1, 0);
				
				DataDialog dialog(m_app.getProject(), m_action.action(), index, m_action.data(index));
				auto result = dialog.exec();
				if (result == QDialog::Accepted)
				{
					m_action.mutable_data()->Set(index, dialog.getData());
					m_ui->actionTextLabel->setText(getTriggerActionText(m_app.getProject(), m_action, true));
				}
			}
			else if (link.startsWith("text-"))
			{
				QString numString = link.right(link.size() - 5);
				int index = numString.toInt();

				if (m_action.texts_size() <= index) m_action.mutable_texts()->Reserve(index + 1);
				TextDialog dialog(m_action.texts(index).c_str());
				auto result = dialog.exec();
				if (result == QDialog::Accepted)
				{
					*m_action.mutable_texts()->Mutable(index) = dialog.getText().toStdString();
					m_ui->actionTextLabel->setText(getTriggerActionText(m_app.getProject(), m_action, true));
				}
			}
			else
			{
				// Unhandled hyperlink type
			}
		}

	}
}
