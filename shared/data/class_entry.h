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

#include "templates/basic_template.h"
#include "common/enum_strings.h"

namespace wowpp
{
	namespace power_type
	{
		enum Type
		{
			/// The most common one, mobs actually have this or rage.
			Mana			= 0x00,
			/// This is what warriors use to cast their spells.
			Rage			= 0x01,
			/// Unused in classic?
			Focus			= 0x02,
			/// Used by rogues to do their spells.
			Energy			= 0x03,
			/// Used by hunter pet's - more happiness increases pet damage.
			Happiness		= 0x04,
			///
			Health			= 0xFFFFFFFE,

			Count_			= 0x06,
			Invalid_ = Count_
		};
	}

	typedef power_type::Type PowerType;

	namespace class_flags
	{
		enum Type
		{
			/// Nothing special about the character class.
			None = 0x00,
			/// This class has a relic slot (used by druids and paladins).
			HasRelocSlot = 0x01,

			Count_,
			Invalid_ = Count_
		};
	}

	typedef class_flags::Type ClassFlags;

	namespace constant_literal
	{
		typedef EnumStrings<PowerType, power_type::Count_,
		        power_type::Invalid_> PowerTypeStrings;
		extern const PowerTypeStrings powerType;
	}

	struct ClassEntry : BasicTemplate<UInt32>
	{
		typedef BasicTemplate<UInt32> Super;

		struct ClassBaseValues
		{
			UInt32 health;
			UInt32 mana;

			ClassBaseValues()
				: health(0)
				, mana(0)
			{
			}
		};

		typedef std::map<UInt32, ClassBaseValues> ClassLevelBaseValues;

		PowerType powerType;
		String name;
		String internalName;
		UInt32 spellFamily;
		ClassFlags flags;
		ClassLevelBaseValues levelBaseValues;

		ClassEntry();
		bool load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper);
		void save(BasicTemplateSaveContext &context) const;
	};
}
