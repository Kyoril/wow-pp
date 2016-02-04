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

#include "loot_instance.h"
#include "proto_data/project.h"
#include "defines.h"
#include "game_character.h"
#include "common/utilities.h"

namespace wowpp
{
	LootInstance::LootInstance(proto::ItemManager &items, UInt64 lootGuid)
		: m_itemManager(items)
		, m_lootGuid(lootGuid)
		, m_gold(0)
	{
	}

	LootInstance::LootInstance(proto::ItemManager &items, UInt64 lootGuid, const proto::LootEntry *entry, UInt32 minGold, UInt32 maxGold, const std::vector<GameCharacter*> &lootRecipients)
		: m_itemManager(items)
		, m_lootGuid(lootGuid)
		, m_gold(0)
	{
		if (entry)
		{
			for (int i = 0; i < entry->groups_size(); ++i)
			{
				const auto &group = entry->groups(i);

				std::uniform_real_distribution<float> lootDistribution(0.0f, 100.0f);
				float groupRoll = lootDistribution(randomGenerator);

				//auto shuffled = group;
				//TODO std::shuffle(shuffled.definitions().begin(), shuffled.definitions().end(), randomGenerator);

				bool foundNonEqualChanced = false;
				std::vector<const proto::LootDefinition*> equalChanced;
				for (int i = 0; i < group.definitions_size(); ++i)
				{
					const auto &def = group.definitions(i);

					// Is quest item?
					if (def.conditiontype() == 9)
					{
						UInt32 questItemCount = 0;
						UInt32 questId = def.conditionvala();
						for (const auto *recipient : lootRecipients)
						{
							if (recipient &&
								recipient->getQuestStatus(questId) == game::quest_status::Incomplete)
							{
								questItemCount++;
								break;		// Later...
							}
						}

						// Skip this quest item
						if (questItemCount == 0)
							continue;
					}

					if (def.dropchance() == 0.0f)
					{
						equalChanced.push_back(&def);
						continue;
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

		// Generate gold
		std::uniform_int_distribution<UInt32> goldDistribution(minGold, maxGold);
		m_gold = goldDistribution(randomGenerator);
	}

	void LootInstance::addLootItem(const proto::LootDefinition &def)
	{
		const auto *lootItem = m_itemManager.getById(def.item());
		if (!lootItem)
			return;

		UInt32 dropCount = def.mincount();
		if (def.maxcount() > def.mincount())
		{
			std::uniform_int_distribution<UInt32> dropDistribution(def.mincount(), def.maxcount());
			dropCount = dropDistribution(randomGenerator);
		}

		if (dropCount > lootItem->maxstack())
		{
			WLOG("Item drop count was " << dropCount << " but max item stack count is " << lootItem->maxstack());
			dropCount = lootItem->maxstack();
		}

		// Always at least 1 item
		if (dropCount == 0) dropCount = 1;
		m_items.emplace_back(LootItem(dropCount, def));
	}

	void LootInstance::takeGold()
	{
		m_gold = 0;
		if (isEmpty())
		{
			cleared();
		}
	}

	const LootItem * LootInstance::getLootDefinition(UInt8 slot) const
	{
		if (slot >= m_items.size()) return nullptr;
		return &m_items[slot];
	}

	bool LootInstance::isEmpty() const
	{
		const bool noGold = (m_gold == 0);
		if (m_items.empty())
		{
			return noGold;
		}
		else if (noGold)
		{
			for (auto &item : m_items)
			{
				if (!item.isLooted)
					return false;
			}

			return true;
		}

		return false;
	}

	void LootInstance::takeItem(UInt8 slot)
	{
		if (slot >= m_items.size()) 
			return;

		if (m_items[slot].isLooted)
		{
			return;
		}

		m_items[slot].isLooted = true;
		if (isEmpty())
		{
			cleared();
		}
	}

	io::Writer & operator<<(io::Writer &w, LootInstance const& loot)
	{
		// Write gold
		w
			<< io::write<NetUInt32>(loot.m_gold);

		// Write placeholder item count (real value will be overwritten later)
		const size_t itemCountpos = w.sink().position();
		w
			<< io::write<NetUInt8>(loot.m_items.size());

		// Iterate through all loot items...
		UInt8 realCount = 0;
		UInt8 slot = 0;
		for (const auto &def : loot.m_items)
		{
			// Only write item entry if the item hasn't been looted yet
			if (!def.isLooted)
			{
				const auto *itemEntry = loot.m_itemManager.getById(def.definition.item());
				if (itemEntry)
				{
					w
						<< io::write<NetUInt8>(slot)
						<< io::write<NetUInt32>(def.definition.item())
						<< io::write<NetUInt32>(def.count)
						<< io::write<NetUInt32>(itemEntry->displayid())
						<< io::write<NetUInt32>(0)	// RandomSuffixIndex TODO
						<< io::write<NetUInt32>(0)	// RandomPropertyId TODO
						<< io::write<NetUInt8>(game::loot_slot_type::AllowLoot)
						;
					realCount++;
				}
			}
			
			slot++;
		}

		// Overwrite real item count
		w.writePOD(itemCountpos, realCount);
		return w;
	}
}
