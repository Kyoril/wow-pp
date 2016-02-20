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

namespace wowpp
{
	struct AreaTriggerEntry : BasicTemplate<UInt32>
	{
		typedef BasicTemplate<UInt32> Super;

		String name;
		UInt32 map;
		float x;
		float y;
		float z;
		float radius;
		float box_x;
		float box_y;
		float box_z;
		float box_o;
		UInt32 targetMap;
		float targetX, targetY, targetZ, targetO;

		AreaTriggerEntry();
		bool load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper);
		void save(BasicTemplateSaveContext &context) const;
	};
}
