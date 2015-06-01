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

#include "class_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"

namespace wowpp
{
	namespace constant_literal
	{
		const PowerTypeStrings::StringArray strings =
		{{
			"mana",
			"rage",
			"focus",
			"energy",
			"happiness"
			}
		};


		static_assert(power_type::Mana == 0, "");
		static_assert(power_type::Rage == 1, "");
		static_assert(power_type::Focus == 2, "");
		static_assert(power_type::Energy == 3, "");
		static_assert(power_type::Happiness == 4, "");
		static_assert(power_type::Count_ == 5, "");

		const PowerTypeStrings powerType(strings);
	}


	ClassEntry::ClassEntry()
	{
	}

	bool ClassEntry::load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		// Load name and directory
		wrapper.table.tryGetString("name", name);
		wrapper.table.tryGetString("internalName", internalName);

		// Load power type
		powerType =
			constant_literal::powerType.getIdentifier(wrapper.table.getString("powerType"));
		if (powerType == power_type::Invalid_)
		{
			context.onError("Invalid class power type");
			return false;
		}

		// Load spellFamily and flags
		wrapper.table.tryGetInteger("spellFamily", spellFamily);
		flags = static_cast<ClassFlags>(wrapper.table.getInteger("flags", 0));

		return true;
	}

	void ClassEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) context.table.addKey("name", name);
		if (!internalName.empty()) context.table.addKey("internalName", internalName);
		context.table.addKey("powerType", constant_literal::powerType.getName(powerType));
		context.table.addKey("flags", flags);
		context.table.addKey("spellFamily", spellFamily);
	}
}
