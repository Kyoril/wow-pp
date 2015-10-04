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
#include "binary_io/writer.h"
#include "data/loot_entry.h"

namespace wowpp
{
	/// Represents an instance of loot. This will, for example, be generated on
	/// creature death and can be sent to the client.
	class LootInstance
	{
		friend io::Writer &operator << (io::Writer &w, LootInstance const& loot);

	public:

		/// Initializes a new instance of the loot instance.
		explicit LootInstance();
		explicit LootInstance(const LootEntry &entry, UInt32 minGold, UInt32 maxGold);

		/// Determines whether the loot is empty.
		bool isEmpty() const { return (m_gold == 0 && m_items.empty()); }

	private:

		void addLootItem(const LootDefinition &def);

	private:

		UInt32 m_gold;
		std::vector<std::pair<UInt32, LootDefinition>> m_items;
	};

	io::Writer &operator << (io::Writer &w, LootInstance const& loot);
}
