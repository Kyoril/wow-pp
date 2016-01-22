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

#include "data_dialog.h"
#include "ui_data_dialog.h"
#include "proto_data/project.h"
#include "proto_data/trigger_helper.h"

namespace wowpp
{
	namespace editor
	{
		DataDialog::DataDialog(proto::Project &project, UInt32 action, UInt32 index, UInt32 data)
			: QDialog()
			, m_ui(new Ui::DataDialog)
			, m_project(project)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);
			m_ui->dataField->setText(QString("%1").arg(data));
			m_ui->dataField->setValidator(new QIntValidator(0, 999999, this));

			bool mayChoose = false;
			switch (action)
			{
			case trigger_actions::CastSpell:	// SpellId
			case trigger_actions::Trigger:		// TriggerId
				mayChoose = (index == 0);
				break;
			default:
				break;
			}

			m_ui->dataButton->setVisible(mayChoose);
		}

		void DataDialog::on_buttonBox_accepted()
		{
			// TODO
		}

		UInt32 DataDialog::getData() const
		{
			return m_ui->dataField->text().toInt();
		}

	}
}
