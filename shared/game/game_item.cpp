//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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

#include "game_item.h"
#include "data/item_entry.h"

namespace wowpp
{
	GameItem::GameItem(const ItemEntry &entry)
		: GameObject()
		, m_entry(entry)
	{
		// Resize values field
		m_values.resize(item_fields::ItemFieldCount);
		m_valueBitset.resize((item_fields::ItemFieldCount + 31) / 32);

		// 2.3.2 - 0x18
		m_objectType |= type_mask::Item;
	}

	GameItem::~GameItem()
	{
	}

	void GameItem::initialize()
	{
		GameObject::initialize();

		setUInt32Value(object_fields::Entry, m_entry.id);
		setFloatValue(object_fields::ScaleX, 1.0f);

		setUInt32Value(item_fields::MaxDurability, m_entry.durability);
		setUInt32Value(item_fields::Durability, m_entry.durability);
		setUInt32Value(item_fields::StackCount, 1);
		setUInt32Value(item_fields::Flags, m_entry.flags);

		// Spell charges
		for (size_t i = 0; i < 5; ++i)
		{
			setUInt32Value(item_fields::SpellCharges + i, m_entry.itemSpells[i].charges);
		}
	}
}
