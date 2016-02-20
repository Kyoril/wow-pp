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

#include "zone_entry.h"
#include "templates/basic_template_save_context.h"
#include "map_entry.h"

namespace wowpp
{
	ZoneEntry::ZoneEntry()
		: Super()
		, parent(nullptr)
		, map(nullptr)
		, explore(0)
		, flags(0)
		, team(0)
		, level(0)
	{
	}

	bool ZoneEntry::load(DataLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		// Load name and directory
		wrapper.table.tryGetString("name", name);

		// Load the parent zone if any
		UInt32 parentId = 0;
		if (wrapper.table.tryGetInteger("parent", parentId) &&
		        parentId != 0)
		{
			context.loadLater.push_back([parentId, &context, this]() -> bool
			{
				this->parent = context.getZone(parentId);
				if (this->parent == nullptr)
				{
					context.onWarning("Could not find parent zone by index");
				}

				return true;
			});
		}

		UInt32 mapId = 0;
		wrapper.table.tryGetInteger("map", mapId);
		map = context.getMap(mapId);
		if (!map)
		{
			std::ostringstream strm;
			strm << "Could not find map which is referenced in an area or zone. Map-ID: " << mapId << "; Zone-ID: " << id;
			context.onWarning(strm.str());
		}

		wrapper.table.tryGetInteger("explore", explore);
		wrapper.table.tryGetInteger("flags", flags);
		wrapper.table.tryGetInteger("team", team);
		wrapper.table.tryGetInteger("level", level);

		return true;
	}

	void ZoneEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) {
			context.table.addKey("name", name);
		}
		if (parent) {
			context.table.addKey("parent", parent->id);
		}
		if (map) {
			context.table.addKey("map", map->id);
		}
		if (explore != 0) {
			context.table.addKey("explore", explore);
		}
		if (flags != 0) {
			context.table.addKey("flags", flags);
		}
		if (team != 0) {
			context.table.addKey("team", team);
		}
		if (level != 0) {
			context.table.addKey("level", level);
		}
	}
}
