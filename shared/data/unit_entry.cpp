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

#include "unit_entry.h"
#include "templates/basic_template_load_context.h"
#include "templates/basic_template_save_context.h"
#include "data_load_context.h"
#include "trigger_entry.h"
#include "item_entry.h"
#include "loot_entry.h"
#include "faction_template_entry.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	UnitEntry::UnitEntry()
		: minLevel(1)
		, maxLevel(1)
		, maleModel(0)
		, femaleModel(0)
		, minLevelHealth(0)
		, maxLevelHealth(0)
		, minLevelMana(0)
		, maxLevelMana(0)
		, minMeleeDamage(0)
		, maxMeleeDamage(0)
		, minRangedDamage(0)
		, maxRangedDamage(0)
		, scale(1.0f)
		, allianceFaction(nullptr)
		, hordeFaction(nullptr)
		, type(0)
		, family(0)
		, regeneratesHealth(true)
		, npcFlags(0)
		, unitFlags(0)
		, dynamicFlags(0)
		, extraFlags(0)
		, creatureTypeFlags(0)
		, walkSpeed(1.0f)
		, runSpeed(1.0f)
		, unitClass(1)
		, rank(0)
		, armor(0)
		, meleeBaseAttackTime(0)
		, rangedBaseAttackTime(0)
		, damageSchool(0)
		, minLootGold(0)
		, maxLootGold(0)
		, xpMin(0)
		, xpMax(0)
		, mainHand(nullptr)
		, offHand(nullptr)
		, ranged(nullptr)
		, attackPower(0)
		, rangedAttackPower(0)
		, unitLootEntry(nullptr)
	{
		resistances.fill(0);
	}

	bool UnitEntry::load(DataLoadContext &context, const ReadTableWrapper &wrapper)
	{
		if (!Super::loadBase(context, wrapper))
		{
			return false;
		}

		triggers.clear();
		triggersByEvent.clear();

		const sff::read::tree::Array<DataFileIterator> *triggerArray = wrapper.table.getArray("triggers");
		if (triggerArray)
		{
			for (size_t j = 0, d = triggerArray->getSize(); j < d; ++j)
			{
				UInt32 triggerId = triggerArray->getInteger(j, 0);
				if (triggerId == 0)
				{
					context.onWarning("Invalid trigger entry");
					continue;
				}

				const auto *trigger = context.getTrigger(triggerId);
				if (!trigger)
				{
					context.onWarning("Could not find trigger - skipping");
					continue;
				}

				triggers.push_back(trigger);
				for (auto &e : trigger->events)
				{
					triggersByEvent[e].push_back(trigger);
				}
			}
		}

#define MIN_MAX_CHECK(minval, maxval) if (maxval < minval) maxval = minval; else if(minval > maxval) minval = maxval;

		wrapper.table.tryGetString("name", name);
		wrapper.table.tryGetString("subname", subname);
		wrapper.table.tryGetInteger("min_level", minLevel);
		wrapper.table.tryGetInteger("max_level", maxLevel);
		MIN_MAX_CHECK(minLevel, maxLevel);
		wrapper.table.tryGetInteger("model_id_1", maleModel);
		wrapper.table.tryGetInteger("model_id_2", femaleModel);
		wrapper.table.tryGetInteger("min_level_health", minLevelHealth);
		wrapper.table.tryGetInteger("max_level_health", maxLevelHealth);
		MIN_MAX_CHECK(minLevelHealth, maxLevelHealth);
		wrapper.table.tryGetInteger("min_level_mana", minLevelMana);
		wrapper.table.tryGetInteger("max_level_mana", maxLevelMana);
		MIN_MAX_CHECK(minLevelMana, maxLevelMana);
		wrapper.table.tryGetInteger("min_melee_dmg", minMeleeDamage);
		wrapper.table.tryGetInteger("max_melee_dmg", maxMeleeDamage);
		MIN_MAX_CHECK(minMeleeDamage, maxMeleeDamage);
		wrapper.table.tryGetInteger("min_ranged_dmg", minRangedDamage);
		wrapper.table.tryGetInteger("max_ranged_dmg", maxRangedDamage);
		MIN_MAX_CHECK(minRangedDamage, maxRangedDamage);
		wrapper.table.tryGetInteger("scale", scale);
		wrapper.table.tryGetInteger("rank", rank);
		wrapper.table.tryGetInteger("armor", armor);
		wrapper.table.tryGetInteger("melee_attack_time", meleeBaseAttackTime);
		wrapper.table.tryGetInteger("ranged_attack_time", rangedBaseAttackTime);
		UInt32 allianceFactionID = 0;
		wrapper.table.tryGetInteger("a_faction", allianceFactionID);
		allianceFaction = context.getFactionTemplate(allianceFactionID);
		if (!allianceFaction)
		{
			std::ostringstream strm;
			strm << "Invalid alliance faction id " << allianceFactionID << " for unit " << id;
			context.onError(strm.str());
			return false;
		}
		UInt32 hordeFactionID = 0;
		wrapper.table.tryGetInteger("h_faction", hordeFactionID);
		hordeFaction = context.getFactionTemplate(hordeFactionID);
		if (!hordeFaction)
		{
			std::ostringstream strm;
			strm << "Invalid alliance faction id " << hordeFactionID << " for unit " << id;
			context.onError(strm.str());
			return false;
		}
		wrapper.table.tryGetInteger("unit_flags", unitFlags);
		wrapper.table.tryGetInteger("npc_flags", npcFlags);
		wrapper.table.tryGetInteger("unit_class", unitClass);
		wrapper.table.tryGetInteger("min_loot_gold", minLootGold);
		wrapper.table.tryGetInteger("max_loot_gold", maxLootGold);
		MIN_MAX_CHECK(minLootGold, maxLootGold);
		wrapper.table.tryGetInteger("type", type);
		wrapper.table.tryGetInteger("family", family);
		wrapper.table.tryGetInteger("min_xp", xpMin);
		wrapper.table.tryGetInteger("max_xp", xpMax);
		MIN_MAX_CHECK(xpMin, xpMax);
		UInt32 eqMain = 0, eqOff = 0, eqRange = 0;
		wrapper.table.tryGetInteger("eq_main_hand", eqMain);
		wrapper.table.tryGetInteger("eq_off_hand", eqOff);
		wrapper.table.tryGetInteger("eq_ranged", eqRange);
		if (eqMain != 0 || eqOff != 0 || eqRange != 0)
		{
			context.loadLater.push_back([eqMain, eqOff, eqRange, &context, this]() -> bool
			{
				if (eqMain != 0) this->mainHand = context.getItem(eqMain);
				if (eqOff != 0) this->offHand = context.getItem(eqOff);
				if (eqRange != 0) this->ranged = context.getItem(eqRange);

				return true;
			});
		}
		wrapper.table.tryGetInteger("atk_power", attackPower);
		wrapper.table.tryGetInteger("rng_atk_power", rangedAttackPower);
		UInt32 lootId = 0;
		wrapper.table.tryGetInteger("unit_loot", lootId);
		if (lootId != 0)
		{
			context.loadLater.push_back([lootId, &context, this]() -> bool
			{
				unitLootEntry = context.getUnitLoot(lootId);
				if (unitLootEntry == nullptr)
				{
					WLOG("Unit " << id << " has unknown unit loot entry " << lootId << " - creature will have no unit loot!");
				}
				return true;
			});
		}
#undef MIN_MAX_CHECK

		return true;
	}

	void UnitEntry::save(BasicTemplateSaveContext &context) const
	{
		Super::saveBase(context);

		if (!name.empty()) context.table.addKey("name", name);
		if (!subname.empty()) context.table.addKey("subname", subname);
		if (minLevel > 1) context.table.addKey("min_level", minLevel);
		if (maxLevel != minLevel) context.table.addKey("max_level", maxLevel);
		if (maleModel != 0) context.table.addKey("model_id_1", maleModel);
		if (femaleModel != 0) context.table.addKey("model_id_2", femaleModel);
		if (minLevelHealth != 0) context.table.addKey("min_level_health", minLevelHealth);
		if (maxLevelHealth != 0) context.table.addKey("max_level_health", maxLevelHealth);
		if (minLevelMana != 0) context.table.addKey("min_level_mana", minLevelMana);
		if (maxLevelMana != 0) context.table.addKey("max_level_mana", maxLevelMana);
		if (minMeleeDamage != 0) context.table.addKey("min_melee_dmg", minMeleeDamage);
		if (maxMeleeDamage != 0) context.table.addKey("max_melee_dmg", maxMeleeDamage);
		if (minRangedDamage != 0) context.table.addKey("min_ranged_dmg", minRangedDamage);
		if (maxRangedDamage != 0) context.table.addKey("max_ranged_dmg", maxRangedDamage);
		if (scale != 1.0f) context.table.addKey("scale", scale);
		if (rank != 0) context.table.addKey("rank", rank);
		if (armor != 0) context.table.addKey("armor", armor);
		if (meleeBaseAttackTime != 0) context.table.addKey("melee_attack_time", meleeBaseAttackTime);
		if (rangedBaseAttackTime != 0) context.table.addKey("ranged_attack_time", rangedBaseAttackTime);
		if (allianceFaction != 0) context.table.addKey("a_faction", allianceFaction->id);
		if (hordeFaction != 0) context.table.addKey("h_faction", hordeFaction->id);
		if (unitFlags != 0) context.table.addKey("unit_flags", unitFlags);
		if (npcFlags != 0) context.table.addKey("npc_flags", npcFlags);
		if (unitClass != 0) context.table.addKey("unit_class", unitClass);
		if (minLootGold != 0) context.table.addKey("min_loot_gold", minLootGold);
		if (maxLootGold != 0) context.table.addKey("max_loot_gold", maxLootGold);
		if (type != 0) context.table.addKey("type", type);
		if (family != 0) context.table.addKey("family", family);
		if (xpMin != 0) context.table.addKey("min_xp", xpMin);
		if (xpMax != 0) context.table.addKey("max_xp", xpMax);
		if (mainHand != 0) context.table.addKey("eq_main_hand", mainHand->id);
		if (offHand != 0) context.table.addKey("eq_off_hand", offHand->id);
		if (ranged != 0) context.table.addKey("eq_ranged", ranged->id);
		if (attackPower != 0) context.table.addKey("atk_power", attackPower);
		if (rangedAttackPower != 0) context.table.addKey("rng_atk_power", rangedAttackPower);
		if (unitLootEntry != nullptr) context.table.addKey("unit_loot", unitLootEntry->id);

		if (!triggers.empty())
		{
			sff::write::Array<char> triggerArray(context.table, "triggers", sff::write::Comma);
			{
				for (const auto *t : triggers)
				{
					triggerArray.addElement(t->id);
				}
			}
			triggerArray.finish();
		}
	}
}
