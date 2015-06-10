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

#include "load_map.h"
#include "ui_load_map.h"
#include "data/map_entry.h"
#include "program.h"
#include "data/project.h"
#include "templates/basic_template.h"

namespace wowpp
{
	namespace editor
	{
		LoadMap::LoadMap(Project &project)
			: QDialog()
			, m_ui(new Ui::LoadMap)
			, m_project(project)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			// Setup list content
			const auto &templates = m_project.maps.getTemplates();
			for (const auto &map : templates)
			{
				m_ui->listWidget->addItem(
					QString("%1 - %2").arg(QString::number(map->id), 3, (QLatin1Char)'0').arg(map->name.c_str()));
			}

			// Not resizable
			setFixedSize(size());
			setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
		}

		void LoadMap::on_buttonBox_accepted()
		{
			auto selectedIndices = m_ui->listWidget->selectionModel()->selectedIndexes();
			if (selectedIndices.empty())
			{
				return;
			}

			const auto &templates = m_project.maps.getTemplates();
			
			// Get the id of the specified map
			auto mapId = templates.at(selectedIndices[0].row())->id;

			// Get the editable map
			auto *editable = m_project.maps.getEditableById(mapId);
			if (!editable)
				return;

			Program *program = static_cast<Program*>(QCoreApplication::instance());
			if (program)
			{
				program->openMap(*editable);
			}
		}

	}
}
