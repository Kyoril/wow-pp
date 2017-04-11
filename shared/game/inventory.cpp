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
#include "inventory.h"
#include "game_character.h"
#include "game_item.h"
#include "game_bag.h"
#include "world_instance.h"
#include "proto_data/project.h"
#include "common/linear_set.h"
#include "binary_io/vector_sink.h"
#include "binary_io/writer.h"
#include "binary_io/reader.h"
#include "wowpp_protocol/wowpp_world_realm.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	io::Writer &operator<<(io::Writer &w, ItemData const &object)
	{
		w.writePOD(object);
		return w;
	}

	io::Reader &operator>>(io::Reader &r, ItemData &object)
	{
		r.readPOD(object);
		return r;
	}


	Inventory::Inventory(GameCharacter &owner)
		: m_owner(owner)
		, m_freeSlots(player_inventory_pack_slots::End - player_inventory_pack_slots::Start)	// Default slot count with only a backpack
	{
		// Warning: m_owner might not be completely constructed at this point
	}
	game::InventoryChangeFailure Inventory::createItems(const proto::ItemEntry &entry, UInt16 amount /* = 1*/, std::map<UInt16, UInt16> *out_addedBySlot /* = nullptr*/)
	{
		// Incorrect value used, so give at least one item
		if (amount == 0) {
			amount = 1;
		}

		// Limit the total amount of items
		const UInt16 itemCount = getItemCount(entry.id());
		if (entry.maxcount() > 0 &&
		        UInt32(itemCount + amount) > entry.maxcount())
		{
			return game::inventory_change_failure::CantCarryMoreOfThis;
		}

		// Quick check if there are enough free slots (only works if we don't have an item of this type yet)
		const UInt16 requiredSlots = (amount - 1) / entry.maxstack() + 1;
		if ((itemCount == 0 || entry.maxstack() <= 1) &&
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

		forEachBag([this, &itemsProcessed, &availableStacks, &usedCapableSlots, &emptySlots, &requiredSlots, &itemCount, &entry](UInt8 bag, UInt8 slotStart, UInt8 slotEnd) -> bool
		{
			for (UInt8 slot = slotStart; slot < slotEnd; ++slot)
			{
				const UInt16 absoluteSlot = getAbsoluteSlot(bag, slot);

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

			// We can stop now
			if (itemsProcessed >= itemCount &&
			        emptySlots.size() >= requiredSlots)
			{
				return false;
			}

			return true;
		});

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

				if (out_addedBySlot) {
					out_addedBySlot->insert(std::make_pair(slot, added));
				}

				// Notify update
				itemInstanceUpdated(item, slot);
				if (isInventorySlot(slot) || isEquipmentSlot(slot) || isBagPackSlot(slot))
				{
					m_owner.forceFieldUpdate(character_fields::InvSlotHead + ((slot & 0xFF) * 2));
					m_owner.forceFieldUpdate(character_fields::InvSlotHead + ((slot & 0xFF) * 2) + 1);
				}
				else if (isBagSlot(slot))
				{
					auto bag = getBagAtSlot(slot);
					if (bag)
					{
						bag->forceFieldUpdate(bag_fields::Slot_1 + ((slot & 0xFF) * 2));
						bag->forceFieldUpdate(bag_fields::Slot_1 + ((slot & 0xFF) * 2) + 1);
						itemInstanceUpdated(bag, slot);
					}
				}
			}

			// Everything added
			if (amountLeft == 0) {
				break;
			}
		}

		// Are there still items left?
		if (amountLeft)
		{
			// Now iterate through all empty slots
			for (auto &slot : emptySlots)
			{
				// Create a new item instance (or maybe a bag if it is a bag)
				std::shared_ptr<GameItem> item;
				if (entry.itemclass() == game::item_class::Container ||
					entry.itemclass() == game::item_class::Quiver)
				{
					item = std::make_shared<GameBag>(m_owner.getProject(), entry);
				}
				else
				{
					item = std::make_shared<GameItem>(m_owner.getProject(), entry);
				}
				item->initialize();

				// We need a valid world instance for this
				auto *world = m_owner.getWorldInstance();
				ASSERT(world);

				// Determine slot
				UInt8 bag = 0, subslot = 0;
				getRelativeSlots(slot, bag, subslot);

				// Generate a new id for this item based on the characters world instance
				auto newItemId = world->getItemIdGenerator().generateId();
				item->setGuid(createEntryGUID(newItemId, entry.id(), wowpp::guid_type::Item));
				if (isBagSlot(slot))
				{
					auto bagInst = getBagAtSlot(slot);
					item->setUInt64Value(item_fields::Contained, bagInst ? bagInst->getGuid() : m_owner.getGuid());
				}
				else
				{
					item->setUInt64Value(item_fields::Contained, m_owner.getGuid());
				}
				item->setUInt64Value(item_fields::Owner, m_owner.getGuid());

				// Bind this item
				if (entry.bonding() == game::item_binding::BindWhenPickedUp)
				{
					item->addFlag(item_fields::Flags, game::item_flags::Bound);
				}

				// One stack has been created by initializing the item
				amountLeft--;

				// Modify stack count
				UInt16 added = item->addStacks(amountLeft);
				amountLeft -= added;

				// Increase cached counter
				m_itemCounter[entry.id()] += added + 1;
				if (out_addedBySlot) {
					out_addedBySlot->insert(std::make_pair(slot, added + 1));
				}

				// Add this item to the inventory slot and reduce our free slot cache
				m_itemsBySlot[slot] = item;
				m_freeSlots--;

				// Watch for item despawn packet
				m_itemDespawnSignals[item->getGuid()] 
					= item->despawned.connect(std::bind(&Inventory::onItemDespawned, this, std::placeholders::_1));

				// Create the item instance
				itemInstanceCreated(item, slot);

				// Update player fields
				if (bag == player_inventory_slots::Bag_0)
				{
					m_owner.setUInt64Value(character_fields::InvSlotHead + (subslot * 2), item->getGuid());
					if (isBagBarSlot(slot))
						m_owner.applyItemStats(*item, true);

					if (isEquipmentSlot(slot))
					{
						m_owner.setUInt32Value(character_fields::VisibleItem1_0 + (subslot * 16), item->getEntry().id());
						m_owner.setUInt64Value(character_fields::VisibleItem1_CREATOR + (subslot * 16), item->getUInt64Value(item_fields::Creator));
						m_owner.applyItemStats(*item, true);
					}
				}
				else if (isBagSlot(slot))
				{
					auto packSlot = getAbsoluteSlot(player_inventory_slots::Bag_0, bag);
					auto bagInst = getBagAtSlot(packSlot);
					if (bagInst)
					{
						bagInst->setUInt64Value(bag_fields::Slot_1 + (subslot * 2), item->getGuid());
						itemInstanceUpdated(bagInst, packSlot);
					}
				}

				// All done
				if (amountLeft == 0) {
					break;
				}
			}
		}

		// WARNING: There should never be any items left here!
		ASSERT(amountLeft == 0);
		if (amountLeft > 0)
		{
			ELOG("Could not add all items, something went really wrong! " << __FUNCTION__);
			return game::inventory_change_failure::InventoryFull;
		}

		// Quest check
		m_owner.onQuestItemAddedCredit(entry, amount);

		// Everything okay
		return game::inventory_change_failure::Okay;
	}
	game::InventoryChangeFailure Inventory::addItem(std::shared_ptr<GameItem> item, UInt16 * out_slot)
	{
		ASSERT(item);
		
		const auto &entry = item->getEntry();

		// Limit the total amount of items
		const UInt16 itemCount = getItemCount(entry.id());
		if (entry.maxcount() > 0 &&
			UInt32(itemCount + item->getStackCount()) > entry.maxcount())
		{
			return game::inventory_change_failure::CantCarryMoreOfThis;
		}

		if (m_freeSlots < 1)
		{
			return game::inventory_change_failure::InventoryFull;
		}

		// Now check all bags for the next free slot
		UInt16 targetSlot = 0;
		forEachBag([this, &targetSlot](UInt8 bag, UInt8 slotStart, UInt8 slotEnd) -> bool
		{
			for (UInt8 slot = slotStart; slot < slotEnd; ++slot)
			{
				const UInt16 absoluteSlot = getAbsoluteSlot(bag, slot);

				// Check if this slot is empty
				auto it = m_itemsBySlot.find(absoluteSlot);
				if (it == m_itemsBySlot.end())
				{
					// Slot is empty - use it!
					targetSlot = absoluteSlot;
					return false;
				}
			}

			// Continue with the next bag
			return true;
		});

		// Check if an empty slot was found
		if (targetSlot == 0)
		{
			return game::inventory_change_failure::InventoryFull;
		}

		// Finally add the item
		UInt8 bag = 0, subslot = 0;
		getRelativeSlots(targetSlot, bag, subslot);

		// Fix item properties like container guid and owner guid
		if (isBagSlot(targetSlot))
		{
			auto bagInst = getBagAtSlot(targetSlot);
			ASSERT(bagInst);
			item->setUInt64Value(item_fields::Contained, bagInst ? bagInst->getGuid() : m_owner.getGuid());
		}
		else
		{
			item->setUInt64Value(item_fields::Contained, m_owner.getGuid());
		}
		item->setUInt64Value(item_fields::Owner, m_owner.getGuid());

		// Bind this item
		if (entry.bonding() == game::item_binding::BindWhenPickedUp)
		{
			item->addFlag(item_fields::Flags, game::item_flags::Bound);
		}

		// Increase cached counter
		m_itemCounter[entry.id()] += item->getStackCount();
		if (out_slot) {
			*out_slot = targetSlot;
		}

		// Add this item to the inventory slot and reduce our free slot cache
		m_itemsBySlot[targetSlot] = item;
		m_freeSlots--;

		// Watch for item despawn packet
		m_itemDespawnSignals[item->getGuid()]
			= item->despawned.connect(std::bind(&Inventory::onItemDespawned, this, std::placeholders::_1));

		// Create the item instance
		itemInstanceCreated(item, targetSlot);

		// Update player fields
		if (bag == player_inventory_slots::Bag_0)
		{
			m_owner.setUInt64Value(character_fields::InvSlotHead + (subslot * 2), item->getGuid());
			if (isBagBarSlot(targetSlot))
				m_owner.applyItemStats(*item, true);

			if (isEquipmentSlot(targetSlot))
			{
				m_owner.setUInt32Value(character_fields::VisibleItem1_0 + (subslot * 16), item->getEntry().id());
				m_owner.setUInt64Value(character_fields::VisibleItem1_CREATOR + (subslot * 16), item->getUInt64Value(item_fields::Creator));
				m_owner.applyItemStats(*item, true);
			}
		}
		else if (isBagSlot(targetSlot))
		{
			auto packSlot = getAbsoluteSlot(player_inventory_slots::Bag_0, bag);
			auto bagInst = getBagAtSlot(packSlot);
			if (bagInst)
			{
				bagInst->setUInt64Value(bag_fields::Slot_1 + (subslot * 2), item->getGuid());
				itemInstanceUpdated(bagInst, packSlot);
			}
		}

		// Everything okay
		return game::inventory_change_failure::Okay;
	}
	game::InventoryChangeFailure Inventory::removeItems(const proto::ItemEntry &entry, UInt16 amount)
	{
		// If amount equals 0, remove ALL items of that entry.
		const UInt16 itemCount = getItemCount(entry.id());
		if (amount == 0) {
			amount = itemCount;
		}

		// We don't have enough items, so we don't need to bother iterating through
		if (itemCount < amount)
		{
			// Maybe use a different result
			return game::inventory_change_failure::ItemNotFound;
		}

		// Counter used to know when to stop iteration
		UInt16 itemsToDelete = amount;

		forEachBag([this, &itemCount, &entry, &itemsToDelete](UInt8 bag, UInt8 slotStart, UInt8 slotEnd) -> bool
		{
			for (UInt8 slot = slotStart; slot < slotEnd; ++slot)
			{
				const UInt16 absoluteSlot = getAbsoluteSlot(bag, slot);

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
					m_itemCounter[entry.id()] -= itemsToDelete;
					itemsToDelete = 0;

					// Notify client about this update
					itemInstanceUpdated(it->second, slot);
				}

				// All items processed, we can stop here
				if (itemsToDelete == 0) {
					return false;
				}
			}

			return true;
		});

		// WARNING: There should never be any items left here!
		ASSERT(itemsToDelete == 0);
		ASSERT(m_itemCounter[entry.id()] == itemCount - amount);

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
		if (stacks == 0 || stacks > stackCount) {
			stacks = stackCount;
		}
		m_itemCounter[it->second->getEntry().id()] -= stacks;

		// Increase ref count since the item instance might be destroyed below
		auto item = it->second;
		if (stackCount == stacks)
		{
			// No longer watch for item despawn
			m_itemDespawnSignals.erase(item->getGuid());

			// Remove item from slot
			m_itemsBySlot.erase(it);
			m_freeSlots++;

			UInt8 bag = 0, subslot = 0;
			getRelativeSlots(absoluteSlot, bag, subslot);
			if (bag == player_inventory_slots::Bag_0)
			{
				m_owner.setUInt64Value(character_fields::InvSlotHead + (subslot * 2), 0);
				if (isBagBarSlot(absoluteSlot))
					m_owner.applyItemStats(*item, false);

				if (isEquipmentSlot(absoluteSlot))
				{
					m_owner.setUInt32Value(character_fields::VisibleItem1_0 + (subslot * 16), item->getEntry().id());
					m_owner.setUInt64Value(character_fields::VisibleItem1_CREATOR + (subslot * 16), item->getUInt64Value(item_fields::Creator));
					m_owner.applyItemStats(*item, false);
				}
			}
			else if (isBagSlot(absoluteSlot))
			{
				auto packSlot = getAbsoluteSlot(player_inventory_slots::Bag_0, bag);
				auto bagInst = getBagAtSlot(packSlot);
				if (bagInst)
				{
					bagInst->setUInt64Value(bag_fields::Slot_1 + (subslot * 2), 0);
					itemInstanceUpdated(bagInst, packSlot);
				}
			}

			// Notify about destruction
			itemInstanceDestroyed(item, absoluteSlot);
		}
		else
		{
			item->setUInt32Value(item_fields::StackCount, stackCount - stacks);
			itemInstanceUpdated(it->second, absoluteSlot);
		}

		// Quest check
		m_owner.onQuestItemRemovedCredit(item->getEntry(), stacks);
		return game::inventory_change_failure::Okay;
	}
	game::InventoryChangeFailure Inventory::removeItemByGUID(UInt64 guid, UInt16 stacks)
	{
		UInt16 slot = 0;
		if (!findItemByGUID(guid, slot))
		{
			return game::inventory_change_failure::InternalBagError;
		}

		return removeItem(slot, stacks);
	}
	game::InventoryChangeFailure Inventory::swapItems(UInt16 slotA, UInt16 slotB)
	{
		// We need a valid source slot
		auto srcItem = getItemAtSlot(slotA);
		auto dstItem = getItemAtSlot(slotB);
		if (!srcItem)
		{
			m_owner.inventoryChangeFailure(game::inventory_change_failure::ItemNotFound, srcItem.get(), dstItem.get());
			return game::inventory_change_failure::ItemNotFound;
		}

		// Owner has to be alive
		if (!m_owner.isAlive())
		{
			m_owner.inventoryChangeFailure(game::inventory_change_failure::YouAreDead, srcItem.get(), dstItem.get());
			return game::inventory_change_failure::YouAreDead;
		}

		// Can't change equipment while in combat!
		if (m_owner.isInCombat() && isEquipmentSlot(slotA))
		{
			if ((slotA & 0xFF) != player_equipment_slots::Mainhand &&
				(slotA & 0xFF) != player_equipment_slots::Offhand &&
				(slotA & 0xFF) != player_equipment_slots::Ranged)
			{
				m_owner.inventoryChangeFailure(game::inventory_change_failure::NotInCombat, srcItem.get(), dstItem.get());
				return game::inventory_change_failure::NotInCombat;
			}
		}
		if (m_owner.isInCombat() && isEquipmentSlot(slotB))
		{
			if ((slotB & 0xFF) != player_equipment_slots::Mainhand &&
				(slotB & 0xFF) != player_equipment_slots::Offhand &&
				(slotB & 0xFF) != player_equipment_slots::Ranged)
			{
				m_owner.inventoryChangeFailure(game::inventory_change_failure::NotInCombat, srcItem.get(), dstItem.get());
				return game::inventory_change_failure::NotInCombat;
			}
		}

		// Verify destination slot for source item
		auto result = isValidSlot(slotB, srcItem->getEntry());
		if (result != game::inventory_change_failure::Okay)
		{
			m_owner.inventoryChangeFailure(result, srcItem.get(), dstItem.get());
			return result;
		}

		// If there is an item in the destination slot, also verify the source slot
		if (dstItem)
		{
			result = isValidSlot(slotA, dstItem->getEntry());
			if (result != game::inventory_change_failure::Okay)
			{
				m_owner.inventoryChangeFailure(result, srcItem.get(), dstItem.get());
				return result;
			}

			// Check if both items share the same entry
			if (srcItem->getEntry().id() == dstItem->getEntry().id())
			{
				const UInt32 maxStack = srcItem->getEntry().maxstack();
				if (maxStack > 1 && maxStack > dstItem->getStackCount())
				{
					// Check if we can just add stacks to the destination item
					const UInt32 availableDstStacks = maxStack - dstItem->getStackCount();
					if (availableDstStacks > 0)
					{
						if (availableDstStacks < srcItem->getStackCount())
						{
							// Everything swapped
							dstItem->addStacks(availableDstStacks);
							srcItem->setUInt32Value(item_fields::StackCount, srcItem->getStackCount() - availableDstStacks);
							itemInstanceUpdated(dstItem, slotB);
							itemInstanceUpdated(srcItem, slotA);
							return game::inventory_change_failure::Okay;
						}
						else	// Available Stacks >= srcItem stack count
						{
							dstItem->addStacks(availableDstStacks);
							itemInstanceUpdated(dstItem, slotB);

							// Remove source item
							m_owner.setUInt64Value(character_fields::InvSlotHead + (slotA & 0xFF) * 2, 0);

							// No longer watch for item despawn
							m_itemDespawnSignals.erase(srcItem->getGuid());
							itemInstanceDestroyed(srcItem, slotA);
							m_itemsBySlot.erase(slotA);
							m_freeSlots++;

							return game::inventory_change_failure::Okay;
						}
					}
				}
			}
		}

		if (srcItem->getTypeId() == object_type::Container &&
		        !isBagPackSlot(slotB))
		{
			if (!std::dynamic_pointer_cast<GameBag>(srcItem)->isEmpty())
			{
				m_owner.inventoryChangeFailure(game::inventory_change_failure::CanOnlyDoWithEmptyBags, srcItem.get(), dstItem.get());
				return game::inventory_change_failure::CanOnlyDoWithEmptyBags;
			}
		}

		// Everything seems to be okay, swap items
		if (isEquipmentSlot(slotA) || isInventorySlot(slotA) || isBagPackSlot(slotA))
		{
			m_owner.setUInt64Value(character_fields::InvSlotHead + (slotA & 0xFF) * 2, (dstItem ? dstItem->getGuid() : 0));

			if (dstItem &&
			        dstItem->getUInt64Value(item_fields::Contained) != m_owner.getGuid())
			{
				dstItem->setUInt64Value(item_fields::Contained, m_owner.getGuid());
				itemInstanceUpdated(dstItem, slotA);
			}
		}
		else if (isBagSlot(slotA))
		{
			auto bag = getBagAtSlot(slotA);
			if (!bag)
			{
				m_owner.inventoryChangeFailure(game::inventory_change_failure::InternalBagError, srcItem.get(), dstItem.get());
				return game::inventory_change_failure::InternalBagError;
			}

			bag->setUInt64Value(bag_fields::Slot_1 + (slotA & 0xFF) * 2, (dstItem ? dstItem->getGuid() : 0));
			itemInstanceUpdated(bag, getAbsoluteSlot(player_inventory_slots::Bag_0, slotA >> 8));

			if (dstItem &&
			        dstItem->getUInt64Value(item_fields::Contained) != bag->getGuid())
			{
				dstItem->setUInt64Value(item_fields::Contained, bag->getGuid());
				itemInstanceUpdated(dstItem, slotA);
			}
		}

		if (isEquipmentSlot(slotB) || isInventorySlot(slotB) || isBagPackSlot(slotB))
		{
			m_owner.setUInt64Value(character_fields::InvSlotHead + (slotB & 0xFF) * 2, srcItem->getGuid());

			if (srcItem->getUInt64Value(item_fields::Contained) != m_owner.getGuid())
			{
				srcItem->setUInt64Value(item_fields::Contained, m_owner.getGuid());
				itemInstanceUpdated(srcItem, slotB);
			}
		}
		else if (isBagSlot(slotB))
		{
			auto bag = getBagAtSlot(slotB);
			if (!bag)
			{
				m_owner.inventoryChangeFailure(game::inventory_change_failure::InternalBagError, srcItem.get(), dstItem.get());
				return game::inventory_change_failure::InternalBagError;
			}

			bag->setUInt64Value(bag_fields::Slot_1 + (slotB & 0xFF) * 2, srcItem->getGuid());
			itemInstanceUpdated(bag, getAbsoluteSlot(player_inventory_slots::Bag_0, slotA >> 8));

			if (srcItem->getUInt64Value(item_fields::Contained) != bag->getGuid())
			{
				srcItem->setUInt64Value(item_fields::Contained, bag->getGuid());
				itemInstanceUpdated(srcItem, slotB);
			}
		}

		// Adjust bag slots
		const bool isBagPackA = isBagPackSlot(slotA);
		const bool isBagBackB = isBagPackSlot(slotB);
		if (isBagPackA && !isBagBackB)
		{
			if (!dstItem &&
			        srcItem->getTypeId() == object_type::Container)
			{
				m_freeSlots -= srcItem->getUInt32Value(bag_fields::NumSlots);
			}
			else if (
			    dstItem &&
			    dstItem->getTypeId() == object_type::Container &&
			    srcItem->getTypeId() == object_type::Container)
			{
				m_freeSlots -= srcItem->getUInt32Value(bag_fields::NumSlots);
				m_freeSlots += dstItem->getUInt32Value(bag_fields::NumSlots);
			}
		}
		else if (isBagBackB && !isBagPackA)
		{
			if (!dstItem &&
			        srcItem->getTypeId() == object_type::Container)
			{
				m_freeSlots += srcItem->getUInt32Value(bag_fields::NumSlots);
			}
			else if (
			    dstItem &&
			    dstItem->getTypeId() == object_type::Container &&
			    srcItem->getTypeId() == object_type::Container)
			{
				m_freeSlots -= dstItem->getUInt32Value(bag_fields::NumSlots);
				m_freeSlots += srcItem->getUInt32Value(bag_fields::NumSlots);
			}
		}

		std::swap(m_itemsBySlot[slotA], m_itemsBySlot[slotB]);
		if (!dstItem)
		{
			// Remove new item in SlotA
			m_itemsBySlot.erase(slotA);

			// No item in slot B, and slot A was an inventory slot, so this gives us another free slot
			// if slot B is not an inventory/bag slot, too
			if ((isInventorySlot(slotA) || isBagSlot(slotA)) &&
			        !(isInventorySlot(slotB) || isBagSlot(slotB)))
			{
				m_freeSlots++;
			}
			// No item in slot A, and slot B is an inventory slot, so this will use another free slot
			else if ((isInventorySlot(slotB) || isBagSlot(slotB)) &&
			         !(isInventorySlot(slotA) || isBagSlot(slotA)))
			{
				ASSERT(m_freeSlots >= 1);
				m_freeSlots--;
			}
		}

		// Apply bag stats (mainly for quivers so far...)
		if (isBagBarSlot(slotA))
		{
			m_owner.applyItemStats(*srcItem, false);
			if (dstItem)
			{
				m_owner.applyItemStats(*dstItem, true);
			}
		}
		if (isBagBarSlot(slotB))
		{
			m_owner.applyItemStats(*srcItem, true);
			if (dstItem)
			{
				m_owner.applyItemStats(*dstItem, false);
			}
		}

		// Update visuals
		if (isEquipmentSlot(slotA))
		{
			m_owner.setUInt32Value(character_fields::VisibleItem1_0 + ((slotA & 0xFF) * 16), (dstItem ? dstItem->getEntry().id() : 0));
			m_owner.setUInt64Value(character_fields::VisibleItem1_CREATOR + ((slotA & 0xFF) * 16), (dstItem ? dstItem->getUInt64Value(item_fields::Creator) : 0));
			m_owner.applyItemStats(*srcItem, false);
			if (srcItem->getEntry().itemset() != 0)
			{
				onSetItemUnequipped(srcItem->getEntry().itemset());
			}
			if (dstItem) 
			{
				m_owner.applyItemStats(*dstItem, true);
				if (dstItem->getEntry().itemset() != 0)
				{
					onSetItemEquipped(dstItem->getEntry().itemset());
				}
			}
		}
		if (isEquipmentSlot(slotB))
		{
			m_owner.setUInt32Value(character_fields::VisibleItem1_0 + ((slotB & 0xFF) * 16), srcItem->getEntry().id());
			m_owner.setUInt64Value(character_fields::VisibleItem1_CREATOR + ((slotB & 0xFF) * 16), srcItem->getUInt64Value(item_fields::Creator));
			m_owner.applyItemStats(*srcItem, true);
			if (srcItem->getEntry().itemset() != 0)
			{
				onSetItemEquipped(srcItem->getEntry().itemset());
			}
			if (dstItem) 
			{
				m_owner.applyItemStats(*dstItem, false);
				if (dstItem->getEntry().itemset() != 0)
				{
					onSetItemUnequipped(dstItem->getEntry().itemset());
				}
			}

			// Bind this item
			if (srcItem->getEntry().bonding() == game::item_binding::BindWhenEquipped)
			{
				srcItem->addFlag(item_fields::Flags, game::item_flags::Bound);
				itemInstanceUpdated(srcItem, slotB);
			}
		}
		else if (isBagPackSlot(slotB))
		{
			if (srcItem->getEntry().bonding() == game::item_binding::BindWhenEquipped)
			{
				srcItem->addFlag(item_fields::Flags, game::item_flags::Bound);
			}
		}

		return game::inventory_change_failure::Okay;
	}
	namespace
	{
		game::weapon_prof::Type weaponProficiency(UInt32 subclass)
		{
			switch (subclass)
			{
			case game::item_subclass_weapon::Axe:
				return game::weapon_prof::OneHandAxe;
			case game::item_subclass_weapon::Axe2:
				return game::weapon_prof::TwoHandAxe;
			case game::item_subclass_weapon::Bow:
				return game::weapon_prof::Bow;
			case game::item_subclass_weapon::CrossBow:
				return game::weapon_prof::Crossbow;
			case game::item_subclass_weapon::Dagger:
				return game::weapon_prof::Dagger;
			case game::item_subclass_weapon::Fist:
				return game::weapon_prof::Fist;
			case game::item_subclass_weapon::Gun:
				return game::weapon_prof::Gun;
			case game::item_subclass_weapon::Mace:
				return game::weapon_prof::OneHandMace;
			case game::item_subclass_weapon::Mace2:
				return game::weapon_prof::TwoHandMace;
			case game::item_subclass_weapon::Polearm:
				return game::weapon_prof::Polearm;
			case game::item_subclass_weapon::Staff:
				return game::weapon_prof::Staff;
			case game::item_subclass_weapon::Sword:
				return game::weapon_prof::OneHandSword;
			case game::item_subclass_weapon::Sword2:
				return game::weapon_prof::TwoHandSword;
			case game::item_subclass_weapon::Thrown:
				return game::weapon_prof::Throw;
			case game::item_subclass_weapon::Wand:
				return game::weapon_prof::Wand;
			}

			return game::weapon_prof::None;
		}
		game::armor_prof::Type armorProficiency(UInt32 subclass)
		{
			switch (subclass)
			{
			case game::item_subclass_armor::Misc:
				return game::armor_prof::Common;
			case game::item_subclass_armor::Buckler:
			case game::item_subclass_armor::Shield:
				return game::armor_prof::Shield;
			case game::item_subclass_armor::Cloth:
				return game::armor_prof::Cloth;
			case game::item_subclass_armor::Leather:
				return game::armor_prof::Leather;
			case game::item_subclass_armor::Mail:
				return game::armor_prof::Mail;
			case game::item_subclass_armor::Plate:
				return game::armor_prof::Plate;
			}

			return game::armor_prof::None;
		}
	}
	game::InventoryChangeFailure Inventory::isValidSlot(UInt16 slot, const proto::ItemEntry &entry) const
	{
		// Split the absolute slot
		UInt8 bag = 0, subslot = 0;
		if (!getRelativeSlots(slot, bag, subslot)) {
			return game::inventory_change_failure::InternalBagError;
		}

		// Check if it is a special bag....
		if (isEquipmentSlot(slot))
		{
			auto armorProf = m_owner.getArmorProficiency();
			auto weaponProf = m_owner.getWeaponProficiency();

			// Determine whether the provided inventory type can go into the slot
			if (entry.itemclass() == game::item_class::Weapon)
			{
				if ((weaponProf & weaponProficiency(entry.subclass())) == 0)
				{
					return game::inventory_change_failure::NoRequiredProficiency;
				}
			}
			else if (entry.itemclass() == game::item_class::Armor)
			{
				if ((armorProf & armorProficiency(entry.subclass())) == 0)
				{
					return game::inventory_change_failure::NoRequiredProficiency;
				}
			}

			if (entry.requiredlevel() > 0 &&
			        entry.requiredlevel() > m_owner.getLevel())
			{
				return game::inventory_change_failure::CantEquipLevel;
			}

			if (entry.requiredskill() != 0 &&
			        !m_owner.hasSkill(entry.requiredskill()))
			{
				return game::inventory_change_failure::CantEquipSkill;
			}

			// Validate equipment slots
			auto srcInvType = entry.inventorytype();
			switch (subslot)
			{
			case player_equipment_slots::Head:
				if (srcInvType != game::inventory_type::Head) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Body:
				if (srcInvType != game::inventory_type::Body) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Chest:
				if (srcInvType != game::inventory_type::Chest &&
				        srcInvType != game::inventory_type::Robe) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Feet:
				if (srcInvType != game::inventory_type::Feet) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Neck:
				if (srcInvType != game::inventory_type::Neck) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Ranged:
				if (srcInvType != game::inventory_type::Ranged &&
					srcInvType != game::inventory_type::Thrown &&
					srcInvType != game::inventory_type::RangedRight) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Finger1:
			case player_equipment_slots::Finger2:
				if (srcInvType != game::inventory_type::Finger) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Trinket1:
			case player_equipment_slots::Trinket2:
				if (srcInvType != game::inventory_type::Trinket) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Hands:
				if (srcInvType != game::inventory_type::Hands) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Legs:
				if (srcInvType != game::inventory_type::Legs) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Mainhand:
				if (srcInvType != game::inventory_type::MainHandWeapon &&
				        srcInvType != game::inventory_type::TwoHandedWeapon &&
				        srcInvType != game::inventory_type::Weapon)
				{
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				else if (srcInvType == game::inventory_type::TwoHandedWeapon)
				{
					auto offhand = getItemAtSlot(getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Offhand));
					if (offhand)
					{
						// We need to be able to store the offhand weapon in the inventory
						auto result = canStoreItems(offhand->getEntry());
						if (result != game::inventory_change_failure::Okay)
						{
							return result;
						}
					}
				}
				break;
			case player_equipment_slots::Offhand:
				if (srcInvType != game::inventory_type::OffHandWeapon &&
				        srcInvType != game::inventory_type::Shield &&
				        srcInvType != game::inventory_type::Weapon &&
						srcInvType != game::inventory_type::Holdable)
				{
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				else
				{
					if (srcInvType != game::inventory_type::Shield &&
						srcInvType != game::inventory_type::Holdable &&
					        !m_owner.canDualWield())
					{
						return game::inventory_change_failure::CantDualWield;
					}

					auto item = getItemAtSlot(getAbsoluteSlot(player_inventory_slots::Bag_0, player_equipment_slots::Mainhand));
					if (item &&
					        item->getEntry().inventorytype() == game::inventory_type::TwoHandedWeapon)
					{
						return game::inventory_change_failure::CantEquipWithTwoHanded;
					}
				}
				break;
			case player_equipment_slots::Shoulders:
				if (srcInvType != game::inventory_type::Shoulders) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Tabard:
				if (srcInvType != game::inventory_type::Tabard) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Waist:
				if (srcInvType != game::inventory_type::Waist) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Wrists:
				if (srcInvType != game::inventory_type::Wrists) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			case player_equipment_slots::Back:
				if (srcInvType != game::inventory_type::Cloak) {
					return game::inventory_change_failure::ItemDoesNotGoToSlot;
				}
				break;
			default:
				return game::inventory_change_failure::ItemDoesNotGoToSlot;
			}

			return game::inventory_change_failure::Okay;
		}
		else if (isInventorySlot(slot))
		{
			// TODO: Inventory slot validation? However, isInventorySlot already
			// performs some checks
			return game::inventory_change_failure::Okay;
		}
		else if (isBagSlot(slot))
		{
			// Validate bag
			auto bag = getBagAtSlot(slot);
			if (!bag)
			{
				return game::inventory_change_failure::ItemDoesNotGoToSlot;
			}

			if (subslot >= bag->getSlotCount())
			{
				return game::inventory_change_failure::ItemDoesNotGoToSlot;
			}
			
			if (bag->getEntry().itemclass() == game::item_class::Quiver &&
				entry.inventorytype() != game::inventory_type::Ammo)
			{
				return game::inventory_change_failure::OnlyAmmoCanGoHere;
			}

			return game::inventory_change_failure::Okay;
		}
		else if (isBagPackSlot(slot))
		{
			if (entry.itemclass() != game::item_class::Container &&
				entry.itemclass() != game::item_class::Quiver)
			{
				return game::inventory_change_failure::NotABag;
			}

			// Make sure that we have only up to one quiver equipped at a time
			if (entry.itemclass() == game::item_class::Quiver)
			{
				if (hasEquippedQuiver())
					return game::inventory_change_failure::CanEquipOnlyOneQuiver;
			}

			auto bagItem = getItemAtSlot(slot);
			if (bagItem)
			{
				if (bagItem->getTypeId() != object_type::Container)
				{
					// Return code valid? ...
					return game::inventory_change_failure::NotABag;
				}

				auto castedBag = std::static_pointer_cast<GameBag>(bagItem);
				ASSERT(castedBag);

				if (!castedBag->isEmpty())
				{
					return game::inventory_change_failure::CanOnlyDoWithEmptyBags;
				}
			}

			return game::inventory_change_failure::Okay;
		}

		return game::inventory_change_failure::InternalBagError;
	}
	game::InventoryChangeFailure Inventory::canStoreItems(const proto::ItemEntry &entry, UInt16 amount) const
	{
		// Incorrect value used, so give at least one item
		if (amount == 0) {
			amount = 1;
		}

		// Limit the total amount of items
		const UInt16 itemCount = getItemCount(entry.id());
		if (entry.maxcount() > 0 &&
		        UInt32(itemCount + amount) > entry.maxcount())
		{
			return game::inventory_change_failure::CantCarryMoreOfThis;
		}

		// Quick check if there are enough free slots (only works if we don't have an item of this type yet)
		const UInt16 requiredSlots = (amount - 1) / entry.maxstack() + 1;
		if ((itemCount == 0 || entry.maxstack() <= 1) &&
		        requiredSlots > m_freeSlots)
		{
			return game::inventory_change_failure::InventoryFull;
		}

		// TODO

		return game::inventory_change_failure::Okay;
	}
	UInt16 Inventory::getItemCount(UInt32 itemId) const
	{
		auto it = m_itemCounter.find(itemId);
		return (it != m_itemCounter.end() ? it->second : 0);
	}
	bool Inventory::hasEquippedQuiver() const
	{
		for (UInt8 slot = player_inventory_slots::Start; slot < player_inventory_slots::End; ++slot)
		{
			auto testBag = getItemAtSlot(getAbsoluteSlot(player_inventory_slots::Bag_0, slot));
			if (testBag && testBag->getEntry().itemclass() == game::item_class::Quiver)
				return true;
		}

		return false;
	}
	UInt16 Inventory::getAbsoluteSlot(UInt8 bag, UInt8 slot)
	{
		return (bag << 8) | slot;
	}
	bool Inventory::getRelativeSlots(UInt16 absoluteSlot, UInt8 &out_bag, UInt8 &out_slot)
	{
		out_bag = static_cast<UInt8>(absoluteSlot >> 8);
		out_slot = static_cast<UInt8>(absoluteSlot & 0xFF);
		return true;
	}
	std::shared_ptr<GameItem> Inventory::getItemAtSlot(UInt16 absoluteSlot) const
	{
		auto it = m_itemsBySlot.find(absoluteSlot);
		if (it != m_itemsBySlot.end()) {
			return it->second;
		}

		return std::shared_ptr<GameItem>();
	}
	std::shared_ptr<GameBag> Inventory::getBagAtSlot(UInt16 absoluteSlot) const
	{
		if (!isBagPackSlot(absoluteSlot))
		{
			// Convert bag slot to bag pack slot which is 0xFFXX where XX is the bag slot id
			absoluteSlot = getAbsoluteSlot(player_inventory_slots::Bag_0, UInt8(absoluteSlot >> 8));
		}

		auto it = m_itemsBySlot.find(absoluteSlot);
		if (it != m_itemsBySlot.end() && it->second->getTypeId() == object_type::Container)
		{
			return std::dynamic_pointer_cast<GameBag>(it->second);
		}

		return std::shared_ptr<GameBag>();
	}
	std::shared_ptr<GameItem> Inventory::getWeaponByAttackType(game::WeaponAttack attackType, bool nonbroken, bool useable) const
	{
		UInt8 slot;

		switch (attackType)
		{
			case game::weapon_attack::BaseAttack:
				slot = player_equipment_slots::Mainhand;
				break;
			case game::weapon_attack::OffhandAttack:
				slot = player_equipment_slots::Offhand;
				break;
			case game::weapon_attack::RangedAttack:
				slot = player_equipment_slots::Ranged;
				break;
		}

		std::shared_ptr<GameItem> item = getItemAtSlot(getAbsoluteSlot(player_inventory_slots::Bag_0, slot));

		if (!item || item->getEntry().itemclass() != game::item_class::Weapon)
		{
			return nullptr;
		}

		if (nonbroken && item->isBroken())
		{
			return nullptr;
		}

		if (useable && !m_owner.canUseWeapon(attackType))
		{
			return nullptr;
		}

		return item;
	}
	bool Inventory::findItemByGUID(UInt64 guid, UInt16 &out_slot) const
	{
		for (auto &item : m_itemsBySlot)
		{
			if (item.second->getGuid() == guid)
			{
				out_slot = item.first;
				return true;
			}
		}

		return false;
	}
	bool Inventory::isEquipmentSlot(UInt16 absoluteSlot)
	{
		return (
		           absoluteSlot >> 8 == player_inventory_slots::Bag_0 &&
		           (absoluteSlot & 0xFF) < player_equipment_slots::End
		       );
	}
	bool Inventory::isBagPackSlot(UInt16 absoluteSlot)
	{
		return (
		           absoluteSlot >> 8 == player_inventory_slots::Bag_0 &&
		           (absoluteSlot & 0xFF) >= player_inventory_slots::Start &&
		           (absoluteSlot & 0xFF) < player_inventory_slots::End
		       );
	}
	bool Inventory::isInventorySlot(UInt16 absoluteSlot)
	{
		return (
		           absoluteSlot >> 8 == player_inventory_slots::Bag_0 &&
		           (absoluteSlot & 0xFF) >= player_inventory_pack_slots::Start &&
		           (absoluteSlot & 0xFF) < player_inventory_pack_slots::End
		       );
	}
	bool Inventory::isBagSlot(UInt16 absoluteSlot)
	{
		return (
		           absoluteSlot >> 8 >= player_inventory_slots::Start &&
		           absoluteSlot >> 8 < player_inventory_slots::End);
	}
	bool Inventory::isBagBarSlot(UInt16 absoluteSlot)
	{
		return (
			absoluteSlot >> 8 == player_inventory_slots::Bag_0 &&
			(absoluteSlot & 0xFF) >= player_inventory_slots::Start &&
			(absoluteSlot & 0xFF) < player_inventory_slots::End
			);
	}
	void Inventory::addRealmData(const ItemData &data)
	{
		m_realmData.push_back(data);
	}
	void Inventory::addSpawnBlocks(std::vector<std::vector<char>> &out_blocks)
	{
		// Reconstruct realm data if available
		if (!m_realmData.empty())
		{
			// World instance has to be ready
			auto *world = m_owner.getWorldInstance();
			if (!world)
			{
				return;
			}

			// We need to store bag items for later, since we first need to create all bags
			std::map<UInt16, std::shared_ptr<GameItem>> bagItems;

			// Iterate through all entries
			for (auto &data : m_realmData)
			{
				auto *entry = m_owner.getProject().items.getById(data.entry);
				if (!entry)
				{
					ELOG("Could not find item " << data.entry);
					continue;
				}

				// Create a new item instance
				std::shared_ptr<GameItem> item;
				if (entry->itemclass() == game::item_class::Container ||
					entry->itemclass() == game::item_class::Quiver)
				{
					item = std::make_shared<GameBag>(m_owner.getProject(), *entry);
				}
				else
				{
					item = std::make_shared<GameItem>(m_owner.getProject(), *entry);
				}
				auto newItemId = world->getItemIdGenerator().generateId();
				item->setGuid(createEntryGUID(newItemId, entry->id(), wowpp::guid_type::Item));
				item->initialize();
				item->setUInt64Value(item_fields::Owner, m_owner.getGuid());
				//item->setUInt64Value(item_fields::Creator, data.creator);
				item->setUInt64Value(item_fields::Contained, m_owner.getGuid());
				item->setUInt32Value(item_fields::Durability, data.durability);
				if (entry->bonding() == game::item_binding::BindWhenPickedUp)
				{
					item->addFlag(item_fields::Flags, game::item_flags::Bound);
				}

				// Add this item to the inventory slot
				m_itemsBySlot[data.slot] = item;

				// Determine slot
				UInt8 bag = 0, subslot = 0;
				getRelativeSlots(data.slot, bag, subslot);
				if (bag == player_inventory_slots::Bag_0)
				{
					if (isBagBarSlot(data.slot))
						m_owner.applyItemStats(*item, true);

					if (isEquipmentSlot(data.slot))
					{
						m_owner.setUInt64Value(character_fields::InvSlotHead + (subslot * 2), item->getGuid());
						m_owner.setUInt32Value(character_fields::VisibleItem1_0 + (subslot * 16), item->getEntry().id());
						m_owner.setUInt64Value(character_fields::VisibleItem1_CREATOR + (subslot * 16), item->getUInt64Value(item_fields::Creator));
						m_owner.applyItemStats(*item, true);
						if (item->getEntry().itemset() != 0)
						{
							onSetItemEquipped(item->getEntry().itemset());
						}

						// Apply bonding
						if (entry->bonding() == game::item_binding::BindWhenEquipped)
						{
							item->addFlag(item_fields::Flags, game::item_flags::Bound);
						}
					}
					else if (isInventorySlot(data.slot))
					{
						m_owner.setUInt64Value(character_fields::InvSlotHead + (subslot * 2), item->getGuid());
					}
					else if (isBagPackSlot(data.slot) && item->getTypeId() == object_type::Container)
					{
						m_owner.setUInt64Value(character_fields::InvSlotHead + (subslot * 2), item->getGuid());

						// Increase slot count since this is an equipped bag
						m_freeSlots += reinterpret_cast<GameBag *>(item.get())->getSlotCount();

						// Apply bonding
						if (entry->bonding() == game::item_binding::BindWhenEquipped)
						{
							item->addFlag(item_fields::Flags, game::item_flags::Bound);
						}
					}
				}
				else if (isBagSlot(data.slot))
				{
					bagItems[data.slot] = item;
				}

				// Modify stack count
				(void)item->addStacks(data.stackCount - 1);
				m_itemCounter[data.entry] += data.stackCount;

				// Watch for item despawn packet
				m_itemDespawnSignals[item->getGuid()]
					= item->despawned.connect(std::bind(&Inventory::onItemDespawned, this, std::placeholders::_1));

				// Quest check
				m_owner.onQuestItemAddedCredit(item->getEntry(), data.stackCount);

				// Inventory slot used
				if (isInventorySlot(data.slot) || isBagSlot(data.slot)) {
					m_freeSlots--;
				}
			}

			// Clear realm data since we don't need it any more
			m_realmData.clear();

			// Store items in bags
			for (auto &pair : bagItems)
			{
				auto bag = getBagAtSlot(getAbsoluteSlot(player_inventory_slots::Bag_0, pair.first >> 8));
				if (!bag)
				{
					ELOG("Could not find bag at slot " << pair.first << ": Maybe this bag is sent after the item");
				}
				else
				{
					pair.second->setUInt64Value(item_fields::Contained, bag->getGuid());
					bag->setUInt64Value(bag_fields::Slot_1 + ((pair.first & 0xFF) * 2), pair.second->getGuid());
				}
			}
		}

		for (auto &pair : m_itemsBySlot)
		{
			std::vector<char> createItemBlock;
			io::VectorSink createItemSink(createItemBlock);
			io::Writer createItemWriter(createItemSink);
			{
				UInt8 updateType = 0x02;						// Item
				UInt8 updateFlags = 0x08 | 0x10;				//
				UInt64 guid = pair.second->getGuid();

				UInt8 objectTypeId = pair.second->getTypeId();	// Item

				// Header with object guid and type
				createItemWriter
				    << io::write<NetUInt8>(updateType)
					<< io::write_packed_guid(guid)
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
			out_blocks.push_back(std::move(createItemBlock));
		}
	}

	void Inventory::forEachBag(BagCallbackFunc callback)
	{
		// Enumerates all possible bags
		static std::array<UInt8, 5> bags =
		{
			player_inventory_slots::Bag_0,
			player_inventory_slots::Start + 0,
			player_inventory_slots::Start + 1,
			player_inventory_slots::Start + 2,
			player_inventory_slots::Start + 3
		};

		for (const auto &bag : bags)
		{
			UInt8 slotStart = 0, slotEnd = 0;
			if (bag == player_inventory_slots::Bag_0)
			{
				slotStart = player_inventory_pack_slots::Start;
				slotEnd = player_inventory_pack_slots::End;
			}
			else
			{
				auto bagInst = getBagAtSlot(getAbsoluteSlot(player_inventory_slots::Bag_0, bag));
				if (!bagInst)
				{
					// Skip this bag
					continue;
				}

				slotStart = 0;
				slotEnd = bagInst->getSlotCount();
			}

			if (slotEnd <= slotStart)
			{
				continue;
			}

			if (!callback(bag, slotStart, slotEnd))
			{
				break;
			}
		}
	}

	void Inventory::onItemDespawned(GameObject & object)
	{
		if (object.getTypeId() != object_type::Item &&
			object.getTypeId() != object_type::Container)
		{
			return;
		}

		// Find item slot by guid
		UInt16 slot = 0;
		if (!findItemByGUID(object.getGuid(), slot))
		{
			WLOG("Could not find item by slot!");
			return;
		}

		// Destroy this item
		removeItem(slot);
	}

	void Inventory::onSetItemEquipped(UInt32 set)
	{
		auto *setEntry = m_owner.getProject().itemSets.getById(set);
		if (!setEntry)
		{
			return;
		}

		SpellTargetMap targetMap;
		targetMap.m_targetMap = game::spell_cast_target_flags::Unit;
		targetMap.m_unitTarget = m_owner.getGuid();

		auto &counter = m_setItems[set];
		for (auto &spell : setEntry->spells())
		{
			if (spell.itemcount() == counter + 1)
			{
				// Apply spell
				m_owner.castSpell(targetMap, spell.spell(), { 0, 0, 0 }, 0, true);
			}
		}

		// Increment counter
		counter++;
	}

	void Inventory::onSetItemUnequipped(UInt32 set)
	{
		auto *setEntry = m_owner.getProject().itemSets.getById(set);
		if (!setEntry)
		{
			return;
		}

		auto &counter = m_setItems[set];
		for (auto &spell : setEntry->spells())
		{
			if (spell.itemcount() == counter)
			{
				// Remove spell auras
				m_owner.getAuras().removeAllAurasDueToSpell(spell.spell());
			}
		}

		// Increment counter
		counter--;
	}

	io::Writer &operator << (io::Writer &w, Inventory const &object)
	{
		if (object.m_realmData.empty())
		{
			// Inventory has actual item instances, so we serialize this object for realm usage
			w
			        << io::write<NetUInt16>(object.m_itemsBySlot.size());
			for (const auto &pair : object.m_itemsBySlot)
			{
				ItemData data;
				data.entry = pair.second->getEntry().id();
				data.slot = pair.first;
				data.stackCount = pair.second->getStackCount();
				data.creator = pair.second->getUInt64Value(item_fields::Creator);
				data.contained = pair.second->getUInt64Value(item_fields::Contained);
				data.durability = pair.second->getUInt32Value(item_fields::Durability);
				data.randomPropertyIndex = 0;
				data.randomSuffixIndex = 0;
				w
				        << data;
			}
		}
		else
		{
			// Inventory has realm data left, and no item instances
			w
			        << io::write<NetUInt16>(object.m_realmData.size());
			for (const auto &data : object.m_realmData)
			{
				w
				        << data;
			}
		}

		return w;
	}
	io::Reader &operator >> (io::Reader &r, Inventory &object)
	{
		object.m_itemsBySlot.clear();
		object.m_freeSlots = player_inventory_pack_slots::End - player_inventory_pack_slots::Start;
		object.m_itemCounter.clear();
		object.m_realmData.clear();

		// Read amount of items
		UInt16 itemCount = 0;
		r >> io::read<NetUInt16>(itemCount);

		// Read realm data
		object.m_realmData.resize(itemCount);
		for (UInt16 i = 0; i < itemCount; ++i)
		{
			r >> object.m_realmData[i];
		}

		return r;
	}
}
