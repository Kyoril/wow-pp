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

#include "game_item.h"

namespace wowpp
{
	namespace bag_fields
	{
		enum Enum
		{
			NumSlots			= item_fields::ItemFieldCount + 0x0000,	// Number of slots (1-36)
			AlignPad			= item_fields::ItemFieldCount + 0x0001,	// Unused
			Slot_1				= item_fields::ItemFieldCount + 0x0002,	// 36 UInt64 values (item guids)

			BagFieldCount		= item_fields::ItemFieldCount + 0x004A
		};
	}

	typedef bag_fields::Enum BagFields;

	///
	class GameBag : public GameItem
	{
		friend io::Writer &operator << (io::Writer &w, GameBag const &object);
		friend io::Reader &operator >> (io::Reader &r, GameBag &object);

	public:

		///
		explicit GameBag(proto::Project &project, const proto::ItemEntry &entry);
		~GameBag();

		virtual void initialize() override;

		virtual ObjectType getTypeId() const override {
			return object_type::Container;
		}

		/// Gets the amount of total slots in this bag.
		UInt32 getSlotCount() const {
			return getUInt32Value(bag_fields::NumSlots);
		}
		/// Determines whether this bag is empty.
		bool isEmpty() const;

	private:


	};

	io::Writer &operator << (io::Writer &w, GameBag const &object);
	io::Reader &operator >> (io::Reader &r, GameBag &object);
}
