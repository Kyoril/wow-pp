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
#include "load_map_dialog.h"
#include "ui_load_map_dialog.h"
#include "editor_application.h"

namespace wowpp
{
	namespace editor
	{
		LoadMapDialog::LoadMapDialog(EditorApplication &app)
			: QDialog()
			, m_ui(new Ui::LoadMapDialog)
			, m_app(app)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);
			m_ui->treeView->setModel(m_app.getMapListModel());
		}

		void LoadMapDialog::on_buttonBox_accepted()
		{
			// TODO
		}

		proto::MapEntry * LoadMapDialog::getSelectedMap() const
		{
			auto *sel = m_ui->treeView->selectionModel();
			if (!sel)
				return nullptr;

			auto current = sel->currentIndex();
			if (!current.isValid())
				return nullptr;

			auto &templates = m_app.getProject().maps.getTemplates();
			if (current.row() >= templates.entry_size())
				return nullptr;

			return templates.mutable_entry(current.row());
		}

	}
}
