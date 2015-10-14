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

#include "game_character.h"
#include <log/default_log_levels.h>
#include "data/skill_entry.h"
#include "data/item_entry.h"
#include "game_item.h"

namespace wowpp
{
	GameCharacter::GameCharacter(
		TimerQueue &timers,
		DataLoadContext::GetRace getRace,
		DataLoadContext::GetClass getClass,
		DataLoadContext::GetLevel getLevel,
		DataLoadContext::GetSpell getSpell)
		: GameUnit(timers, getRace, getClass, getLevel, getSpell)
		, m_name("UNKNOWN")
		, m_zoneIndex(0)
		, m_weaponProficiency(0)
		, m_armorProficiency(0)
		, m_comboTarget(0)
		, m_comboPoints(0)
		, m_healthRegBase(0.0f)
		, m_manaRegBase(0.0f)
		, m_groupUpdateFlags(group_update_flags::None)
		, m_canBlock(false)
		, m_canParry(false)
	{
		// Resize values field
		m_values.resize(character_fields::CharacterFieldCount);
		m_valueBitset.resize((character_fields::CharacterFieldCount + 31) / 32);

		m_objectType |= type_mask::Player;
	}

	GameCharacter::~GameCharacter()
	{
	}

	void GameCharacter::initialize()
	{
		GameUnit::initialize();

		setUInt32Value(object_fields::Type, 25);					//OBJECT_FIELD_TYPE				(TODO: Flags)
		setUInt32Value(character_fields::CharacterFlags, 0x50000);

		setFloatValue(unit_fields::BoundingRadius, 0.388999998569489f);
		setFloatValue(unit_fields::CombatReach, 1.5f);
		setByteValue(unit_fields::Bytes2, 1, 0x08 | 0x20);
		setUInt32Value(unit_fields::Bytes2, 0);
		setUInt32Value(unit_fields::UnitFlags, 0x08);
		setFloatValue(unit_fields::ModCastSpeed, 1.0f);

		// -1
		setInt32Value(character_fields::WatchedFactionIndex, -1);
		setUInt32Value(character_fields::CharacterPoints_2, 2);
		setUInt32Value(character_fields::ModHealingDonePos, 0);
		for (UInt8 i = 0; i < 7; ++i)
		{
			setUInt32Value(character_fields::ModDamageDoneNeg + i, 0);
			setUInt32Value(character_fields::ModDamageDonePos + i, 0);
			setFloatValue(character_fields::ModDamageDonePct + i, 1.00f);
		}

		//reset attack power, damage and attack speed fields
		setUInt32Value(unit_fields::BaseAttackTime, 2000);
		setUInt32Value(unit_fields::BaseAttackTime + 1, 2000); // offhand attack time
		setUInt32Value(unit_fields::RangedAttackTime, 2000);

		setFloatValue(unit_fields::MinDamage, 0.0f);
		setFloatValue(unit_fields::MaxDamage, 0.0f);
		setFloatValue(unit_fields::MinOffHandDamage, 0.0f);
		setFloatValue(unit_fields::MaxOffHandDamage, 0.0f);
		setFloatValue(unit_fields::MinRangedDamage, 0.0f);
		setFloatValue(unit_fields::MaxRangedDamage, 0.0f);

		setInt32Value(unit_fields::AttackPower, 0);
		setInt32Value(unit_fields::AttackPowerMods, 0);
		setFloatValue(unit_fields::AttackPowerMultiplier, 0.0f);
		setInt32Value(unit_fields::RangedAttackPower, 0);
		setInt32Value(unit_fields::RangedAttackPowerMods, 0);
		setFloatValue(unit_fields::RangedAttackPowerMultiplier, 0.0f);

		// Base crit values (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
		setFloatValue(character_fields::CritPercentage, 0.0f);
		setFloatValue(character_fields::OffHandCritPercentage, 0.0f);
		setFloatValue(character_fields::RangedCritPercentage, 0.0f);

		// Init spell schools (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
		for (UInt8 i = 0; i < 7; ++i)
			setFloatValue(character_fields::SpellCritPercentage + i, 0.0f);

		setFloatValue(character_fields::ParryPercentage, 0.0f);
		setFloatValue(character_fields::BlockPercentage, 0.0f);
		setUInt32Value(character_fields::ShieldBlock, 0);

		// Dodge percentage
		setFloatValue(character_fields::DodgePercentage, 0.0f);
	}

	void GameCharacter::levelChanged(const LevelEntry &levelInfo)
	{
		// Superclass
		GameUnit::levelChanged(levelInfo);

		// One talent point per level
		updateTalentPoints();

		// Update xp to next level
		setUInt32Value(character_fields::NextLevelXp, levelInfo.nextLevelXP);
		
		// Try to find base values
		if (getClassEntry())
		{
			auto &levelBaseValues = getClassEntry()->levelBaseValues;
			auto it = levelBaseValues.find(levelInfo.id);
			if (it != levelBaseValues.end())
			{
				// Update base health and mana
				setUInt32Value(unit_fields::BaseHealth, it->second.health);
				setUInt32Value(unit_fields::BaseMana, it->second.mana);
			}
			else
			{
				DLOG("Couldn't find level entry for level " << levelInfo.id);
			}
		}
		else
		{
			DLOG("Couldn't find class entry!");
		}

		// Update mana regeneration per spirit
		auto regenIt = levelInfo.regen.find(getClass());
		if (regenIt != levelInfo.regen.end())
		{
			m_healthRegBase = regenIt->second[0];
			m_manaRegBase = regenIt->second[1];
		}
		else
		{
			m_healthRegBase = 0.0f;
			m_manaRegBase = 0.0f;
		}

		// Update all stats
		updateAllStats();

		// Maximize health
		setUInt32Value(unit_fields::Health, getUInt32Value(unit_fields::MaxHealth));

		// Maximize mana and energy
		setUInt32Value(unit_fields::Power1, getUInt32Value(unit_fields::MaxPower1));
		setUInt32Value(unit_fields::Power3, getUInt32Value(unit_fields::MaxPower3));
	}

	void GameCharacter::setName(const String &name)
	{
		m_name = name;
	}

	void GameCharacter::addItem(std::shared_ptr<GameItem> item, UInt16 slot)
	{
		if (slot < player_equipment_slots::End)
		{
			if (item->getEntry().durability == 0 ||
				item->getUInt32Value(item_fields::Durability) > 0)
			{
				// Apply values
				for (const auto &entry : item->getEntry().itemStats)
				{
					if (entry.statValue != 0)
					{
						switch (entry.statType)
						{
						case 0:		// Mana
							updateModifierValue(unit_mods::Mana, unit_mod_type::TotalValue, entry.statValue, true);
							break;
						case 1:		// Health
							updateModifierValue(unit_mods::Health, unit_mod_type::TotalValue, entry.statValue, true);
							break;
						case 3:		// Agility
							updateModifierValue(unit_mods::StatAgility, unit_mod_type::TotalValue, entry.statValue, true);
							break;
						case 4:		// Strength
							updateModifierValue(unit_mods::StatStrength, unit_mod_type::TotalValue, entry.statValue, true);
							break;
						case 5:		// Intellect
							updateModifierValue(unit_mods::StatIntellect, unit_mod_type::TotalValue, entry.statValue, true);
							break;
						case 6:		// Spirit
							updateModifierValue(unit_mods::StatSpirit, unit_mod_type::TotalValue, entry.statValue, true);
							break;
						case 7:		// Stamina
							updateModifierValue(unit_mods::StatStamina, unit_mod_type::TotalValue, entry.statValue, true);
							break;
						default:
							break;
						}
					}
				}
			}
		}

		// Check if that item already exists
		auto it = m_itemSlots.find(slot);
		if (it == m_itemSlots.end())
		{
			// Set new item guid
			setUInt64Value(character_fields::InvSlotHead + (slot * 2), item->getGuid());
			item->setUInt64Value(item_fields::Contained, getGuid());
			item->setUInt64Value(item_fields::Owner, getGuid());
			
			// If this item was equipped, make it visible for other players
			if (slot < player_equipment_slots::End)
			{
				setUInt32Value(character_fields::VisibleItem1_0 + (slot * 16), item->getEntry().id);
				setUInt64Value(character_fields::VisibleItem1_CREATOR + (slot * 16), item->getUInt64Value(item_fields::Creator));

				// TODO: Apply Enchantment Slots

				// TODO: Apply random property settings

			}

			// Store new item
			m_itemSlots[slot] = std::move(item);
		}
		else
		{
			// Item already exists, increase the stack number, old item will be deleted
			auto &playerItem = it->second;

			// Increase stack
			const UInt32 existingStackCount = playerItem->getUInt32Value(item_fields::StackCount);
			const UInt32 additionalStackCount = item->getUInt32Value(item_fields::StackCount);

			// Apply stack count
			playerItem->setUInt32Value(item_fields::StackCount, existingStackCount + additionalStackCount);
		}

		// Equipment changed
		if (slot < player_equipment_slots::End)
		{
			updateAllStats();
		}
	}

	void GameCharacter::addSpell(const SpellEntry &spell)
	{
		if (hasSpell(spell.id))
		{
			return;
		}

		// Evaluate parry and block spells
		for (auto &effect : spell.effects)
		{
			if (effect.type == game::spell_effects::Parry)
			{
				m_canParry = true;
			}
			else if (effect.type == game::spell_effects::Block)
			{
				m_canBlock = true;
			}
		}

		m_spells.push_back(&spell);
		
		// Add dependant skills
		for (const auto *skill : spell.skillsOnLearnSpell)
		{
			addSkill(*skill);
		}

		// Talent point update
		if (spell.talentCost > 0)
		{
			updateTalentPoints();
		}
	}

	bool GameCharacter::hasSpell(UInt32 spellId) const
	{
		for (const auto *spell : m_spells)
		{
			if (spell->id == spellId)
			{
				return true;
			}
		}

		return false;
	}

	void GameCharacter::addSkill(const SkillEntry &skill)
	{
		if (hasSkill(skill.id))
		{
			return;
		}

		UInt16 current = 1;
		UInt16 max = 1;

		switch (skill.category)
		{
			case game::skill_category::Languages:
			{
				current = 300;
				max = 300;
				break;
			}

			case game::skill_category::Armor:
			{
				current = 1;
				max = 1;
				break;
			}

			case game::skill_category::Weapon:
			case game::skill_category::Class:
			{
				max = getLevel() * 5;
				break;
			}

			case game::skill_category::Secondary:
			case game::skill_category::Profession:
			{
				current = 1;
				max = 1;
				break;
			}

			case game::skill_category::NotDisplayed:
			case game::skill_category::Attributes:
			{
				// Invisible / Not added
				return;
			}

			default:
			{
				WLOG("Unsupported skill category: " << skill.category);
				return;
			}
		}

		// Find the next usable skill slot
		const UInt32 maxSkills = 127;
		for (UInt32 i = 0; i < maxSkills; ++i)
		{
			// Unit field values
			const UInt32 skillIndex = character_fields::SkillInfo1_1 + (i * 3);

			// Get current skill
			UInt32 skillValue = getUInt32Value(skillIndex);
			if (skillValue == 0 || skillValue == skill.id)
			{
				// Update values
				const UInt32 minMaxValue = UInt32(UInt16(current) | (UInt32(max) << 16));
				setUInt32Value(skillIndex, skill.id);
				setUInt32Value(skillIndex + 1, minMaxValue);
				m_skills.push_back(&skill);

				// TODO: Learn dependant spells if any
				return;
			}
		}

		ELOG("Maximum number of skill for character reached!");
	}

	bool GameCharacter::hasSkill(UInt32 skillId) const
	{
		for (const auto *skill : m_skills)
		{
			if (skill->id == skillId)
			{
				return true;
			}
		}

		return false;
	}

	void GameCharacter::removeSkill(UInt32 skillId)
	{
		// Find the next usable skill slot
		const UInt32 maxSkills = 127;
		for (UInt32 i = 0; i < maxSkills; ++i)
		{
			// Unit field values
			const UInt32 skillIndex = character_fields::SkillInfo1_1 + (i * 3);

			// Get current skill
			if (getUInt32Value(skillIndex) == skillId)
			{
				setUInt32Value(skillIndex, 0);
				setUInt32Value(skillIndex + 1, 0);
				setUInt32Value(skillIndex + 2, 0);
			}
		}

		// Remove skill from the list
		auto it = m_skills.begin();
		while (it != m_skills.end())
		{
			if ((*it)->id == skillId)
			{
				it = m_skills.erase(it);
				break;
			}

			++it;
		}
	}

	void GameCharacter::setSkillValue(UInt32 skillId, UInt16 current, UInt16 maximum)
	{
		// Find the next usable skill slot
		const UInt32 maxSkills = 127;
		for (UInt32 i = 0; i < maxSkills; ++i)
		{
			// Unit field values
			const UInt32 skillIndex = character_fields::SkillInfo1_1 + (i * 3);

			// Get current skill
			if (getUInt32Value(skillIndex) == skillId)
			{
				const UInt32 minMaxValue = UInt32(UInt16(current) | (UInt32(maximum) << 16));
				setUInt32Value(skillIndex + 1, minMaxValue);
				break;
			}
		}
	}

	bool GameCharacter::isValidItemPos(UInt8 bag, UInt8 slot) const
	{
		// Has a bag been specified?
		if (bag == 0)
			return true;

		// Any bag
		if (bag == 255)
		{
			if (slot == 255)
				return true;

			// equipment
			if (slot < player_equipment_slots::End)
				return true;

			// bag equip slots
			if (slot >= player_inventory_slots::Start && slot < player_inventory_slots::End)
				return true;

			// backpack slots
			if (slot >= player_item_slots::Start && slot < player_item_slots::End)
				return true;

			// keyring slots
			if (slot >= player_key_ring_slots::Start && slot < player_key_ring_slots::End)
				return true;

			// bank main slots
			if (slot >= player_bank_item_slots::Start && slot < player_bank_item_slots::End)
				return true;

			// bank bag slots
			if (slot >= player_bank_bag_slots::Start && slot < player_bank_bag_slots::End)
				return true;

			// Invalid
			return false;
		}

		// bag content slots
		if (bag >= player_inventory_slots::Start && bag < player_inventory_slots::End)
		{
			// TODO: Get bag at the specified position

			// Any slot
			if (slot == 255)
				return true;

			//return slot < pBag->GetBagSize();
			return false;
		}

		// bank bag content slots
		if (bag >= player_bank_bag_slots::Start && bag < player_bank_bag_slots::End)
		{
			// TODO: Get bag at the specified position

			// Any slot
			if (slot == 255)
				return true;

			//return slot < pBag->GetBagSize();
			return false;
		}

		return false;
	}

	void GameCharacter::swapItem(UInt16 src, UInt16 dst)
	{
		UInt8 srcBag = src >> 8;
		UInt8 srcSlot = src & 0xFF;

		UInt8 dstBag = dst >> 8;
		UInt8 dstSlot = dst & 0xFF;

		GameItem *srcItem = getItemByPos(srcBag, srcSlot);
		GameItem *dstItem = getItemByPos(dstBag, dstSlot);

		// Check if we have a valid source item
		if (!srcItem)
		{
			inventoryChangeFailure(game::inventory_change_failure::ItemNotFound, srcItem, dstItem);
			return;
		}

		// Check if we are alive
		if (getUInt32Value(unit_fields::Health) == 0)
		{
			inventoryChangeFailure(game::inventory_change_failure::YouAreDead, srcItem, dstItem);
			return;
		}

		// TODO: Check if source item is in still consisted as loot

		// TODO: Validate source item

		// TODO: Validate dest item

		// Detect case
		if (!dstItem)
		{
			DLOG("TODO: Moving...");

			// Move items (little test)
			setUInt64Value(character_fields::InvSlotHead + (srcSlot * 2), 0);
			setUInt64Value(character_fields::InvSlotHead + (dstSlot * 2), srcItem->getGuid());
		}
		else
		{
			DLOG("TODO: Swapping...");


		}
	}

	GameItem * GameCharacter::getItemByPos(UInt8 bag, UInt8 slot) const
	{
		if (bag == player_inventory_slots::Bag_0)
		{
			if (slot < player_bank_item_slots::End || (slot >= player_key_ring_slots::Start && slot < player_key_ring_slots::End))
			{
				auto it = m_itemSlots.find(slot);
				if (it != m_itemSlots.end())
				{
					return it->second.get();
				}
			}
		}
		else
		{
			// Is this a valid bag slot?
			const bool isBagSlot = (
				(bag >= player_inventory_slots::Start && bag < player_inventory_slots::End) ||
				(bag >= player_bank_bag_slots::Start && bag < player_bank_bag_slots::End)
				);

			if (isBagSlot)
			{
				// TODO: Get item from bag
			}
		}

		return nullptr;
	}

	void GameCharacter::updateArmor()
	{
		float baseArmor = getModifierValue(unit_mods::Armor, unit_mod_type::BaseValue);

		// Apply equipment
		for (UInt8 i = player_equipment_slots::Start; i < player_equipment_slots::End; ++i)
		{
			auto it = m_itemSlots.find(i);
			if (it != m_itemSlots.end())
			{
				// Add armor value from item
				baseArmor += it->second->getEntry().armor;
			}
		}

		const float basePct = getModifierValue(unit_mods::Armor, unit_mod_type::BasePct);
		const float totalArmor = getModifierValue(unit_mods::Armor, unit_mod_type::TotalValue);
		const float totalPct = getModifierValue(unit_mods::Armor, unit_mod_type::TotalPct);

		// Add armor from agility
		baseArmor += getUInt32Value(unit_fields::Stat1) * 2;
		float value = ((baseArmor * basePct) + totalArmor) * totalPct;

		setUInt32Value(unit_fields::Resistances, value);
		setUInt32Value(unit_fields::ResistancesBuffModsPositive, totalArmor);
	}

	void GameCharacter::updateDamage()
	{
		float atkPower = 0.0f;
		float level = float(getLevel());

		// Melee Attack Power
		{
			// Update attack power
			switch (getClass())
			{
			case game::char_class::Warrior:
				atkPower = level * 3.0f + getUInt32Value(unit_fields::Stat0) * 2.0f - 20.0f;
				break;
			case game::char_class::Paladin:
				atkPower = level * 3.0f + getUInt32Value(unit_fields::Stat0) * 2.0f - 20.0f;
				break;
			case game::char_class::Rogue:
				atkPower = level * 2.0f + getUInt32Value(unit_fields::Stat0) + getUInt32Value(unit_fields::Stat1) - 20.0f;
				break;
			case game::char_class::Hunter:
				atkPower = level * 2.0f + getUInt32Value(unit_fields::Stat0) + getUInt32Value(unit_fields::Stat1) - 20.0f;
				break;
			case game::char_class::Shaman:
				atkPower = level * 2.0f + getUInt32Value(unit_fields::Stat0) * 2.0f - 20.0f;
				break;
			case game::char_class::Druid:
				switch (getByteValue(unit_fields::Bytes2, 3))
				{
				case 1:		// Cat
					atkPower = level * 2.0f + getUInt32Value(unit_fields::Stat0) * 2.0f + getUInt32Value(unit_fields::Stat1) - 20.0f;
					break;
				case 5:
				case 8:
					atkPower = level * 3.0f + getUInt32Value(unit_fields::Stat0) * 2.0f - 20.0f;
					break;
				case 31:
					atkPower = level * 1.5f + getUInt32Value(unit_fields::Stat0) * 2.0f - 20.0f;
					break;
				default:
					atkPower = getUInt32Value(unit_fields::Stat0) * 2.0f - 20.0f;
					break;
				}
				break;
			case game::char_class::Mage:
				atkPower = getUInt32Value(unit_fields::Stat0) - 10.0f;
				break;
			case game::char_class::Priest:
				atkPower = getUInt32Value(unit_fields::Stat0) - 10.0f;
				break;
			case game::char_class::Warlock:
				atkPower = getUInt32Value(unit_fields::Stat0) - 10.0f;
				break;
			}

			setInt32Value(unit_fields::AttackPower, UInt32(atkPower));
		}
		
		// Ranged Attack power
		{
			switch (getClass())
			{
			case game::char_class::Hunter:
				atkPower = level * 2.0f + getUInt32Value(unit_fields::Stat1) - 10.0f;
				break;
			case game::char_class::Rogue:
				atkPower = level + getUInt32Value(unit_fields::Stat1) - 10.0f;
				break;
			case game::char_class::Warrior:
				atkPower = level + getUInt32Value(unit_fields::Stat1) - 10.0f;
				break;
			case game::char_class::Druid:
				// TODO shape shift
				atkPower = getUInt32Value(unit_fields::Stat1) - 10.0f;
				break;
			default:
				atkPower = getUInt32Value(unit_fields::Stat1) - 10.0f;
				break;
			}

			setInt32Value(unit_fields::RangedAttackPower, UInt32(atkPower));
		}

		float minDamage = 1.0f;
		float maxDamage = 2.0f;
		UInt32 attackTime = 2000;

		// TODO: Check druid form etc.

		UInt8 form = getByteValue(unit_fields::Bytes2, 3);
		if (form == 1 || form == 5 || form == 8)
		{
			attackTime = (form == 1 ? 1000 : 2500);
		}
		else
		{
			// Check if we are wearing a weapon in our main hand
			auto it = m_itemSlots.find(player_equipment_slots::Mainhand);
			if (it != m_itemSlots.end())
			{
				// Get weapon damage values
				const auto &entry = it->second->getEntry();
				if (entry.itemDamage[0].min != 0.0f) minDamage = entry.itemDamage[0].min;
				if (entry.itemDamage[0].max != 0.0f) maxDamage = entry.itemDamage[0].max;
				if (entry.delay != 0) attackTime = entry.delay;
			}
		}

		const float att_speed = attackTime / 1000.0f;
		const float base_value = getUInt32Value(unit_fields::AttackPower) / 14.0f * att_speed;

		switch (form)
		{
			case 1:
			case 5:
			case 8:
				minDamage = (level > 60 ? 60 : level) * 0.85f * att_speed;
				maxDamage = (level > 60 ? 60 : level) * 1.25f * att_speed;
				break;
			default:
				break;
		}

		setFloatValue(unit_fields::MinDamage, base_value + minDamage);
		setFloatValue(unit_fields::MaxDamage, base_value + maxDamage);
		setUInt32Value(unit_fields::BaseAttackTime, attackTime);
	}

	void GameCharacter::addComboPoints(UInt64 target, UInt8 points)
	{
		if (target == 0 || points == 0)
		{
			// Reset
			m_comboTarget = 0;
			m_comboPoints = 0;
		}
		else
		{
			if (target == m_comboTarget)
			{
				if (m_comboPoints < 5)
				{
					// Add new combo points
					m_comboPoints += points;
				}
				else
				{
					// Nothing to do here since combo points are already maxed out
					return;
				}
			}
			else
			{
				// Set new combo points
				m_comboTarget = target;
				m_comboPoints = std::min<UInt8>(5, points);
			}
		}

		// Notify about combo point change
		comboPointsChanged();
	}

	void GameCharacter::updateManaRegen()
	{
		const float intellect = getUInt32Value(unit_fields::Stat3);
		const float spirit = getUInt32Value(unit_fields::Stat4) * m_manaRegBase;
		const float regen = sqrtf(intellect) * spirit;

		float modManaRegenInterrupt = 0.0f;	//TODO
		setFloatValue(character_fields::ModManaRegenInterrupt, regen * modManaRegenInterrupt);
		setFloatValue(character_fields::ModManaRegen, regen);
	}

	void GameCharacter::rewardExperience(GameUnit *victim, UInt32 experience)
	{
		// Get current experience points
		UInt32 currentXP = getUInt32Value(character_fields::Xp);
		UInt32 nextLevel = getUInt32Value(character_fields::NextLevelXp);
		UInt32 level = getLevel();

		// Nothing to do here
		if (nextLevel == 0)
			return;

		UInt32 newXP = currentXP + experience;
		while (newXP >= nextLevel && nextLevel > 0)
		{
			// Calculate new XP amount
			newXP -= nextLevel;
			
			// Level up!
			setLevel(level + 1);
			nextLevel = getUInt32Value(character_fields::NextLevelXp);
		}

		// Update amount of XP
		experienceGained(victim ? victim->getGuid() : 0, experience, 0);
		setUInt32Value(character_fields::Xp, newXP);
	}

	void GameCharacter::modifyGroupUpdateFlags(GroupUpdateFlags flags, bool apply)
	{
		if (apply)
		{
			m_groupUpdateFlags = static_cast<GroupUpdateFlags>(m_groupUpdateFlags | flags);
		}
		else
		{
			m_groupUpdateFlags = static_cast<GroupUpdateFlags>(m_groupUpdateFlags & ~flags);
		}
	}

	bool GameCharacter::canBlock() const
	{
		if (!m_canBlock)
		{
			return false;
		}

		const UInt16 slot = static_cast<UInt16>(player_equipment_slots::Offhand);
		auto it = m_itemSlots.find(slot);
		if (it == m_itemSlots.end())
		{
			return false;
		}

		auto item = it->second;
		if (!item)
		{
			return false;
		}

		if (item->getEntry().inventoryType != inventory_type::Shield)
		{
			return false;
		}

		return true;
	}

	bool GameCharacter::canParry() const
	{
		if (!m_canParry)
		{
			return false;
		}

		const UInt16 slot = static_cast<UInt16>(player_equipment_slots::Mainhand);
		auto it = m_itemSlots.find(slot);
		return (it != m_itemSlots.end());
	}

	bool GameCharacter::canDodge() const
	{
		return true;
	}

	void GameCharacter::regenerateHealth()
	{
		float healthRegRate = 1.0f;
		if (getLevel() < 15)
		{
			healthRegRate *= (2.066f - (getLevel() * 0.066f));
		}

		// TODO: When polymorphed, regenerate health quickly

		// TODO: One missing value
		float spirit = static_cast<float>(getUInt32Value(unit_fields::Stat4));
		float baseSpirit = spirit;
		if (baseSpirit > 50.0f) baseSpirit = 50.0f;
		float moreSpirit = spirit - baseSpirit;
		float spiritRegen = baseSpirit * m_healthRegBase + moreSpirit * m_healthRegBase;

		float addHealth = spiritRegen * healthRegRate;
		UInt8 standState = getByteValue(unit_fields::Bytes1, 0);
		if (standState != unit_stand_state::Stand &&
			standState != unit_stand_state::Dead)
		{
			// 50% more regeneration when not standing
			addHealth *= 1.5f;
		}

		if (addHealth < 0.0f) 
			return;

		heal(static_cast<UInt32>(addHealth), nullptr, true);
	}

	void GameCharacter::updateTalentPoints()
	{
		auto level = getLevel();

		UInt32 talentPoints = 0;
		if (level >= 10)
		{
			// This is the maximum number of talent points available at the current character level
			talentPoints = level - 9;

			// Now iterate through every learned spell and reduce the amount of talent points
			for (auto &spell : m_spells)
			{
				talentPoints -= spell->talentCost;
			}
		}

		setUInt32Value(character_fields::CharacterPoints_1, talentPoints);
	}

	io::Writer & operator<<(io::Writer &w, GameCharacter const& object)
	{
		w
			<< reinterpret_cast<GameUnit const&>(object)
			<< io::write_dynamic_range<NetUInt8>(object.m_name)
			<< io::write<NetUInt32>(object.m_zoneIndex)
			<< io::write<float>(object.m_healthRegBase)
			<< io::write<float>(object.m_manaRegBase)
			<< io::write<NetUInt64>(object.m_groupId);
		return w;
	}

	io::Reader & operator>>(io::Reader &r, GameCharacter& object)
	{
		object.initialize();
		r
			>> reinterpret_cast<GameUnit&>(object)
			>> io::read_container<NetUInt8>(object.m_name)
			>> io::read<NetUInt32>(object.m_zoneIndex)
			>> io::read<float>(object.m_healthRegBase)
			>> io::read<float>(object.m_manaRegBase)
			>> io::read<NetUInt64>(object.m_groupId);

		// Reset all auras
		for (UInt32 i = 0; i < 56; ++i)
		{
			object.setUInt32Value(unit_fields::Aura + i, 0);
		}

		// Update all stats
		object.setLevel(object.getLevel());

		return r;
	}
}
