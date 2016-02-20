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
#include "loot_dialog.h"
#include "ui_loot_dialog.h"
#include "proto_data/project.h"
#include "common/utilities.h"
#include <QListWidgetItem>

namespace wowpp
{
	namespace editor
	{
		LootDialog::LootDialog(proto::Project &project, const proto::LootEntry &loot)
			: QDialog()
			, m_ui(new Ui::LootDialog)
			, m_project(project)
			, m_loot(loot)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);
			rerollLoot();
		}

		void LootDialog::addLootItem(const proto::LootDefinition &def)
		{
			UInt32 dropCount = def.mincount();
			if (def.maxcount() > def.mincount())
			{
				std::uniform_int_distribution<UInt32> dropDistribution(def.mincount(), def.maxcount());
				dropCount = dropDistribution(randomGenerator);
			}

			const auto *item = m_project.items.getById(def.item());
			if (!item)
				return;

			QListWidgetItem *widget = new QListWidgetItem(QString("%1x %2").arg(dropCount).arg(item->name().c_str()), m_ui->listWidget);
			QColor textColor = QColor(Qt::white);
			switch (item->quality())
			{
			case 0:
				textColor = QColor(Qt::gray);
				break;
			case 1:
				textColor = QColor(Qt::white);
				break;
			case 2:
				textColor = QColor(Qt::green);
				break;
			case 3:
				textColor = QColor(0, 114, 198);
				break;
			case 4:
				textColor = QColor(Qt::magenta);
				break;
			case 5:
				textColor = QColor(Qt::yellow);
				break;
			default:
				textColor = QColor(Qt::red);
				break;
			}
			widget->setTextColor(textColor);
		}

		void LootDialog::rerollLoot()
		{
			// Remove all items
			m_ui->listWidget->clear();

			// Roll for all groups
			for (const auto &group : m_loot.groups())
			{
				std::uniform_real_distribution<float> lootDistribution(0.0f, 100.0f);
				float groupRoll = lootDistribution(randomGenerator);

				// We need to copy our loot groups here, because otherwise, we invalidate the 
				// save state of our editor project here. On the world node however, we don't
				// have to do this!
				/*auto shuffled = group;
				std::shuffle(shuffled.begin(), shuffled.end(), randomGenerator);*/

				bool foundNonEqualChanced = false;
				std::vector<const proto::LootDefinition*> equalChanced;
				for (const auto &def : group.definitions())
				{
					if (def.dropchance() == 0.0f)
					{
						equalChanced.push_back(&def);
					}

					if (def.dropchance() > 0.0f &&
						def.dropchance() >= groupRoll)
					{
						addLootItem(def);
						foundNonEqualChanced = true;
						break;
					}

					groupRoll -= def.dropchance();
				}

				if (!foundNonEqualChanced &&
					!equalChanced.empty())
				{
					std::uniform_int_distribution<UInt32> equalDistribution(0, equalChanced.size() - 1);
					UInt32 index = equalDistribution(randomGenerator);
					addLootItem(*equalChanced[index]);
				}
			}
		}

		void LootDialog::rerollStats()
		{
			m_ui->listWidget_2->clear();
			std::unordered_map<const proto::ItemEntry*, UInt32> drops;

			int killCount = limit(m_ui->killCountBox->value(), 1, 10000);
			for (int i = 0; i < killCount; ++i)
			{
				// Roll for all groups
				for (const auto &group : m_loot.groups())
				{
					std::uniform_real_distribution<float> lootDistribution(0.0f, 100.0f);
					float groupRoll = lootDistribution(randomGenerator);

					// We need to copy our loot groups here, because otherwise, we invalidate the 
					// save state of our editor project here. On the world node however, we don't
					// have to do this!
					/*auto shuffled = group;
					std::shuffle(shuffled.begin(), shuffled.end(), randomGenerator);*/

					bool foundNonEqualChanced = false;
					std::vector<const proto::LootDefinition*> equalChanced;
					for (const auto &def : group.definitions())
					{
						auto *itemEntry = m_project.items.getById(def.item());
						if (!itemEntry)
							continue;

						if (def.dropchance() == 0.0f)
						{
							equalChanced.push_back(&def);
						}

						if (def.dropchance() > 0.0f &&
							def.dropchance() >= groupRoll)
						{
							UInt32 dropCount = def.mincount();
							if (def.maxcount() > def.mincount())
							{
								std::uniform_int_distribution<UInt32> dropDistribution(def.mincount(), def.maxcount());
								dropCount = dropDistribution(randomGenerator);
							}

							UInt32 &count = drops[itemEntry];
							count += dropCount;

							foundNonEqualChanced = true;
							break;
						}

						groupRoll -= def.dropchance();
					}

					if (!foundNonEqualChanced &&
						!equalChanced.empty())
					{
						std::uniform_int_distribution<UInt32> equalDistribution(0, equalChanced.size() - 1);
						UInt32 index = equalDistribution(randomGenerator);

						const auto *def = equalChanced[index];
						auto *itemEntry = m_project.items.getById(def->item());
						if (!itemEntry)
							continue;

						UInt32 dropCount = def->mincount();
						if (def->maxcount() > def->mincount())
						{
							std::uniform_int_distribution<UInt32> dropDistribution(def->mincount(), def->maxcount());
							dropCount = dropDistribution(randomGenerator);
						}

						UInt32 &count = drops[itemEntry];
						count += dropCount;
					}
				}
			}

			for (const auto &lootPair : drops)
			{
				QListWidgetItem *item = new QListWidgetItem(QString("%1x %2").arg(lootPair.second).arg(lootPair.first->name().c_str()), m_ui->listWidget_2);
				QColor textColor = QColor(Qt::white);
				switch (lootPair.first->quality())
				{
				case 0:
					textColor = QColor(Qt::gray);
					break;
				case 1:
					textColor = QColor(Qt::white);
					break;
				case 2:
					textColor = QColor(Qt::green);
					break;
				case 3:
					textColor = QColor(0, 114, 198);
					break;
				case 4:
					textColor = QColor(Qt::magenta);
					break;
				case 5:
					textColor = QColor(Qt::yellow);
					break;
				default:
					textColor = QColor(Qt::red);
					break;
				}
				item->setTextColor(textColor);
			}
		}

		void LootDialog::on_rerollButton_clicked()
		{
			rerollLoot();
		}

		void LootDialog::on_rebuildStatisticBtn_clicked()
		{
			rerollStats();
		}

	}
}
