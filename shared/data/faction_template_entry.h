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

#include "common/typedefs.h"
#include "data_load_context.h"
#include "templates/basic_template.h"

namespace wowpp
{
	struct FactionEntry;

	struct FactionTemplateEntry : BasicTemplate<UInt32>
	{
		typedef BasicTemplate<UInt32> Super;

		UInt32 flags;
		UInt32 selfMask;
		UInt32 friendMask;
		UInt32 enemyMask;
		const FactionEntry *faction;
		std::vector<const FactionEntry*> friends;
		std::vector<const FactionEntry*> enemies;
		
		FactionTemplateEntry();
		bool load(DataLoadContext &context, const ReadTableWrapper &wrapper);
		void save(BasicTemplateSaveContext &context) const;

		/// Determines whether this faction template entry is friendly towards a specific other entry.
		/// @param entry The other faction template entry to check.
		bool isFriendlyTo(const FactionTemplateEntry &entry) const;
		/// Determines whether this faction template entry is hostile towards a specific other entry.
		/// @param entry The other faction template entry to check.
		bool isHostileTo(const FactionTemplateEntry &entry) const;
		/// Determines if this faction template entry is hostile towards (all) players.
		bool isHostileToPlayers() const;
		/// Determines if this faction template entry is neutral towards all other factions.
		bool isNeutralToAll() const;
	};
}
