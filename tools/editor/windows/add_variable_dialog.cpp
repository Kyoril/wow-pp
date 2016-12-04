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
#include "add_variable_dialog.h"
#include "ui_add_variable_dialog.h"
#include "proto_data/project.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace editor
	{
		AddVariableDialog::AddVariableDialog(proto::Project &project)
			: QDialog()
			, m_ui(new Ui::AddVariableDialog)
			, m_project(project)
			, m_addedId(0)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);
		}

		void AddVariableDialog::on_buttonBox_accepted()
		{
			const UInt32 maxId = std::numeric_limits<UInt32>::max();

			// Determine highest trigger id
			UInt32 newId = 1;
			while (m_project.variables.getById(newId) && newId != maxId)
			{
				newId++;
			}

			if (newId == maxId)
			{
				ELOG("No free id found to use for new variable!");
				return;
			}

			// Create new variable
			auto *added = m_project.variables.add(newId);
			if (!added)
			{
				ELOG("Could not add new variable!");
				return;
			}

			m_addedId = newId;

			// Setup variable values
			added->set_name(m_ui->nameBox->text().toStdString().c_str());
			switch (m_ui->typeBox->currentIndex())
			{
				case 0:
					added->set_intvalue(m_ui->defaultBox->text().toInt());
					break;
				case 1:
					added->set_longvalue(m_ui->defaultBox->text().toLong());
					break;
				case 2:
					added->set_floatvalue(m_ui->defaultBox->text().toFloat());
					break;
				case 3:
					added->set_stringvalue(m_ui->defaultBox->text().toStdString());
					break;
				default:
					WLOG("Invalid index - variable will default to integer type!");
					added->set_intvalue(0);
					break;
			}
		}
	}
}
