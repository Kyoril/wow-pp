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
#include "data_load_context.h"

namespace wowpp
{
	struct MapEntry;

	/// Contains data for a zone antry. A zone may have a parent, in which case the zone is not a
	/// zone, but an area of a zone.
	struct ZoneEntry : BasicTemplate<UInt32>
	{
		typedef BasicTemplate<UInt32> Super;

		/// Name of the zone.
		String name;
		/// Parent of the zone.
		const ZoneEntry *parent;
		/// Map where this zone is referenced at.
		const MapEntry *map;
		/// The exploration bit which is set if this area is explored.
		UInt32 explore;
		/// Zone flags.
		UInt32 flags;
		/// Which faction does control this zone? This field decides whether to turn on pvp mode on pvp realms.
		UInt32 team;
		/// This field is used to calculate the amount of XP gained when exploring this zone.
		Int32 level;

		/// Default constructor.
		ZoneEntry();
		/// Loads a zone entry from a data file.
		bool load(DataLoadContext &context, const ReadTableWrapper &wrapper);
		/// Writes a zone entry into a data file.
		void save(BasicTemplateSaveContext &context) const;
	};
}
