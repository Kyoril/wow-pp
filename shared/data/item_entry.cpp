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

#include "item_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"
#include "spell_entry.h"
#include "log/default_log_levels.h"
#include "data_load_context.h"
#include <memory>

namespace wowpp
{
	ItemEntry::ItemEntry()
		: Super()
		, itemClass(0)
		, subClass(0)
		, displayId(0)
		, quality(0)
		, flags(0)
		, buyCount(0)
		, buyPrice(0)
		, sellPrice(0)
		, inventoryType(0)
		, allowedClasses(-1)
		, allowedRaces(-1)
		, itemLevel(0)
		, requiredLevel(0)
		, requiredSkill(0)
		, requiredSkillRank(0)
		, requiredSpell(0)
		, requiredHonorRank(0)
		, requiredCityRank(0)
		, requiredReputation(0)
		, requiredReputationRank(0)
		, maxCount(0)
		, maxStack(0)
		, containerSlots(0)
		, armor(0)
		, holyResistance(0)
		, fireResistance(0)
		, natureResistance(0)
		, frostResistance(0)
		, shadowResistance(0)
		, arcaneResistance(0)
		, delay(0)
		, ammoType(0)
		, bonding(0)
		, lockId(0)
		, sheath(0)
		, randomProperty(0)
		, randomSuffix(0)
		, block(0)
		, set(0)
		, durability(0)
		, area(0)
		, map(0)
		, bagFamily(0)
		, material(0)
		, totemCategory(0)
		, socketBonus(0)
		, gemProperties(0)
		, requiredDisenchantSkill(0)
		, disenchantId(0)
		, foodType(0)
		, minLootGold(0)
		, maxLootGold(0)
		, duration(0)
		, extraFlags(0)
	{
	}

	bool ItemEntry::load(DataLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		// Load name and directory
		wrapper.table.tryGetString("name", name);
		wrapper.table.tryGetString("desc", description);
		wrapper.table.tryGetInteger("class", itemClass);
		wrapper.table.tryGetInteger("subclass", subClass);
		wrapper.table.tryGetInteger("display_id", displayId);
		wrapper.table.tryGetInteger("quality", quality);
		wrapper.table.tryGetInteger("flags", flags);
		wrapper.table.tryGetInteger("buy_count", buyCount);
		wrapper.table.tryGetInteger("buy_price", buyPrice);
		wrapper.table.tryGetInteger("sell_price", sellPrice);
		wrapper.table.tryGetInteger("inv_type", inventoryType);
		wrapper.table.tryGetInteger("allowed_class", allowedClasses);
		wrapper.table.tryGetInteger("allowed_race", allowedRaces);
		wrapper.table.tryGetInteger("level", itemLevel);
		wrapper.table.tryGetInteger("req_level", requiredLevel);
		wrapper.table.tryGetInteger("req_skill", requiredSkill);
		wrapper.table.tryGetInteger("req_skill_rank", requiredSkillRank);
		wrapper.table.tryGetInteger("req_spell", requiredSpell);
		wrapper.table.tryGetInteger("req_rank", requiredHonorRank);
		wrapper.table.tryGetInteger("req_city_rank", requiredCityRank);
		wrapper.table.tryGetInteger("req_rep_faction", requiredReputation);
		wrapper.table.tryGetInteger("req_rep_rank", requiredReputationRank);
		wrapper.table.tryGetInteger("max_count", maxCount);
		wrapper.table.tryGetInteger("max_stack", maxStack);
		wrapper.table.tryGetInteger("container_slots", containerSlots);
		wrapper.table.tryGetInteger("armor", armor);
		wrapper.table.tryGetInteger("holy_res", holyResistance);
		wrapper.table.tryGetInteger("fire_res", fireResistance);
		wrapper.table.tryGetInteger("nature_res", natureResistance);
		wrapper.table.tryGetInteger("frost_res", frostResistance);
		wrapper.table.tryGetInteger("shadow_res", shadowResistance);
		wrapper.table.tryGetInteger("arcane_res", arcaneResistance);
		wrapper.table.tryGetInteger("delay", delay);
		wrapper.table.tryGetInteger("ammo_type", ammoType);
		wrapper.table.tryGetInteger("bonding", bonding);
		wrapper.table.tryGetInteger("lock_id", lockId);
		wrapper.table.tryGetInteger("sheath", sheath);
		wrapper.table.tryGetInteger("random_property", randomProperty);
		wrapper.table.tryGetInteger("random_suffix", randomSuffix);
		wrapper.table.tryGetInteger("block", block);
		wrapper.table.tryGetInteger("set", set);
		wrapper.table.tryGetInteger("durability", durability);
		wrapper.table.tryGetInteger("area", area);
		wrapper.table.tryGetInteger("map", map);
		wrapper.table.tryGetInteger("bag_family", bagFamily);
		wrapper.table.tryGetInteger("material", material);
		wrapper.table.tryGetInteger("totem_category", totemCategory);
		wrapper.table.tryGetInteger("socket_bonus", socketBonus);
		wrapper.table.tryGetInteger("gem_properties", gemProperties);
		wrapper.table.tryGetInteger("req_disenchant_skill", requiredDisenchantSkill);
		wrapper.table.tryGetInteger("disenchant_id", disenchantId);
		wrapper.table.tryGetInteger("food_type", foodType);
		wrapper.table.tryGetInteger("min_loot_gold", minLootGold);
		wrapper.table.tryGetInteger("max_loot_gold", maxLootGold);
		wrapper.table.tryGetInteger("duration", duration);
		wrapper.table.tryGetInteger("extra_flags", extraFlags);

		// Read stats
		if (const sff::read::tree::Array<DataFileIterator> *const statArray = wrapper.table.getArray("stats"))
		{
			for (size_t j = 0; j < statArray->getSize(); ++j)
			{
				if (j >= itemStats.size())
				{
					context.onError("Items can only hold up to 10 stats, additional stat values will be ignored.");
					break;
				}

				const sff::read::tree::Table<DataFileIterator> *const statTable = statArray->getTable(j);
				if (!statTable)
				{
					context.onError("Invalid stat entry");
					return false;
				}

				statTable->tryGetInteger("type", itemStats[j].statType);
				statTable->tryGetInteger("value", itemStats[j].statValue);
			}
		}

		// Read damage
		if (const sff::read::tree::Array<DataFileIterator> *const dmgArray = wrapper.table.getArray("dmg"))
		{
			for (size_t j = 0; j < dmgArray->getSize(); ++j)
			{
				if (j >= itemDamage.size())
				{
					context.onError("Items can only hold up to 5 damage values, additional damage values will be ignored.");
					break;
				}

				const sff::read::tree::Table<DataFileIterator> *const dmgTable = dmgArray->getTable(j);
				if (!dmgTable)
				{
					context.onError("Invalid damage entry");
					return false;
				}

				dmgTable->tryGetInteger("min", itemDamage[j].min);
				dmgTable->tryGetInteger("max", itemDamage[j].max);
				dmgTable->tryGetInteger("type", itemDamage[j].type);
			}
		}

		// Read spells
		if (const sff::read::tree::Array<DataFileIterator> *const spellArray = wrapper.table.getArray("spells"))
		{
			for (size_t j = 0; j < spellArray->getSize(); ++j)
			{
				if (j >= itemSpells.size())
				{
					context.onError("Items can only hold up to 5 spells, additional spells will be ignored.");
					break;
				}

				const sff::read::tree::Table<DataFileIterator> *const spellTable = spellArray->getTable(j);
				if (!spellTable)
				{
					context.onError("Invalid spell table");
					return false;
				}

				UInt32 spellId = 0;
				spellTable->tryGetInteger("id", spellId);
				spellTable->tryGetInteger("trigger", itemSpells[j].trigger);
				spellTable->tryGetInteger("charges", itemSpells[j].charges);
				spellTable->tryGetInteger("proc_rate", itemSpells[j].procRate);
				spellTable->tryGetInteger("cooldown", itemSpells[j].cooldown);
				spellTable->tryGetInteger("category", itemSpells[j].category);
				spellTable->tryGetInteger("category_cooldown", itemSpells[j].categoryCooldown);

				context.loadLater.push_back(
					[j, spellId, this, &context]() -> bool
				{
					const SpellEntry * spell = context.getSpell(spellId);
					if (spell)
					{
						this->itemSpells[j].spell = spell;
						return true;
					}
					else
					{
						ELOG("Unknown spell " << spellId);
						return false;
					}
				});
			}
		}

		// Read sockets
		// TODO: Sockets

		return true;
	}

	void ItemEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) context.table.addKey("name", name);
		if (!description.empty()) context.table.addKey("desc", description);
		if (itemClass != 0) context.table.addKey("class", itemClass);
		if (subClass != 0) context.table.addKey("subclass", subClass);
		if (displayId != 0) context.table.addKey("display_id", displayId);
		if (quality != 0) context.table.addKey("quality", quality);
		if (flags != 0) context.table.addKey("flags", flags);
		if (buyCount != 0) context.table.addKey("buy_count", buyCount);
		if (buyPrice != 0) context.table.addKey("buy_price", buyPrice);
		if (sellPrice != 0) context.table.addKey("sell_price", sellPrice);
		if (inventoryType != 0) context.table.addKey("inv_type", inventoryType);
		if (allowedClasses != -1) context.table.addKey("allowed_class", allowedClasses);
		if (allowedRaces != -1) context.table.addKey("allowed_race", allowedRaces);
		if (itemLevel != 0) context.table.addKey("level", itemLevel);
		if (requiredLevel != 0) context.table.addKey("req_level", requiredLevel);
		if (requiredSkill != 0) context.table.addKey("req_skill", requiredSkill);
		if (requiredSkillRank != 0) context.table.addKey("req_skill_rank", requiredSkillRank);
		if (requiredSpell != 0) context.table.addKey("req_spell", requiredSpell);
		if (requiredHonorRank != 0) context.table.addKey("req_rank", requiredHonorRank);
		if (requiredCityRank != 0) context.table.addKey("req_city_rank", requiredCityRank);
		if (requiredReputation != 0) context.table.addKey("req_rep_faction", requiredReputation);
		if (requiredReputationRank != 0) context.table.addKey("req_rep_rank", requiredReputationRank);
		if (maxCount != 0) context.table.addKey("max_count", maxCount);
		if (maxStack != 0) context.table.addKey("max_stack", maxStack);
		if (containerSlots != 0) context.table.addKey("container_slots", containerSlots);
		if (armor != 0) context.table.addKey("armor", armor);
		if (holyResistance != 0) context.table.addKey("holy_res", holyResistance);
		if (fireResistance != 0) context.table.addKey("fire_res", fireResistance);
		if (natureResistance != 0) context.table.addKey("nature_res", natureResistance);
		if (frostResistance != 0) context.table.addKey("frost_res", frostResistance);
		if (shadowResistance != 0) context.table.addKey("shadow_res", shadowResistance);
		if (arcaneResistance != 0) context.table.addKey("arcane_res", arcaneResistance);
		if (delay != 0) context.table.addKey("delay", delay);
		if (ammoType != 0) context.table.addKey("ammo_type", ammoType);
		if (bonding != 0) context.table.addKey("bonding", static_cast<UInt16>(bonding));
		if (lockId != 0) context.table.addKey("lock_id", lockId);
		if (sheath != 0) context.table.addKey("sheath", static_cast<UInt16>(sheath));
		if (randomProperty != 0) context.table.addKey("random_property", randomProperty);
		if (randomSuffix != 0) context.table.addKey("random_suffix", randomSuffix);
		if (block != 0) context.table.addKey("block", block);
		if (set != 0) context.table.addKey("set", set);
		if (durability != 0) context.table.addKey("durability", durability);
		if (area != 0) context.table.addKey("area", area);
		if (map != 0) context.table.addKey("map", map);
		if (bagFamily != 0) context.table.addKey("bag_family", bagFamily);
		if (material != 0) context.table.addKey("material", material);
		if (totemCategory != 0) context.table.addKey("totem_category", static_cast<Int16>(totemCategory));
		if (socketBonus != 0) context.table.addKey("socket_bonus", socketBonus);
		if (gemProperties != 0) context.table.addKey("gem_properties", gemProperties);
		if (requiredDisenchantSkill != 0) context.table.addKey("req_disenchant_skill", requiredDisenchantSkill);
		if (disenchantId != 0) context.table.addKey("disenchant_id", disenchantId);
		if (foodType != 0) context.table.addKey("food_type", static_cast<UInt16>(foodType));
		if (minLootGold != 0) context.table.addKey("min_loot_gold", minLootGold);
		if (maxLootGold != 0) context.table.addKey("max_loot_gold", maxLootGold);
		if (duration != 0) context.table.addKey("duration", duration);
		if (extraFlags != 0) context.table.addKey("extra_flags", extraFlags);

		// Write stats
		sff::write::Array<char> statArray(context.table, "stats", sff::write::Comma);
		{
			for (const auto &stat : itemStats)
			{
				if (stat.statType == 0 && stat.statValue == 0) continue;

				// New stat table
				sff::write::Table<char> valueTable(statArray, sff::write::Comma);
				{
					valueTable.addKey("type", static_cast<UInt16>(stat.statType));
					valueTable.addKey("value", stat.statValue);
				}
				valueTable.finish();
			}
		}
		statArray.finish();

		// Write damage
		sff::write::Array<char> dmgArray(context.table, "dmg", sff::write::Comma);
		{
			for (const auto &dmg : itemDamage)
			{
				if (dmg.min == 0.0f && dmg.max == 0.0f && dmg.type == 0) continue;

				// New stat table
				sff::write::Table<char> valueTable(dmgArray, sff::write::Comma);
				{
					valueTable.addKey("min", dmg.min);
					valueTable.addKey("max", dmg.max);
					valueTable.addKey("type", static_cast<UInt16>(dmg.type));
				}
				valueTable.finish();
			}
		}
		dmgArray.finish();

		// Write spells
		sff::write::Array<char> spellArray(context.table, "spells", sff::write::MultiLine);
		{
			for (const auto &spell : itemSpells)
			{
				// Empty spell entry
				if (!spell.spell) continue;

				// New stat table
				sff::write::Table<char> valueTable(spellArray, sff::write::Comma);
				{
					valueTable.addKey("id", spell.spell->id);
					if (spell.trigger != 0) valueTable.addKey("trigger", static_cast<UInt16>(spell.trigger));
					if (spell.charges != 0) valueTable.addKey("charges", spell.charges);
					if (spell.procRate != 0.0f) valueTable.addKey("proc_rate", spell.procRate);
					if (spell.cooldown != 0) valueTable.addKey("cooldown", spell.cooldown);
					if (spell.category != 0) valueTable.addKey("category", spell.category);
					if (spell.categoryCooldown != 0) valueTable.addKey("category_cooldown", spell.categoryCooldown);
				}
				valueTable.finish();
			}
		}
		spellArray.finish();
	}
}
