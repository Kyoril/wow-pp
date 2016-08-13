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

#include "pch.h"
#include "target_dialog.h"
#include "ui_target_dialog.h"
#include "proto_data/trigger_helper.h"

namespace wowpp
{
	namespace editor
	{
		TargetDialog::TargetDialog(UInt32 target, const QString &text)
			: QDialog()
			, m_ui(new Ui::TargetDialog)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			m_ui->targetTypeBox->setCurrentIndex(target);
			m_ui->targetNameField->setText(text);
			on_targetTypeBox_currentIndexChanged(target);
		}

		void TargetDialog::on_buttonBox_accepted()
		{
			// TODO
		}

		QString TargetDialog::getTargetName() const
		{
			return m_ui->targetNameField->text();
		}

		UInt32 TargetDialog::getTarget() const
		{
			return m_ui->targetTypeBox->currentIndex();
		}

		void TargetDialog::on_targetTypeBox_currentIndexChanged(int index)
		{
			m_ui->targetNameField->setVisible(index == trigger_action_target::NamedCreature || index == trigger_action_target::NamedWorldObject);
		}

	}
}
