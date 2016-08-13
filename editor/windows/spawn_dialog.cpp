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
#include "spawn_dialog.h"
#include "ui_spawn_dialog.h"
#include "editor_application.h"
#include "proto_data/project.h"
#include "common/utilities.h"
#include "game/game_unit.h"

namespace wowpp
{
	namespace editor
	{
		SpawnDialog::SpawnDialog(EditorApplication &app, proto::UnitSpawnEntry &spawn)
			: QDialog()
			, m_ui(new Ui::SpawnDialog)
			, m_app(app)
			, m_spawn(spawn)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			// Initialize values
			m_ui->checkBox->setChecked(spawn.isactive());
			m_ui->spawnNameBox->setChecked(spawn.has_name());
			m_ui->lineEdit->setText(spawn.name().c_str());
			m_ui->checkBox_2->setChecked(spawn.respawn());
			m_ui->spinBox->setValue(spawn.respawndelay());
			m_ui->spinBox_2->setValue(spawn.respawndelay());
			m_ui->spinBox_3->setValue(spawn.radius());
			m_ui->spinBox_4->setValue(spawn.maxcount());
			m_ui->comboBox->setCurrentIndex(limit<UInt32>(spawn.movement(), 0, 2));
			m_ui->comboBox_2->setCurrentIndex(limit<UInt32>(spawn.standstate(), 0, unit_stand_state::Count - 1));

			QTreeWidgetItem *item = new QTreeWidgetItem(m_ui->treeWidget);
			item->setText(0, QString("%1").arg(spawn.unitentry()));
			item->setText(1, "TODO");
			item->setText(2, "100%");

			QTreeWidgetItem *locItem = new QTreeWidgetItem(m_ui->treeWidget_2);
			locItem->setText(1, QString("%1, %2, %3").arg(spawn.positionx()).arg(spawn.positiony()).arg(spawn.positionz()));
			locItem->setText(2, "100%");
		}

		void SpawnDialog::on_buttonBox_accepted()
		{
			m_spawn.set_isactive(m_ui->checkBox->isChecked());
			if (!m_ui->spawnNameBox->isChecked())
			{
				m_spawn.clear_name();
			}
			else
			{
				m_spawn.set_name(m_ui->lineEdit->text().toStdString());
			}
			m_spawn.set_respawn(m_ui->checkBox_2->isChecked());
			m_spawn.set_respawndelay(m_ui->spinBox->value());
			m_spawn.set_radius(m_ui->spinBox_3->value());
			m_spawn.set_maxcount(m_ui->spinBox_4->value());
			m_spawn.set_movement(m_ui->comboBox->currentIndex());
			m_spawn.set_standstate(m_ui->comboBox_2->currentIndex());
		}
	}
}
