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

#pragma once

#include "common/typedefs.h"
#include "game/defines.h"
#include <boost/noncopyable.hpp>
#include <boost/signals2.hpp>
#include <memory>
#include <map>

namespace wowpp
{
	// Class forwarding to reduce number of included files
	class GameCharacter;
	class GameItem;
	namespace proto
	{
		class ItemEntry;
	}

	/// Represents a characters inventory and provides functionalities like
	/// adding and organizing items.
	class Inventory : public boost::noncopyable
	{
	public:

		// NOTE: We provide shared_ptr<> because these signals could be used to collect changed items
		// and send a bundled package to the client, so these item instances may need to stay alive
		// until then.

		/// Fired when a new item instance was created.
		boost::signals2::signal<void(std::shared_ptr<GameItem>, UInt16)> itemInstanceCreated;
		/// Fired when an item instance was updated (stack count changed for example).
		boost::signals2::signal<void(std::shared_ptr<GameItem>, UInt16)> itemInstanceUpdated;
		/// Fired when an item instance is about to be destroyed.
		boost::signals2::signal<void(std::shared_ptr<GameItem>, UInt16)> itemInstanceDestroyed;

	public:

		/// Initializes a new instance of the inventory class.
		/// @param owner The owner of this inventory.
		explicit Inventory(GameCharacter &owner);

		/// Tries to add multiple items of the same entry to the inventory.
		/// @param entry The item template to be used for creating new items.
		/// @param amount The amount of items to create, has to be greater than 0.
		/// @returns game::inventory_change_failure::Okay if succeeded.
		game::InventoryChangeFailure createItems(const proto::ItemEntry &entry, UInt16 amount = 1);
		/// Tries to remove multiple items of the same entry.
		/// @param entry The item template to delete.
		/// @param amount The amount of items to delete. If 0, ALL items of that entry are deleted.
		/// @returns game::inventory_change_failure::Okay if succeeded.
		game::InventoryChangeFailure removeItems(const proto::ItemEntry &entry, UInt16 amount = 1);
		/// Tries to remove an item at a specified slot.
		/// @param absoluteSlot The item slot to delete from.
		/// @param stacks The number of stacks to remove. If 0, ALL stacks will be removed. Stacks are capped automatically,
		///               so that if stacks >= actual item stacks, simply all stacks will be removed as well.
		/// @returns game::inventory_change_failure::Okay if succeeded.
		game::InventoryChangeFailure removeItem(UInt16 absoluteSlot, UInt16 stacks = 0);
		/// Tries to swap two slots. Can also be used to move items, if one of the slots is
		/// empty.
		/// @param slotA The first (source) slot.
		/// @param slotB The second (destination) slot.
		/// @returns game::inventory_change_failure::Okay if succeeded.
		game::InventoryChangeFailure swapItems(UInt16 slotA, UInt16 slotB);
		// TODO: Add item split functionality

		/// Gets a reference of the owner of this inventory.
		GameCharacter &getOwner() { return m_owner; }
		/// Gets the total amount of a specific item in the inventory. This method uses a cache,
		/// so you don't need to worry about performance too much here.
		/// @param itemId The entry id of the searched item.
		/// @returns Number of items that the player has.
		UInt16 getItemCount(UInt32 itemId) const;
		/// Determines whether the player has an item in his inventory. This method is merely 
		/// syntactic sugar, it simply checks if getItemCount(itemId) is greater that 0.
		/// @param itemid The entry id of the searched item.
		/// @returns true if the player has the item.
		bool hasItem(UInt32 itemId) const { return getItemCount(itemId) > 0; }
		/// Gets an absolute slot position from a bag index and a bag slot.
		UInt16 getAbsoluteSlot(UInt8 bag, UInt8 slot) const;
		/// Splits an absolute slot into a bag index and a bag slot.
		bool getRelativeSlots(UInt16 absoluteSlot, UInt8 &out_bag, UInt8 &out_slot) const;
		/// Gets the amount of free inventory slots.
		UInt16 getFreeSlotCount() const { return m_freeSlots; }

	private:

		GameCharacter &m_owner;
		/// Stores the actual item instances by slot.
		std::map<UInt16, std::shared_ptr<GameItem>> m_itemsBySlot;
		/// Used to cache count of items. This is needed to performantly check for total amount
		/// of items in some checks (so we don't have to iterate through the whole inventory).
		std::map<UInt32, UInt16> m_itemCounter;
		/// Cached amount of free slots. Used for performance
		UInt16 m_freeSlots;
	};
}
