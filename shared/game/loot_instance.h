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
#include "common/simple.hpp"
#include "game/defines.h"
#include "binary_io/writer.h"
#include "shared/proto_data/loot_entry.pb.h"
#include "proto_data/project.h"

namespace wowpp
{
	class GameCharacter;

	struct LootItem final
	{
		bool isLooted;
		UInt32 count;
		proto::LootDefinition definition;

		explicit LootItem(UInt32 count, proto::LootDefinition def)
			: isLooted(false)
			, count(count)
			, definition(std::move(def))
		{
		}
	};

	/// Represents an instance of loot. This will, for example, be generated on
	/// creature death and can be sent to the client.
	class LootInstance
	{
		friend io::Writer &operator << (io::Writer &w, LootInstance const &loot);

	public:

		boost::signals2::signal<void()> cleared;
		/// Fired when a looting player closes the loot dialog.
		simple::signal<void(UInt64)> closed;
		/// Fired when gold was looted.
		simple::signal<void()> goldRemoved;
		/// Fired when an item has been removed.
		simple::signal<void(UInt8)> itemRemoved;

	public:

		// Some items exist once for every party member. Because of this, we use the data
		// structures below to keep track of the players who already looted their shared item
		// to prevent them from looting it twice

		typedef std::map<UInt32, UInt32> PlayerItemLootEntry;
		typedef std::map<UInt64, PlayerItemLootEntry> PlayerLootEntries;

	public:

		/// Initializes a new instance of the loot instance.
		explicit LootInstance(proto::ItemManager &items, UInt64 lootGuid);
		explicit LootInstance(proto::ItemManager &items, UInt64 lootGuid, const proto::LootEntry *entry, UInt32 minGold, UInt32 maxGold, const std::vector<GameCharacter *> &lootRecipients);

		///
		UInt64 getLootGuid() const {
			return m_lootGuid;
		}
		/// Determines whether the loot is empty.
		bool isEmpty() const;
		/// Determines whether a certain character can receive any loot from this instance.
		/// @param receiver The receivers GUID.
		bool containsLootFor(UInt64 receiver);
		///
		UInt32 getGold() const {
			return m_gold;
		}
		///
		void takeGold();
		/// Determines if there is gold available to loot.
		bool hasGold() const { return m_gold != 0; }
		/// Get loot item definition from the requested slot.
		const LootItem *getLootDefinition(UInt8 slot) const;
		///
		void takeItem(UInt8 slot, UInt64 receiver);
		/// Gets the number of items.
		UInt32 getItemCount() const {
			return m_items.size();
		}

		void serialize(io::Writer &writer, UInt64 receiver) const;

	private:

		void addLootItem(const proto::LootDefinition &def);

	private:

		proto::ItemManager &m_itemManager;
		UInt64 m_lootGuid;
		UInt32 m_gold;
		std::vector<LootItem> m_items;
		std::vector<UInt64> m_recipients;
		PlayerLootEntries m_playerLootData;
	};
}
