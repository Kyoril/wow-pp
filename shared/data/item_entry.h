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

	namespace inventory_type
	{
		enum Type
		{
			NonEquip			= 0,
			Head				= 1,
			Neck				= 2,
			Shoulders			= 3,
			Body				= 4,
			Chest				= 5,
			Waist				= 6,
			Legs				= 7,
			Feet				= 8,
			Wrists				= 9,
			Hands				= 10,
			Finger				= 11,
			Trinket				= 12,
			Weapon				= 13,
			Shield				= 14,
			Ranged				= 15,
			Cloak				= 16,
			TwoHandWeapon		= 17,
			Bag					= 18,
			Tabard				= 19,
			Robe				= 20,
			WeaponMainHand		= 21,
			WeaponOffHand		= 22,
			Holdable			= 23,
			Ammo				= 24,
			Thrown				= 25,
			RangedRight			= 26,
			Quiver				= 27,
			Relic				= 28,

			Count_				= 29
		};
	}

	typedef inventory_type::Type InventoryType;

	namespace item_class
	{
		enum Type
		{
			Consumable		= 0,
			Container		= 1,
			Weapon			= 2,
			Gem				= 3,
			Armor			= 4,
			Reagent			= 5,
			Projectile		= 6,
			TradeGoods		= 7,
			Generic			= 8,
			Recipe			= 9,
			Money			= 10,
			Quiver			= 11,
			Quest			= 12,
			Key				= 13,
			Permanent		= 14,
			Junk			= 15,

			Count_			= 16,
		};
	}

	typedef item_class::Type ItemClass;

	namespace item_subclass_consumable
	{
		enum Type
		{
			Consumable		= 0,
			Potion			= 1,
			Elixir			= 2,
			Flask			= 3,
			Scroll			= 4,
			Food			= 5,
			ItemEnhancement	= 6,
			Bandage			= 7,
			ConsumableOther	= 8,

			Count_			= 9
		};
	}

	typedef item_subclass_consumable::Type ItemSubclassConsumable;

	namespace item_subclass_container
	{
		enum Type
		{
			Container				= 0,
			SoulContainer			= 1,
			HerbContainer			= 2,
			EnchantingContainer		= 3,
			EngineeringContainer	= 4,
			GemContainer			= 5,
			MiningContainer			= 6,
			LeatherworkingContainer	= 7,

			Count_					= 8
		};
	}

	typedef item_subclass_container::Type ItemSubclassContainer;

	namespace item_subclass_weapon
	{
		enum Type
		{
			Axe				= 0,
			Axe2			= 1,
			Bow				= 2,
			Gun				= 3,
			Mace			= 4,
			Mace2			= 5,
			Polearm			= 6,
			Sword			= 7,
			Sword2			= 8,
			Staff			= 10,
			Exotic			= 11,
			Ecotic2			= 12,
			Fist			= 13,
			Misc			= 14,
			Dagger			= 15,
			Thrown			= 16,
			Spear			= 17,
			CrossBow		= 18,
			Wand			= 19,
			FishingPole		= 20
		};
	}

	typedef item_subclass_weapon::Type ItemSubclassWeapon;

	namespace item_subclass_gem
	{
		enum Type
		{
			Red			= 0,
			Blue		= 1,
			Yellow		= 2,
			Purple		= 3,
			Green		= 4,
			Orange		= 5,
			Meta		= 6,
			Simple		= 7,
			Prismatic	= 8,

			Count_		= 9
		};
	}

	typedef item_subclass_gem::Type ItemSubclassGem;

	namespace item_subclass_armor
	{
		enum Type
		{
			Misc		= 0,
			Cloth		= 1,
			Leather		= 2,
			Mail		= 3,
			Plate		= 4,
			Buckler		= 5,
			Shield		= 6,
			Libram		= 7,
			Idol		= 8,
			Totem		= 9,

			Count_		= 10
		};
	}

	typedef item_subclass_armor::Type ItemSubclassArmor;

	namespace item_subclass_projectile
	{
		enum Type
		{
			Wand			= 0,
			Bolt			= 1,
			Arrow			= 2,
			Bullet			= 3,
			Thrown			= 4,

			Count_			= 5
		};
	}

	typedef item_subclass_projectile::Type ItemSubclassProjectile;

	namespace item_subclass_trade_goods
	{
		enum Type
		{
			TradeGoods			= 0,
			Parts				= 1,
			Eplosives			= 2,
			Devices				= 3,
			Jewelcrafting		= 4,
			Cloth				= 5,
			Leather				= 6,
			MetalStone			= 7,
			Meat				= 8,
			Herb				= 9,
			Elemental			= 10,
			TradeGoodsOther		= 11,
			Enchanting			= 12,
			Material			= 13,

			Count_				= 14
		};
	}

	typedef item_subclass_trade_goods::Type ItemSubclassTradeGoods;

	/// 
	struct ItemEntry : BasicTemplate<UInt32>
	{
		typedef BasicTemplate<UInt32> Super;

		/// 
		struct ItemStatEntry
		{
			UInt8 statType;
			Int16 statValue;

			/// 
			ItemStatEntry()
				: statType(0)
				, statValue(0)
			{
			}
		};

		/// 
		struct ItemDamageEntry
		{
			float min;
			float max;
			UInt8 type;

			/// 
			ItemDamageEntry()
				: min(0.0f)
				, max(0.0f)
				, type(0)
			{
			}
		};

		/// 
		struct ItemSpellEntry
		{
			const SpellEntry *spell;
			UInt8 trigger;
			Int16 charges;
			float procRate;
			Int32 cooldown;
			Int16 category;
			Int32 categoryCooldown;

			/// 
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

		/// 
		struct ItemSocketEntry
		{
			Int8 color;
			Int8 content;

			/// 
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
		Int32 material;
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

		/// 
		ItemEntry();

		bool load(DataLoadContext &context, const ReadTableWrapper &wrapper);
		void save(BasicTemplateSaveContext &context) const;
	};
}
