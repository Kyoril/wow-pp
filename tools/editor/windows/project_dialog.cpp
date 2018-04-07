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
#include "project_dialog.h"
#include "ui_project_dialog.h"
#include "editor_application.h"
#include "configuration.h"
#include <QListWidget>

namespace wowpp
{
	namespace editor
	{
		ProjectDialog::ProjectDialog(EditorApplication& app)
			: QDialog()
			, m_ui(new Ui::ProjectDialog)
			, m_app(app)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			for (const auto& project : app.getConfiguration().projects)
			{
				m_ui->listWidget->addItem(project.name.c_str());
			}
		}

		void ProjectDialog::on_buttonBox_accepted()
		{
			int selectedRow = m_ui->listWidget->currentIndex().row();
			if (selectedRow < 0 || selectedRow >= m_app.getConfiguration().projects.size())
			{
				return;
			}

			// Set the active project
			m_app.setActiveProject(m_app.getConfiguration().projects[selectedRow]);
		}

	}
}
