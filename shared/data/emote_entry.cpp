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

#include "emote_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"

namespace wowpp
{
	EmoteEntry::EmoteEntry()
		: Super()
		, textId(0)
	{
	}

	bool EmoteEntry::load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		wrapper.table.tryGetString("name", name);
		wrapper.table.tryGetInteger("text", textId);
		
		return true;
	}

	void EmoteEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) context.table.addKey("name", name);
		if (textId != 0) context.table.addKey("text", textId);
	}
}
