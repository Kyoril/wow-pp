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

#include "templates/basic_template.h"
#include "common/enum_strings.h"
#include <array>

namespace wowpp
{
	struct SpellEntry;
	struct DataLoadContext;

	struct ItemEntry : BasicTemplate<UInt32>
	{
		typedef BasicTemplate<UInt32> Super;

		struct ItemStatEntry
		{
			UInt8 statType;
			Int16 statValue;

			ItemStatEntry()
				: statType(0)
				, statValue(0)
			{
			}
		};

		struct ItemDamageEntry
		{
			float min;
			float max;
			UInt8 type;

			ItemDamageEntry()
				: min(0.0f)
				, max(0.0f)
				, type(0)
			{
			}
		};

		struct ItemSpellEntry
		{
			const SpellEntry *spell;
			UInt8 trigger;
			Int16 charges;
			float procRate;
			Int32 cooldown;
			Int16 category;
			Int32 categoryCooldown;

			ItemSpellEntry()
				: spell(nullptr)
				, trigger(0)
				, charges(0)
				, procRate(0.0f)
				, cooldown(0)
				, category(0)
				, categoryCooldown(0)
			{
			}
		};

		struct ItemSocketEntry
		{
			Int8 color;
			Int8 content;

			ItemSocketEntry()
				: color(0)
				, content(0)
			{
			}
		};

		String name;
		String description;
		UInt32 itemClass;
		UInt32 subClass;
		UInt32 displayId;
		UInt32 quality;
		UInt32 flags;
		UInt32 buyCount;
		UInt32 buyPrice;
		UInt32 sellPrice;
		UInt32 inventoryType;
		Int32 allowedClasses;
		Int32 allowedRaces;
		UInt32 itemLevel;
		UInt32 requiredLevel;
		UInt32 requiredSkill;
		UInt32 requiredSkillRank;
		UInt32 requiredSpell;
		UInt32 requiredHonorRank;
		UInt32 requiredCityRank;
		UInt32 requiredReputation;
		UInt32 requiredReputationRank;
		UInt32 maxCount;
		UInt32 maxStack;
		UInt32 containerSlots;
		std::array<ItemStatEntry, 10> itemStats;
		std::array<ItemDamageEntry, 5> itemDamage;
		UInt32 armor;
		UInt32 holyResistance;
		UInt32 fireResistance;
		UInt32 natureResistance;
		UInt32 frostResistance;
		UInt32 shadowResistance;
		UInt32 arcaneResistance;
		UInt32 delay;
		UInt32 ammoType;
		std::array<ItemSpellEntry, 5> itemSpells;
		UInt8 bonding;
		UInt32 lockId;
		UInt8 sheath;
		UInt16 randomProperty;
		UInt16 randomSuffix;
		UInt16 block;
		UInt16 set;
		UInt16 durability;
		UInt32 area;
		Int32 map;
		Int16 bagFamily;
		Int8 totemCategory;
		std::array<ItemSocketEntry, 3> itemSockets;
		UInt16 socketBonus;
		UInt16 gemProperties;
		Int16 requiredDisenchantSkill;
		UInt16 disenchantId;
		UInt8 foodType;
		UInt32 minLootGold;
		UInt32 maxLootGold;
		UInt32 duration;
		UInt32 extraFlags;

		ItemEntry();
		bool load(DataLoadContext &context, const ReadTableWrapper &wrapper);
		void save(BasicTemplateSaveContext &context) const;
	};
}
