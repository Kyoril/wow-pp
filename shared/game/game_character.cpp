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

#include "game_character.h"
#include <log/default_log_levels.h>
#include "data/skill_entry.h"
#include "data/item_entry.h"

namespace wowpp
{
	GameCharacter::GameCharacter(
		TimerQueue &timers,
		DataLoadContext::GetRace getRace,
		DataLoadContext::GetClass getClass,
		DataLoadContext::GetLevel getLevel)
		: GameUnit(timers, getRace, getClass, getLevel)
		, m_name("UNKNOWN")
		, m_zoneIndex(0)
		, m_weaponProficiency(0)
		, m_armorProficiency(0)
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

	void GameCharacter::addItem(std::unique_ptr<GameItem> item, UInt16 slot)
	{
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
	}

	void GameCharacter::addSpell(const SpellEntry &spell)
	{
		if (hasSpell(spell.id))
		{
			return;
		}

		m_spells.push_back(&spell);

		// Add dependant skills
		for (const auto *skill : spell.skillsOnLearnSpell)
		{
			addSkill(*skill);
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

	float GameCharacter::getManaBonusFromIntellect() const
	{
		float intellect = float(getUInt32Value(unit_fields::Stat3));
		
		float base = intellect < 20.0f ? intellect : 20.0f;
		float bonus = intellect - base;

		return base + (bonus * 15.0f);
	}

	float GameCharacter::getHealthBonusFromStamina() const
	{
		float stamina = float(getUInt32Value(unit_fields::Stat2));

		float base = stamina < 20.0f ? stamina : 20.0f;
		float bonus = stamina - base;

		return base + (bonus * 10.0f);
	}

	void GameCharacter::updateMaxPower(PowerType power)
	{
		UInt32 createPower = 0;
		switch (power)
		{
			case power_type::Mana:		createPower = getUInt32Value(unit_fields::BaseMana); break;
			case power_type::Rage:		createPower = 1000; break;
			case power_type::Energy:    createPower = 100; break;
		}

		float powerBonus = (power == power_type::Mana && createPower > 0) ? getManaBonusFromIntellect() : 0;

		float value = createPower;
		value += powerBonus;

		setUInt32Value(unit_fields::MaxPower1 + static_cast<UInt16>(power), UInt32(value));
	}

	void GameCharacter::updateMaxHealth()
	{
		float value = getUInt32Value(unit_fields::BaseHealth);
		value += getHealthBonusFromStamina();

		setUInt32Value(unit_fields::MaxHealth, UInt32(value));
	}

	void GameCharacter::updateAllStats()
	{
		updateMaxHealth();
		for (size_t i = 0; i < power_type::Happiness; ++i)
		{
			updateMaxPower(static_cast<PowerType>(i));
		}
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

	io::Writer & operator<<(io::Writer &w, GameCharacter const& object)
	{
		return w
			<< reinterpret_cast<GameUnit const&>(object)
			<< io::write_dynamic_range<NetUInt8>(object.m_name)
			<< io::write<NetUInt32>(object.m_zoneIndex);
	}

	io::Reader & operator>>(io::Reader &r, GameCharacter& object)
	{
		return r
			>> reinterpret_cast<GameUnit&>(object)
			>> io::read_container<NetUInt8>(object.m_name)
			>> io::read<NetUInt32>(object.m_zoneIndex);
	}
}
