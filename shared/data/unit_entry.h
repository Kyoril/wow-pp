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
#include "templates/basic_template.h"
#include <array>

namespace wowpp
{
	struct DataLoadContext;
	struct TriggerEntry;
	struct ItemEntry;
	struct FactionTemplateEntry;

	/// Stores creature related data.
	struct UnitEntry : BasicTemplate<UInt32>
	{
		typedef BasicTemplate<UInt32> Super;

		/// Name of the creature.
		String name;
		/// Subname of the creature. Will be displayed below the name like this: <subname>
		String subname;
		/// Minimum level and maximum level. If different, the level will be randomized for each spawn.
		UInt32 minLevel, maxLevel;
		/// Faction for alliance units.
		const FactionTemplateEntry *allianceFaction;
		/// Faction for horde units.
		const FactionTemplateEntry *hordeFaction;
		/// Display id of this unit if male unit.
		UInt32 maleModel;
		/// Display id of this unit if female unit.
		UInt32 femaleModel;
		/// Unit scale factor. A value of 1.0 means 100%.
		float scale;
		/// Unit type like "Humanoid" etc.
		UInt32 type;
		/// Unit family type
		UInt32 family;
		/// If true, this unit will regenerate health. Some units should not regenerate health, since it is
		/// required to heal them as a quest target for example.
		bool regeneratesHealth;
		/// NPC related flags, like: Questgiver, Vendor etc.
		UInt32 npcFlags;
		/// Unit related flags, like: Non-attackable, Non-selectable etc.
		UInt32 unitFlags;
		/// Dynamic flags
		UInt32 dynamicFlags;
		/// Core-specific flags (not sent to client).
		UInt32 extraFlags;
		/// 
		UInt32 creatureTypeFlags;
		/// Walk speed factor. A value of 1.0 means player walk speed.
		float walkSpeed;
		/// Run speed factor. A value of 1.0 means player run speed.
		float runSpeed;
		/// Units class values. Decied, which resource to use.
		UInt32 unitClass;
		/// Unit rank, like Elite, Boss, Rare etc.
		UInt32 rank;
		/// Maximum health value, based on spawn level (interpolated where min at minLevel and max at maxLevel).
		UInt32 minLevelHealth, maxLevelHealth;
		/// Minimum mana value, based on spawn level (interpolated where min at minLevel and max at maxLevel).
		UInt32 minLevelMana, maxLevelMana;
		/// Minimum raw melee damage (reduced by targets armor value).
		float minMeleeDamage, maxMeleeDamage;
		/// Minimum raw ranged damage (reduced by targets armor value).
		float minRangedDamage, maxRangedDamage;
		/// Armor value.
		UInt32 armor;
		/// Elemental resistance array.
		std::array<UInt32, 6> resistances;
		/// Time between melee auto attacks in milliseconds.
		UInt32 meleeBaseAttackTime;		// ms
		/// Time between ranged auto attacks in milliseconds.
		UInt32 rangedBaseAttackTime;	// ms
		/// Melee damage school.
		UInt32 damageSchool;
		/// Minimum gold drop.
		UInt32 minLootGold, maxLootGold;
		/// Minimum experience reward at equal level. Will be modified depending on group members in range and level difference.
		UInt32 xpMin, xpMax;
		/// Triggers to be raised by this unit.
		std::vector<const TriggerEntry*> triggers;
		/// Triggers filtered by events.
		std::map<UInt32, std::vector<const TriggerEntry*>> triggersByEvent;
		/// Equipment entries of this creature. Only has optical effect right now, maybe should also modify damage, attack speed and amor?
		const ItemEntry *mainHand, *offHand, *ranged;
		/// 
		UInt32 attackPower;
		/// 
		UInt32 rangedAttackPower;

		/// Default constructor.
		UnitEntry();
		/// Loads this unit entry from a data file.
		/// @param context Context which provides access to other data entries and additional functionalities.
		/// @param wrapper 
		bool load(DataLoadContext &context, const ReadTableWrapper &wrapper);
		/// Saves this unit entry into a data file.
		/// @param context Context which stores some objects and provides more information needed for saving.
		void save(BasicTemplateSaveContext &context) const;
	};
}
