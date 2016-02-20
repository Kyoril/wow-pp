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

#include "area_trigger_entry.h"
#include "templates/basic_template_save_context.h"

namespace wowpp
{
	AreaTriggerEntry::AreaTriggerEntry()
		: map(0)
		, x(0.0f)
		, y(0.0f)
		, z(0.0f)
		, radius(0.0f)
		, box_x(0.0f)
		, box_y(0.0f)
		, box_z(0.0f)
		, box_o(0.0f)
		, targetMap(0)
		, targetX(0.0f)
		, targetY(0.0f)
		, targetZ(0.0f)
		, targetO(0.0f)
	{
	}

	bool AreaTriggerEntry::load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		// Load name and directory
		wrapper.table.tryGetString("name", name);
		wrapper.table.tryGetInteger("map", map);
		wrapper.table.tryGetInteger("x", x);
		wrapper.table.tryGetInteger("y", y);
		wrapper.table.tryGetInteger("z", z);
		wrapper.table.tryGetInteger("radius", radius);
		wrapper.table.tryGetInteger("box_x", box_x);
		wrapper.table.tryGetInteger("box_y", box_y);
		wrapper.table.tryGetInteger("box_z", box_z);
		wrapper.table.tryGetInteger("box_o", box_o);

		const sff::read::tree::Table<DataFileIterator> *const teleportTable = wrapper.table.getTable("teleport");
		if (teleportTable)
		{
			teleportTable->tryGetInteger("map", targetMap);
			teleportTable->tryGetInteger("x", targetX);
			teleportTable->tryGetInteger("y", targetY);
			teleportTable->tryGetInteger("z", targetZ);
			teleportTable->tryGetInteger("o", targetO);
		}

		return true;
	}

	void AreaTriggerEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) {
			context.table.addKey("name", name);
		}
		if (map != 0) {
			context.table.addKey("map", map);
		}
		if (x != 0.0f) {
			context.table.addKey("x", x);
		}
		if (y != 0.0f) {
			context.table.addKey("y", y);
		}
		if (z != 0.0f) {
			context.table.addKey("z", z);
		}
		if (radius != 0.0f) {
			context.table.addKey("radius", radius);
		}
		if (box_x != 0.0f) {
			context.table.addKey("box_x", box_x);
		}
		if (box_y != 0.0f) {
			context.table.addKey("box_y", box_y);
		}
		if (box_z != 0.0f) {
			context.table.addKey("box_z", box_z);
		}
		if (box_o != 0.0f) {
			context.table.addKey("box_o", box_o);
		}

		if (targetMap != 0 || targetX != 0.0f || targetY != 0.0f || targetZ != 0.0f || targetO != 0.0f)
		{
			sff::write::Table<char> teleportTable(context.table, "teleport", sff::write::Comma);
			{
				teleportTable.addKey("map", targetMap);
				teleportTable.addKey("x", targetX);
				teleportTable.addKey("y", targetY);
				teleportTable.addKey("z", targetZ);
				teleportTable.addKey("o", targetO);
			}
			teleportTable.finish();
		}
	}
}
