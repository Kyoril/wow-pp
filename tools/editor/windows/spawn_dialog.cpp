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
#include "ogre_wrappers/ogre_dbc_file_manager.h"

namespace wowpp
{
	namespace editor
	{
		SpawnDialog::SpawnDialog(EditorApplication &app, proto::UnitSpawnEntry &spawn)
			: QDialog()
			, m_ui(new Ui::SpawnDialog)
			, m_app(app)
			, m_unitSpawn(&spawn)
			, m_objectSpawn(nullptr)
		{
			initialize();
		}

		SpawnDialog::SpawnDialog(EditorApplication &app, proto::ObjectSpawnEntry &spawn)
			: QDialog()
			, m_ui(new Ui::SpawnDialog)
			, m_app(app)
			, m_unitSpawn(nullptr)
			, m_objectSpawn(&spawn)
		{
			initialize();
		}

		void SpawnDialog::initialize()
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			if (m_unitSpawn)
			{
				// Initialize values
				m_ui->checkBox->setChecked(m_unitSpawn->isactive());
				m_ui->spawnNameBox->setChecked(m_unitSpawn->has_name());
				m_ui->lineEdit->setText(m_unitSpawn->name().c_str());
				m_ui->checkBox_2->setChecked(m_unitSpawn->respawn());
				m_ui->spinBox->setValue(m_unitSpawn->respawndelay());
				m_ui->spinBox_2->setValue(m_unitSpawn->respawndelay());
				m_ui->spinBox_3->setValue(m_unitSpawn->radius());
				m_ui->spinBox_4->setValue(m_unitSpawn->maxcount());
				m_ui->comboBox->setCurrentIndex(limit<UInt32>(m_unitSpawn->movement(), 0, 2));
				m_ui->comboBox_2->setCurrentIndex(limit<UInt32>(m_unitSpawn->standstate(), 0, unit_stand_state::Count - 1));
				m_ui->comboBox->setEnabled(true);
				m_ui->comboBox_2->setEnabled(true);

				QTreeWidgetItem *item = new QTreeWidgetItem(m_ui->treeWidget);
				item->setText(0, QString("%1").arg(m_unitSpawn->unitentry()));
				const auto *unitEntry = m_app.getProject().units.getById(m_unitSpawn->unitentry());
				if (unitEntry)
					item->setText(1, unitEntry->name().c_str());
				else
					item->setText(1, "<INVALID ENTRY>");
				item->setText(2, "100%");

				QTreeWidgetItem *locItem = new QTreeWidgetItem(m_ui->treeWidget_2);
				locItem->setText(1, QString("%1, %2, %3").arg(m_unitSpawn->positionx()).arg(m_unitSpawn->positiony()).arg(m_unitSpawn->positionz()));
				locItem->setText(2, "100%");

				try
				{
					m_emoteDbc = OgreDBCFileManager::getSingleton().load("DBFilesClient\\Emotes.dbc", "WoW");
					if (m_emoteDbc.isNull())
					{
						m_ui->emoteBox->setEnabled(false);
					}
					else
					{
						UInt32 selectedRow = 0;

						m_ui->emoteBox->clear();
						for (UInt32 i = 0; i < m_emoteDbc->getRowCount(); ++i)
						{
							UInt32 id = m_emoteDbc->getField<UInt32>(i, 0);
							if (id == m_unitSpawn->defaultemote())
								selectedRow = i;

							m_ui->emoteBox->addItem(m_emoteDbc->getField(i, 1).c_str());
						}

						m_ui->emoteBox->setCurrentIndex(selectedRow);
						m_ui->emoteBox->setEnabled(true);
					}
				}
				catch (const std::exception &e)
				{
					m_ui->emoteBox->setEnabled(false);
				}
			}
			else if (m_objectSpawn)
			{
				// Initialize values
				m_ui->checkBox->setChecked(m_objectSpawn->isactive());
				m_ui->spawnNameBox->setChecked(m_objectSpawn->has_name());
				m_ui->lineEdit->setText(m_objectSpawn->name().c_str());
				m_ui->checkBox_2->setChecked(m_objectSpawn->respawn());
				m_ui->spinBox->setValue(m_objectSpawn->respawndelay());
				m_ui->spinBox_2->setValue(m_objectSpawn->respawndelay());
				m_ui->spinBox_3->setValue(m_objectSpawn->radius());
				m_ui->spinBox_4->setValue(m_objectSpawn->maxcount());
				m_ui->comboBox->setEnabled(false);
				m_ui->comboBox_2->setEnabled(false);
				m_ui->emoteBox->setEnabled(false);

				QTreeWidgetItem *item = new QTreeWidgetItem(m_ui->treeWidget);
				item->setText(0, QString("%1").arg(m_objectSpawn->objectentry()));
				const auto *objEntry = m_app.getProject().objects.getById(m_objectSpawn->objectentry());
				if (objEntry)
					item->setText(1, objEntry->name().c_str());
				else
					item->setText(1, "<INVALID ENTRY>");
				item->setText(2, "100%");

				QTreeWidgetItem *locItem = new QTreeWidgetItem(m_ui->treeWidget_2);
				locItem->setText(1, QString("%1, %2, %3").arg(m_objectSpawn->positionx()).arg(m_objectSpawn->positiony()).arg(m_objectSpawn->positionz()));
				locItem->setText(2, "100%");
			}
		}

		void SpawnDialog::on_buttonBox_accepted()
		{
			if (m_unitSpawn)
			{
				m_unitSpawn->set_isactive(m_ui->checkBox->isChecked());
				if (!m_ui->spawnNameBox->isChecked())
				{
					m_unitSpawn->clear_name();
				}
				else
				{
					m_unitSpawn->set_name(m_ui->lineEdit->text().toStdString());
				}
				m_unitSpawn->set_respawn(m_ui->checkBox_2->isChecked());
				m_unitSpawn->set_respawndelay(m_ui->spinBox->value());
				m_unitSpawn->set_radius(m_ui->spinBox_3->value());
				m_unitSpawn->set_maxcount(m_ui->spinBox_4->value());
				m_unitSpawn->set_movement(m_ui->comboBox->currentIndex());
				m_unitSpawn->set_standstate(m_ui->comboBox_2->currentIndex());

				if (!m_emoteDbc.isNull())
				{
					UInt32 selectedRow = 0;

					UInt32 index = m_ui->emoteBox->currentIndex();
					if (index < m_emoteDbc->getRowCount())
					{
						UInt32 id = m_emoteDbc->getField<UInt32>(index, 0);
						m_unitSpawn->set_defaultemote(id);
					}
				}
			}
			else if (m_objectSpawn)
			{
				m_objectSpawn->set_isactive(m_ui->checkBox->isChecked());
				if (!m_ui->spawnNameBox->isChecked())
				{
					m_objectSpawn->clear_name();
				}
				else
				{
					m_objectSpawn->set_name(m_ui->lineEdit->text().toStdString());
				}
				m_objectSpawn->set_respawn(m_ui->checkBox_2->isChecked());
				m_objectSpawn->set_respawndelay(m_ui->spinBox->value());
				m_objectSpawn->set_radius(m_ui->spinBox_3->value());
				m_objectSpawn->set_maxcount(m_ui->spinBox_4->value());
			}
		}
	}
}
