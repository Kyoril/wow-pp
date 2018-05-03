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

	LootInstance::LootInstance(proto::ItemManager &items, UInt64 lootGuid, const proto::LootEntry *entry, UInt32 minGold, UInt32 maxGold, const std::vector<GameCharacter *> &lootRecipients)
		: m_itemManager(items)
		, m_lootGuid(lootGuid)
		, m_gold(0)
	{
		m_recipients.reserve(lootRecipients.size());
		for (const auto *character : lootRecipients)
			m_recipients.push_back(character->getGuid());

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
				std::vector<const proto::LootDefinition *> equalChanced;
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
							        recipient->needsQuestItem(def.item()))
							{
								if (recipient->getQuestStatus(questId) == game::quest_status::Incomplete)
								{
									questItemCount++;
									break;		// Later...
								}
							}
						}

						// Skip this quest item
						if (questItemCount == 0) 
						{
							continue;
						}
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

	void LootInstance::serialize(io::Writer & writer, UInt64 receiver) const
	{
		// Write gold
		writer
			<< io::write<NetUInt32>(m_gold);

		// Write placeholder item count (real value will be overwritten later)
		const size_t itemCountpos = writer.sink().position();
		writer
			<< io::write<NetUInt8>(m_items.size());

		auto dataIt = m_playerLootData.find(receiver);
		
		// Iterate through all loot items...
		UInt8 realCount = 0;
		UInt8 slot = 0;
		for (const auto &def : m_items)
		{
			const auto *itemEntry = m_itemManager.getById(def.definition.item());
			if (itemEntry)
			{
				bool isLooted = def.isLooted;
				if (!isLooted &&					// Not yet looted
					(itemEntry->flags() & game::item_flags::PartyLoot) && // Shared item
					dataIt != m_playerLootData.end())		// Player loot data available
				{
					auto counterIt = dataIt->second.find(itemEntry->id());
					if (counterIt != dataIt->second.end())
					{
						isLooted = (counterIt->second >= def.count);
					}
				}

				// Only write item entry if the item hasn't been looted yet
				if (!isLooted)
				{
					writer
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
		writer.writePOD(itemCountpos, realCount);
	}

	void LootInstance::addLootItem(const proto::LootDefinition &def)
	{
		const auto *lootItem = m_itemManager.getById(def.item());
		if (!lootItem) {
			return;
		}

		UInt32 dropCount = def.mincount();
		if (def.maxcount() > def.mincount())
		{
			std::uniform_int_distribution<UInt32> dropDistribution(def.mincount(), def.maxcount());
			dropCount = dropDistribution(randomGenerator);
		}

		if (dropCount > lootItem->maxstack())
		{
			WLOG("Loot entry: Item's " << def.item() << " drop count was " << dropCount << " but max item stack count is " << lootItem->maxstack());
			dropCount = lootItem->maxstack();
		}

		// Always at least 1 item
		if (dropCount == 0) {
			dropCount = 1;
		}

		m_items.emplace_back(LootItem(dropCount, def));
	}

	void LootInstance::takeGold()
	{
		if (!hasGold())
			return;

		// Remove gold
		m_gold = 0;

		// Notify all looting players
		goldRemoved();

		// Notify loot source object
		if (isEmpty())
		{
			cleared();
		}
	}

	const LootItem *LootInstance::getLootDefinition(UInt8 slot) const
	{
		if (slot >= m_items.size()) {
			return nullptr;
		}
		return &m_items[slot];
	}

	bool LootInstance::isEmpty() const
	{
		// Determine if there is gold to loot
		if (hasGold())
			return false;

		// There is no gold, but there are items available that we need to check one by one
		for (const auto &item : m_items)
		{
			// If the item hasn't been looted yet...
			if (!item.isLooted) 
			{
				// Check if this is a party-shared loot item
				const auto *entry = m_itemManager.getById(item.definition.item());
				if (!entry)
					continue;

				if (entry->flags() & game::item_flags::PartyLoot)
				{
					// It is shared by the party, so we need to determine if at least one loot receiver 
					// can still loot this item
					for (const auto &guid : m_recipients)
					{
						auto dataIt = m_playerLootData.find(guid);
						if (dataIt == m_playerLootData.end())
						{
							// No shared loots for this player yet
							return false;
						}

						auto counterIt = dataIt->second.find(entry->id());
						if (counterIt == dataIt->second.end())
						{
							// No loots of this item by this player yet
							return false;
						}

						if (counterIt->second < item.count)
						{
							// Item not looted often enough by this player yet
							return false;
						}
					}
				}
				else
				{
					// This item is not shared, so it is available and we can stop here
					return false;
				}
			}
		}

		// No lootable items found - loot is empty
		return true;
	}

	bool LootInstance::containsLootFor(UInt64 receiver)
	{
		// Gold available?
		if (hasGold())
			return true;

		// Now check all items...
		for (auto &item : m_items)
		{
			// Item not looted?
			if (!item.isLooted)
			{
				const auto *entry = m_itemManager.getById(item.definition.item());
				if (!entry)
					continue;

				// Check if item is a shared item...
				if (entry->flags() & game::item_flags::PartyLoot)
				{
					auto it = m_playerLootData.find(receiver);
					if (it == m_playerLootData.end())
					{
						// Not looted yet, so loot is available
						return true;
					}

					auto it2 = it->second.find(entry->id());
					if (it2 == it->second.end())
					{
						// This item hasn't been looted yet, so loot is available
						return true;
					}

					if (it2->second < item.count)
					{
						// Not enough items looted - so loot is available
						return true;
					}
				}
				else
				{
					// Item isn't looted and is not sharable - so loot is available
					return true;
				}
			}
		}

		// No lootable item is available for this player
		return false;
	}

	void LootInstance::takeItem(UInt8 slot, UInt64 receiver)
	{
		// Check if slot is valid
		if (slot >= m_items.size()) {
			return;
		}

		// Check if item was already looted
		if (m_items[slot].isLooted)
			return;

		// Request item entry for more data
		const auto *entry = m_itemManager.getById(m_items[slot].definition.item());
		if (!entry)
			return;

		// If this item is a party-shared item...
		if (entry->flags() & game::item_flags::PartyLoot)
		{
			// We will simply increment the counter
			auto &data = m_playerLootData[receiver];
			data[entry->id()] = m_items[slot].count;
		}
		else
		{
			// This item is not shared, so it is looted now
			m_items[slot].isLooted = true;
		}

		// Notify all watching players
		itemRemoved(slot);

		// If everything has been looted, we will call the signal. This will most likely
		// update the corpse / game object, that owns this loot instance
		if (isEmpty())
		{
			cleared();
		}
	}
}
