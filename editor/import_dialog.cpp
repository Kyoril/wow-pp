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

#include "import_dialog.h"
#include "ui_import_dialog.h"
#include "editor_application.h"
#include "mysql_wrapper/mysql_connection.h"
#include "mysql_wrapper/mysql_row.h"
#include "mysql_wrapper/mysql_select.h"
#include "mysql_wrapper/mysql_statement.h"
#include <qtconcurrentrun.h>
#include <QMessageBox>

namespace wowpp
{
	namespace editor
	{
		ImportDialog::ImportDialog(EditorApplication &app)
			: QDialog()
			, m_ui(new Ui::ImportDialog)
			, m_app(app)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);

			connect(this, SIGNAL(progressRangeChanged(int,int)), this, SLOT(on_progressRangeChanged(int, int)));
			connect(this, SIGNAL(progressTextChanged(QString)), this, SLOT(on_progressTextChanged(QString)));
			connect(this, SIGNAL(progressValueChanged(int)), this, SLOT(on_progressValueChanged(int)));
			connect(this, SIGNAL(calculationFinished()), this, SLOT(on_calculationFinished()));

			// Run connection thread
			const QFuture<void> future = QtConcurrent::run(this, &ImportDialog::importData);
			m_watcher.setFuture(future);
		}

		void ImportDialog::on_progressRangeChanged(int minimum, int maximum)
		{
			m_ui->progressBar->setMinimum(minimum);
			m_ui->progressBar->setMaximum(maximum);
		}

		void ImportDialog::on_progressValueChanged(int value)
		{
			m_ui->progressBar->setValue(value);
		}

		void ImportDialog::on_progressTextChanged(QString text)
		{
			m_ui->label->setText(text);
		}

		void ImportDialog::on_calculationFinished()
		{
			m_app.markAsChanged();
		}

		void ImportDialog::importData()
		{
			// Change progress range
			emit progressRangeChanged(0, 0);
			emit progressValueChanged(0);
			emit progressTextChanged("Connecting...");

			auto &config = m_app.getConfiguration();

			// Database connection
			MySQL::DatabaseInfo connectionInfo(config.mysqlHost, config.mysqlPort, config.mysqlUser, config.mysqlPassword, config.mysqlDatabase);
			MySQL::Connection connection;
			if (!connection.connect(connectionInfo))
			{
				emit progressRangeChanged(0, 100);
				emit progressTextChanged(QString("Could not connect: %1").arg(connection.getErrorMessage()));
				return;
			}

			UInt32 currentEntry = 0;
			UInt32 lootCount = 0;

			// Not collect data
			emit progressRangeChanged(0, 100);
			emit progressTextChanged("Collecting data...");
			{
				wowpp::MySQL::Select select(connection, "(SELECT COUNT(*) FROM `wowpp_creature_loot_template` WHERE `lootcondition` = 0 AND `active` != 0);");
				if (select.success())
				{
					wowpp::MySQL::Row row(select);
					if (row)
					{
						row.getField(0, lootCount);
						row = row.next(select);
					}
				}
				else
				{
					emit progressTextChanged(QString("Error: %1").arg(connection.getErrorMessage()));
					return;
				}
			}
			
			// Remove old unit loot
			for (int i = 0; i < m_app.getProject().units.getTemplates().entry_size(); ++i)
			{
				auto *unit = m_app.getProject().units.getTemplates().mutable_entry(i);
				unit->set_unitlootentry(0);
			}
			m_app.getProject().unitLoot.clear();

			UInt32 lastEntry = 0;
			UInt32 lastGroup = 0;
			UInt32 groupIndex = 0;

			// Import items...
			emit progressTextChanged(QString("Importing %1 entries...").arg(lootCount));
			{
				wowpp::MySQL::Select select(connection, "(SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount` FROM `wowpp_creature_loot_template` WHERE `lootcondition` = 0  AND `active` != 0) "
					"ORDER BY `entry`, `groupid`;");
				if (select.success())
				{
					wowpp::MySQL::Row row(select);
					while (row)
					{
						emit progressValueChanged(static_cast<int>(static_cast<double>(currentEntry) / static_cast<double>(lootCount) * 100.0));
						currentEntry++;

						UInt32 entry = 0, itemId = 0, groupId = 0, minCount = 0, maxCount = 0;
						float dropChance = 0.0f;
						row.getField(0, entry);
						row.getField(1, itemId);
						row.getField(2, dropChance);
						row.getField(3, groupId);
						row.getField(4, minCount);
						row.getField(5, maxCount);

						// Find referenced item
						const auto *itemEntry = m_app.getProject().items.getById(itemId);
						if (!itemEntry)
						{
							ELOG("Could not find referenced item " << itemId << " (referenced in creature loot entry " << entry << " - group " << groupId << ")");
							row = row.next(select);
							continue;
						}

						// Create a new loot entry
						bool created = false;
						if (entry > lastEntry)
						{
							auto *added = m_app.getProject().unitLoot.add(entry);

							lastEntry = entry;
							lastGroup = groupId;
							groupIndex = 0;
							created = true;
						}

						auto *lootEntry = m_app.getProject().unitLoot.getById(entry);
						if (!lootEntry)
						{
							// Error
							ELOG("Loot entry " << entry << " found, but no creature to assign found");
							row = row.next(select);
							continue;
						}

						if (created)
						{
							auto *unitEntry = m_app.getProject().units.getById(entry);
							if (!unitEntry)
							{
								WLOG("No unit with entry " << entry << " found - creature loot template will not be assigned!");
							}
							else
							{
								unitEntry->set_unitlootentry(lootEntry->id());
							}
						}

						// If there are no loot groups yet, create a new one
						if (lootEntry->groups().empty() || groupId > lastGroup)
						{
							auto *addedGroup = lootEntry->add_groups();
							if (groupId > lastGroup)
							{
								lastGroup = groupId;
								groupIndex++;
							}
						}

						if (lootEntry->groups().empty())
						{
							ELOG("Error retrieving loot group");
							row = row.next(select);
							continue;
						}

						auto *group = lootEntry->mutable_groups(groupIndex);
						auto *def = group->add_definitions();
						def->set_item(itemEntry->id());
						def->set_mincount(minCount);
						def->set_maxcount(maxCount);
						def->set_dropchance(dropChance);
						def->set_isactive(true);

						row = row.next(select);
					}
				}
			}

			emit progressValueChanged(100);
			emit progressTextChanged("Finished");

			emit calculationFinished();
		}
	}
}
