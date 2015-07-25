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

#pragma once

#include "common/typedefs.h"
#include "templates/basic_template.h"
#include <array>

namespace wowpp
{
	struct UnitEntry : BasicTemplate<UInt32>
	{
		typedef BasicTemplate<UInt32> Super;

		String name;
		String subname;
		UInt32 minLevel, maxLevel;
		UInt32 allianceFactionID;
		UInt32 hordeFactionID;
		UInt32 maleModel;
		UInt32 femaleModel;
		float scale;
		UInt32 family;
		bool regeneratesHealth;
		UInt32 npcFlags;
		UInt32 unitFlags;
		UInt32 dynamicFlags;
		UInt32 extraFlags;
		UInt32 creatureTypeFlags;
		float walkSpeed;
		float runSpeed;
		UInt32 unitClass;
		UInt32 rank;
		UInt32 minLevelHealth, maxLevelHealth;
		UInt32 minLevelMana, maxLevelMana;
		float minMeleeDamage, maxMeleeDamage;
		float minRangedDamage, maxRangedDamage;
		UInt32 armor;
		std::array<UInt32, 6> resistances;
		UInt32 meleeBaseAttackTime;		// ms
		UInt32 rangedBaseAttackTime;	// ms
		UInt32 damageSchool;
		UInt32 minLootGold, maxLootGold;
		UInt32 xpMin, xpMax;

		UnitEntry();
		bool load(BasicTemplateLoadContext &context, const ReadTableWrapper &wrapper);
		void save(BasicTemplateSaveContext &context) const;
	};
}
