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
#include "data/loot_entry.h"
#include "data/item_entry.h"
#include "defines.h"
#include "common/utilities.h"

namespace wowpp
{
	LootInstance::LootInstance(UInt64 lootGuid)
		: m_lootGuid(lootGuid)
		, m_gold(0)
	{
	}

	LootInstance::LootInstance(UInt64 lootGuid, const LootEntry *entry, UInt32 minGold, UInt32 maxGold)
		: m_lootGuid(lootGuid)
		, m_gold(0)
	{
		if (entry)
		{
			for (auto &group : entry->lootGroups)
			{
				std::uniform_real_distribution<float> lootDistribution(0.0f, 100.0f);
				float groupRoll = lootDistribution(randomGenerator);

				auto shuffled = group;
				std::shuffle(shuffled.begin(), shuffled.end(), randomGenerator);

				bool foundNonEqualChanced = false;
				std::vector<const LootDefinition*> equalChanced;
				for (auto &def : shuffled)
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

		// Generate gold
		std::uniform_int_distribution<UInt32> goldDistribution(minGold, maxGold);
		m_gold = goldDistribution(randomGenerator);
	}

	void LootInstance::addLootItem(const LootDefinition &def)
	{
		UInt32 dropCount = def.minCount;
		if (def.maxCount > def.minCount)
		{
			std::uniform_int_distribution<UInt32> dropDistribution(def.minCount, def.maxCount);
			dropCount = dropDistribution(randomGenerator);
		}

		UInt8 itemCount = static_cast<UInt8>(m_items.size());
		m_items.insert(std::make_pair(itemCount, std::make_pair(dropCount, def)));
	}

	void LootInstance::takeGold()
	{
		m_gold = 0;
		if (isEmpty())
		{
			cleared();
		}
	}

	game::InventoryChangeFailure LootInstance::takeItem(UInt8 slot, UInt32 &out_count, LootDefinition &out_item)
	{
		auto it = m_items.find(slot);
		if (it == m_items.end())
		{
			return game::inventory_change_failure::SlotIsEmpty;
		}

		out_count = it->second.first;
		out_item = std::move(it->second.second);
		m_items.erase(it);

		if (isEmpty())
		{
			cleared();
		}

		return game::inventory_change_failure::Okay;
	}

	io::Writer & operator<<(io::Writer &w, LootInstance const& loot)
	{
		w
			<< io::write<NetUInt32>(loot.m_gold)
			<< io::write<NetUInt8>(loot.m_items.size());			// Count

		for (const auto &def : loot.m_items)
		{
			w
				<< io::write<NetUInt8>(def.first)
				<< io::write<NetUInt32>(def.second.second.item->id)
				<< io::write<NetUInt32>(def.second.first)
				<< io::write<NetUInt32>(def.second.second.item->displayId)
				<< io::write<NetUInt32>(0)
				<< io::write<NetUInt32>(0)
				<< io::write<NetUInt8>(game::loot_slot_type::AllowLoot)
				;
		}

		return w;
		// Serialized like this:
		// uint32(gold)
		// uint8(itemCount)
		// foreach(item)
		//		uint8(index++)
		//		uint32(itemId)
		//		uint32(count)
		//		uint32(displayInfoId)
		//		uint32(randomSuffixIndex)
		//		uint32(randomPropertyId)
		//		uint8(slotType)
	}
}
