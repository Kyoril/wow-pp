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

#include "game_object.h"

namespace wowpp
{
	namespace proto
	{
		class ItemEntry;
	}

	namespace item_fields
	{
		enum Enum
		{
			Owner				= object_fields::ObjectFieldCount + 0x0000,
			Contained			= object_fields::ObjectFieldCount + 0x0002,
			Creator				= object_fields::ObjectFieldCount + 0x0004,
			GiftCreator			= object_fields::ObjectFieldCount + 0x0006,
			StackCount			= object_fields::ObjectFieldCount + 0x0008,
			Duration			= object_fields::ObjectFieldCount + 0x0009,
			SpellCharges		= object_fields::ObjectFieldCount + 0x000A,
			Flags				= object_fields::ObjectFieldCount + 0x000F,
			Enchantment			= object_fields::ObjectFieldCount + 0x0010,
			PropertySeed		= object_fields::ObjectFieldCount + 0x0031,
			RandomPropertiesID	= object_fields::ObjectFieldCount + 0x0032,
			ItemTextID			= object_fields::ObjectFieldCount + 0x0033,
			Durability			= object_fields::ObjectFieldCount + 0x0034,
			MaxDurability		= object_fields::ObjectFieldCount + 0x0035,

			ItemFieldCount		= object_fields::ObjectFieldCount + 0x0036
		};
	}

	typedef item_fields::Enum ItemFields;

	///
	class GameItem : public GameObject
	{
		friend io::Writer &operator << (io::Writer &w, GameItem const &object);
		friend io::Reader &operator >> (io::Reader &r, GameItem &object);

		boost::signals2::signal<void()> equipped;

	public:

		///
		explicit GameItem(proto::Project &project, const proto::ItemEntry &entry);
		~GameItem();

		virtual void initialize() override;

		virtual ObjectType getTypeId() const override {
			return object_type::Item;
		}
		const proto::ItemEntry &getEntry() const {
			return m_entry;
		}
		UInt32 getStackCount() const {
			return getUInt32Value(item_fields::StackCount);
		}
		/// Adds more stacks to this item instance. This also checks for stack limit.
		/// @param amount The amount to increase the stack for.
		/// @returns The amount of stacks that could be added.
		UInt16 addStacks(UInt16 amount);
		void notifyEquipped();

	private:

		const proto::ItemEntry &m_entry;
	};

	io::Writer &operator << (io::Writer &w, GameItem const &object);
	io::Reader &operator >> (io::Reader &r, GameItem &object);
}
