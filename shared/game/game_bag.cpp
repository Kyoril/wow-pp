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

#include "game_bag.h"
#include "proto_data/project.h"

namespace wowpp
{
	GameBag::GameBag(proto::Project &project, const proto::ItemEntry &entry)
		: GameItem(project, entry)
	{
		// Resize values field
		m_values.resize(bag_fields::BagFieldCount);
		m_valueBitset.resize((bag_fields::BagFieldCount + 31) / 32);

		// 2.3.2 - 0x18
		m_objectType |= type_mask::Container;
	}

	GameBag::~GameBag()
	{
	}

	void GameBag::initialize()
	{
		GameItem::initialize();
		
		auto &entry = getEntry();
		setUInt32Value(bag_fields::NumSlots, entry.containerslots());
		for (UInt32 slot = 0; slot < 36; slot++)
		{
			// Initialize bag slots to 0
			setUInt64Value(bag_fields::Slot_1 + slot * 2, 0);
		}
	}

	bool GameBag::isEmpty() const
	{
		for (UInt32 slot = 0; slot < 36; slot++)
		{
			if (getUInt64Value(bag_fields::Slot_1 + slot * 2))
			{
				return false;
			}
		}
		return true;
	}

	io::Writer & operator<<(io::Writer &w, GameBag const& object)
	{
		return w
			<< reinterpret_cast<GameItem const&>(object);
	}

	io::Reader & operator>>(io::Reader &r, GameBag& object)
	{
		return r
			>> reinterpret_cast<GameItem&>(object);
	}

}
