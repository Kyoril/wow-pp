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
				w
					<< io::write<NetUInt8>(slot)
					<< io::write<NetUInt32>(def.definition.item->id)
					<< io::write<NetUInt32>(def.count)
					<< io::write<NetUInt32>(def.definition.item->displayId)
					<< io::write<NetUInt32>(0)	// RandomSuffixIndex TODO
					<< io::write<NetUInt32>(0)	// RandomPropertyId TODO
					<< io::write<NetUInt8>(game::loot_slot_type::AllowLoot)
					;
				realCount++;
			}
			
			slot++;
		}

		// Overwrite real item count
		w.writePOD(itemCountpos, realCount);
		return w;
	}
}
