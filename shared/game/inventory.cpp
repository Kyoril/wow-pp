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

#include "inventory.h"
#include "game_character.h"
#include "game_item.h"
#include "proto_data/project.h"
#include "common/linear_set.h"
#include "binary_io/vector_sink.h"

namespace wowpp
{
	Inventory::Inventory(GameCharacter & owner)
		: m_owner(owner)
		, m_freeSlots(player_inventory_pack_slots::End - player_inventory_pack_slots::Start)	// Default slot count with only a backpack
	{
	}
	game::InventoryChangeFailure Inventory::createItems(const proto::ItemEntry & entry, UInt16 amount/* = 1*/)
	{
		// Incorrect value used, so give at least one item
		if (amount == 0) amount = 1;

		// Limit the total amount of items
		const UInt16 itemCount = getItemCount(entry.id());
		if (entry.maxcount() > 0 &&
			itemCount + amount >= entry.maxcount())
		{
			return game::inventory_change_failure::CantCarryMoreOfThis;
		}

		// Quick check if there are enough free slots (only works if we don't have an item of this type yet)
		const UInt16 requiredSlots = (amount - 1) / entry.maxstack() + 1;
		if (itemCount == 0 &&
			requiredSlots > m_freeSlots)
		{
			return game::inventory_change_failure::InventoryFull;
		}

		// We need to remember free slots, since we first want to stack items up as best as possible
		LinearSet<UInt16> emptySlots;
		// We also need to remember all valid slots, that contain an item of that entry but are not 
		// at the stack limit so we can fill up those stacks.
		LinearSet<UInt16> usedCapableSlots;
		// This counter represent the number of available space for this item in total
		UInt16 availableStacks = 0;

		// This variable is used so that we can take a shortcut. Since we know the total amount of
		// this item entry in the inventory, we can determine whether we have found all items
		UInt16 itemsProcessed = 0;

		// Now do the iteration. First check the main bag
		// TODO: Check bags, too
		for (UInt8 slot = player_inventory_pack_slots::Start; slot < player_inventory_pack_slots::End; ++slot)
		{
			const UInt16 absoluteSlot = getAbsoluteSlot(player_inventory_slots::Bag_0, slot);
			
			// Check if this slot is empty
			auto it = m_itemsBySlot.find(absoluteSlot);
			if (it == m_itemsBySlot.end())
			{
				// Increase counter
				availableStacks += entry.maxstack();

				// Remember this slot for later and skip it for now
				emptySlots.add(absoluteSlot);
				if (itemsProcessed >= itemCount &&
					emptySlots.size() >= requiredSlots)
				{
					break;
				}

				// If we processed all items, we want to make sure, that we found enough free slots as well
				continue;
			}

			// It is not empty, so check if the item is of the same entry
			if (it->second->getEntry().id() != entry.id())
			{
				// Different item
				continue;
			}

			// Get the items stack count
			const UInt32 stackCount = it->second->getStackCount();
			itemsProcessed += stackCount;
			
			// Check if the item's stack limit is reached
			if (stackCount >= entry.maxstack())
			{
				if (itemsProcessed >= itemCount &&
					emptySlots.size() >= requiredSlots)
				{
					break;
				}

				// If we processed all items, we want to make sure, that we found enough free slots as well
				continue;
			}

			// Stack limit not reached, remember this slot
			availableStacks += (entry.maxstack() - stackCount);
			usedCapableSlots.add(absoluteSlot);
		}

		// Now we can determine if there is enough space
		if (amount > availableStacks)
		{
			// Not enough space
			return game::inventory_change_failure::InventoryFull;
		}

		// Now finally create the items. First, fill up all used stacks
		UInt16 amountLeft = amount;
		for (auto &slot : usedCapableSlots)
		{
			auto item = m_itemsBySlot[slot];

			// Added can not be greater than amountLeft, so we don't need a check on subtraction
			UInt16 added = item->addStacks(amountLeft);
			amountLeft -= added;

			// Notify update
			if (added > 0)
			{
				// Increase cached counter
				m_itemCounter[entry.id()] += added;

				// Notify update
				itemInstanceUpdated(item, slot);
			}

			// Everything added
			if (amountLeft == 0)
				break;
		}

		// Are there still items left?
		if (amountLeft)
		{
			// Now iterate through all empty slots
			for (auto &slot : emptySlots)
			{
				// Create a new item instance
				auto item = std::make_shared<GameItem>(m_owner.getProject(), entry);
				item->initialize();

				// One stack has been created by initializing the item
				amountLeft--;

				// Modify stack count
				UInt16 added = item->addStacks(amountLeft);
				amountLeft -= added;

				// Increase cached counter
				if (added > 0)
				{
					m_itemCounter[entry.id()] += added;
				}

				// Add this item to the inventory slot and reduce our free slot cache
				m_itemsBySlot[slot] = std::move(item);
				m_freeSlots--;

				// Notify creation
				itemInstanceCreated(m_itemsBySlot[slot], slot);

				// All done
				if (amountLeft == 0)
					break;
			}
		}

		// WARNING: There should never be any items left here!
		assert(amountLeft == 0);
		if (amountLeft > 0)
		{
			ELOG("Could not add all items, something went really wrong! " << __FUNCTION__);
			return game::inventory_change_failure::InventoryFull;
		}

		// Everything okay
		return game::inventory_change_failure::Okay;
	}
	game::InventoryChangeFailure Inventory::removeItems(const proto::ItemEntry & entry, UInt16 amount)
	{
		// If amount equals 0, remove ALL items of that entry.
		const UInt16 itemCount = getItemCount(entry.id());
		if (amount == 0) amount = itemCount;

		// We don't have enough items, so we don't need to bother iterating through
		if (itemCount < amount)
		{
			// Maybe use a different result
			return game::inventory_change_failure::ItemNotFound;
		}

		// Counter used to know when to stop iteration
		UInt16 itemsToDelete = amount;

		// Now do the iteration. First check the main bag
		// TODO: Check bags, too
		for (UInt8 slot = player_inventory_pack_slots::Start; slot < player_inventory_pack_slots::End; ++slot)
		{
			const UInt16 absoluteSlot = getAbsoluteSlot(player_inventory_slots::Bag_0, slot);

			// Check if this slot is empty
			auto it = m_itemsBySlot.find(absoluteSlot);
			if (it == m_itemsBySlot.end())
			{
				// Empty slot
				continue;
			}

			// It is not empty, so check if the item is of the same entry
			if (it->second->getEntry().id() != entry.id())
			{
				// Different item
				continue;
			}

			// Get the items stack count
			const UInt32 stackCount = it->second->getStackCount();
			if (stackCount <= itemsToDelete)
			{
				// Remove item at this slot
				auto result = removeItem(absoluteSlot);
				if (result != game::inventory_change_failure::Okay)
				{
					ELOG("Could not remove item at slot " << absoluteSlot);
				}
				else
				{
					// Reduce counter
					itemsToDelete -= stackCount;
				}
			}
			else
			{
				// Reduce stack count
				it->second->setUInt32Value(item_fields::StackCount, stackCount - itemsToDelete);
				m_itemCounter[entry.id()] -= (stackCount - itemsToDelete);
				itemsToDelete = 0;

				// Notify client about this update
				itemInstanceUpdated(it->second, slot);
			}

			// All items processed, we can stop here
			if (itemsToDelete == 0)
				break;
		}

		// WARNING: There should never be any items left here!
		assert(itemsToDelete == 0);
		if (itemsToDelete > 0)
		{
			ELOG("Could not remove all items, something went really wrong! " << __FUNCTION__);
		}

		return game::inventory_change_failure::Okay;
	}
	game::InventoryChangeFailure Inventory::removeItem(UInt16 absoluteSlot, UInt16 stacks/* = 0*/)
	{
		// Try to find item
		auto it = m_itemsBySlot.find(absoluteSlot);
		if (it == m_itemsBySlot.end())
		{
			return game::inventory_change_failure::ItemNotFound;
		}

		// Updated cached item counter
		const UInt32 stackCount = it->second->getStackCount();
		if (stacks == 0 || stacks > stackCount) stacks = stackCount;
		m_itemCounter[it->second->getEntry().id()] -= stacks;

		if (stackCount == stacks)
		{
			// Remove item from slot
			auto item = it->second;
			m_itemsBySlot.erase(it);
			m_freeSlots++;

			// Notify about destruction
			itemInstanceDestroyed(item, absoluteSlot);
		}
		else
		{
			it->second->setUInt32Value(item_fields::StackCount, stackCount - stacks);
			itemInstanceUpdated(it->second, absoluteSlot);
		}

		return game::inventory_change_failure::Okay;
	}
	game::InventoryChangeFailure Inventory::swapItems(UInt16 slotA, UInt16 slotB)
	{
		// TODO
		return game::inventory_change_failure::Okay;
	}
	UInt16 Inventory::getItemCount(UInt32 itemId) const
	{
		auto it = m_itemCounter.find(itemId);
		return (it != m_itemCounter.end() ? it->second : 0);
	}
	UInt16 Inventory::getAbsoluteSlot(UInt8 bag, UInt8 slot) const
	{
		return (bag << 8) | slot;
	}
	bool Inventory::getRelativeSlots(UInt16 absoluteSlot, UInt8 & out_bag, UInt8 & out_slot) const
	{
		out_bag = static_cast<UInt8>(absoluteSlot >> 8);
		out_slot = static_cast<UInt8>(absoluteSlot & 0xFF);
		return true;
	}
	void Inventory::addSpawnBlocks(std::vector<std::vector<char>>& out_blocks)
	{
		for (auto &pair : m_itemsBySlot)
		{
			std::vector<char> createItemBlock;
			io::VectorSink createItemSink(createItemBlock);
			io::Writer createItemWriter(createItemSink);
			{
				UInt8 updateType = 0x02;						// Item
				UInt8 updateFlags = 0x08 | 0x10;				// 
				UInt8 objectTypeId = 0x01;						// Item
				UInt64 guid = pair.second->getGuid();

				// Header with object guid and type
				createItemWriter
					<< io::write<NetUInt8>(updateType);
				UInt64 guidCopy = guid;
				UInt8 packGUID[8 + 1];
				packGUID[0] = 0;
				size_t size = 1;
				for (UInt8 i = 0; guidCopy != 0; ++i)
				{
					if (guidCopy & 0xFF)
					{
						packGUID[0] |= UInt8(1 << i);
						packGUID[size] = UInt8(guidCopy & 0xFF);
						++size;
					}

					guidCopy >>= 8;
				}
				createItemWriter.sink().write((const char*)&packGUID[0], size);
				createItemWriter
					<< io::write<NetUInt8>(objectTypeId)
					<< io::write<NetUInt8>(updateFlags);
				if (updateFlags & 0x08)
				{
					createItemWriter
						<< io::write<NetUInt32>(guidLowerPart(guid));
				}
				if (updateFlags & 0x10)
				{
					createItemWriter
						<< io::write<NetUInt32>((guid << 48) & 0x0000FFFF);
				}

				pair.second->writeValueUpdateBlock(createItemWriter, m_owner, true);
			}
			out_blocks.emplace_back(std::move(createItemBlock));
		}
	}
}
