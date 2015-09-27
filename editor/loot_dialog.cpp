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

#include "loot_dialog.h"
#include "ui_loot_dialog.h"
#include "data/project.h"
#include "common/utilities.h"
#include <QListWidgetItem>

namespace wowpp
{
	namespace editor
	{
		LootDialog::LootDialog(Project &project, const LootEntry &loot)
			: QDialog()
			, m_ui(new Ui::LootDialog)
			, m_project(project)
			, m_loot(loot)
		{
			// Setup auto generated ui
			m_ui->setupUi(this);
			rerollLoot();
		}

		void LootDialog::on_rerollButton_clicked()
		{
			rerollLoot();
		}

		void LootDialog::rerollLoot()
		{
			// Remove all items
			m_ui->listWidget->clear();

			// Roll for all groups
			for (auto &group : m_loot.lootGroups)
			{
				std::uniform_real_distribution<float> lootDistribution(0.0f, 100.0f);
				float groupRoll = lootDistribution(randomGenerator);

				bool foundNonEqualChanced = false;
				std::vector<const LootDefinition*> equalChanced;
				for (auto &def : group)
				{
					if (def.dropChance == 0.0f)
					{
						equalChanced.push_back(&def);
					}

					if (def.dropChance > 0.0f &&
						def.dropChance >= groupRoll)
					{
						addLootItem(def);
						foundNonEqualChanced = true;
						break;
					}

					groupRoll -= def.dropChance;
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

		void LootDialog::addLootItem(const LootDefinition &def)
		{
			UInt32 dropCount = def.minCount;
			if (def.maxCount > def.minCount)
			{
				std::uniform_int_distribution<UInt32> dropDistribution(def.minCount, def.maxCount);
				dropCount = dropDistribution(randomGenerator);
			}

			QListWidgetItem *item = new QListWidgetItem(QString("%1x %2").arg(dropCount).arg(def.item->name.c_str()), m_ui->listWidget);
			QColor textColor = QColor(Qt::white);
			switch (def.item->quality)
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
}
