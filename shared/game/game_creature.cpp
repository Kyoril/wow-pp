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

#include "game_creature.h"

namespace wowpp
{
	GameCreature::GameCreature(TimerQueue &timers, DataLoadContext::GetRace getRace, DataLoadContext::GetClass getClass, DataLoadContext::GetLevel getLevel, const UnitEntry &entry)
		: GameUnit(timers, getRace, getClass, getLevel)
		, m_originalEntry(entry)
		, m_entry(nullptr)
	{
	}

	void GameCreature::initialize()
	{
		// Initialize the unit
		GameUnit::initialize();

		setEntry(m_originalEntry);
	}

	void GameCreature::setEntry(const UnitEntry &entry)
	{
		const bool isInitialize = (m_entry == nullptr);
		
		// Choose a level
		UInt8 creatureLevel = entry.minLevel;
		if (entry.maxLevel != entry.minLevel)
		{
			std::uniform_int_distribution<int> levelDistribution(entry.minLevel, entry.maxLevel);
			creatureLevel = levelDistribution(randomGenerator);
		}

		// calculate interpolation factor
		const float t =
			(entry.maxLevel != entry.minLevel) ?
			(creatureLevel - entry.minLevel) / (entry.maxLevel - entry.minLevel) :
			0.0f;

		// Randomize gender
		std::uniform_int_distribution<int> genderDistribution(0, 1);
		int gender = genderDistribution(randomGenerator);

		setLevel(creatureLevel);
		setClass(entry.unitClass);
		setUInt32Value(object_fields::Entry, entry.id);
		setFloatValue(object_fields::ScaleX, entry.scale);
		setUInt32Value(unit_fields::FactionTemplate, entry.hordeFactionID);
		setGender(static_cast<game::Gender>(gender));
		setUInt32Value(unit_fields::DisplayId, (gender == game::gender::Male ? entry.maleModel : entry.femaleModel));
		setUInt32Value(unit_fields::NativeDisplayId, (gender == game::gender::Male ? entry.maleModel : entry.femaleModel));
		setUInt32Value(unit_fields::BaseHealth, interpolate(entry.minLevelHealth, entry.maxLevelHealth, t));
		setUInt32Value(unit_fields::MaxHealth, getUInt32Value(unit_fields::BaseHealth));
		setUInt32Value(unit_fields::Health, getUInt32Value(unit_fields::BaseHealth));
		setUInt32Value(unit_fields::MaxPower1, interpolate(entry.minLevelMana, entry.maxLevelMana, t));
		setUInt32Value(unit_fields::Power1, getUInt32Value(unit_fields::MaxPower1));
		setUInt32Value(unit_fields::UnitFlags, entry.unitFlags);
		setUInt32Value(unit_fields::DynamicFlags, entry.dynamicFlags);
		setUInt32Value(unit_fields::NpcFlags, entry.npcFlags);
		setByteValue(unit_fields::Bytes2, 1, 16);
		setByteValue(unit_fields::Bytes2, 0, 1);		// Sheath State: Melee weapon
		setFloatValue(unit_fields::MinDamage, entry.minMeleeDamage);
		setFloatValue(unit_fields::MaxDamage, entry.maxMeleeDamage);
		setUInt32Value(unit_fields::BaseAttackTime, entry.meleeBaseAttackTime);

		// Unit Mods
		for (UInt32 i = unit_mods::StatStart; i < unit_mods::StatEnd; ++i)
		{
			setModifierValue(static_cast<UnitMods>(i), unit_mod_type::BaseValue, 0.0f);
			setModifierValue(static_cast<UnitMods>(i), unit_mod_type::TotalValue, 0.0f);
			setModifierValue(static_cast<UnitMods>(i), unit_mod_type::BasePct, 1.0f);
			setModifierValue(static_cast<UnitMods>(i), unit_mod_type::TotalPct, 1.0f);
		}
		setModifierValue(unit_mods::Armor, unit_mod_type::BaseValue, entry.armor);

		// Setup new entry
		m_entry = &entry;

		// Emit signal
		if (!isInitialize)
		{
			entryChanged();
		}

		updateAllStats();
	}

	void GameCreature::onKilled(GameUnit *killer)
	{
		GameUnit::onKilled(killer);

		if (killer)
		{
			// Reward the killer with experience points
			const float t =
				(m_entry->maxLevel != m_entry->minLevel) ?
				(getLevel() - m_entry->minLevel) / (m_entry->maxLevel - m_entry->minLevel) :
				0.0f;

			// Base XP for equal level
			UInt32 xp = interpolate(m_entry->xpMin, m_entry->xpMax, t);

			// Level adjustment factor
			const float levelXPMod = calcXpModifier(killer->getLevel());
			xp *= levelXPMod;
			if (xp > 0)
			{
				killer->rewardExperience(this, xp);
			}
		}

		// Decide whether to despawn based on unit type
		const bool isElite = (m_entry->rank > 0 && m_entry->rank < 4);
		const bool isRare = (m_entry->rank == 4);

		// Calculate despawn delay for rare mobs and elite mobs
		GameTime despawnDelay = constants::OneSecond * 30;
		if (isElite || isRare) despawnDelay = constants::OneMinute * 3;

		// Despawn in 30 seconds
		triggerDespawnTimer(despawnDelay);
	}

	float GameCreature::calcXpModifier(UInt32 attackerLevel) const
	{
		// No experience points
		const UInt32 level = getLevel();
		if (level < getGrayLevel(attackerLevel))
			return 0.0f;

		const UInt32 ZD = getZeroDiffXPValue(attackerLevel);
		if (level < attackerLevel)
		{
			return (1.0f - (attackerLevel - level) / ZD);
		}
		else
		{
			return (1.0f + 0.05f * (level - attackerLevel));
		}
	}

	UInt32 getZeroDiffXPValue(UInt32 killerLevel)
	{
		if (killerLevel < 8)
			return 5;
		else if (killerLevel < 10)
			return 6;
		else if (killerLevel < 12)
			return 7;
		else if (killerLevel < 16)
			return 8;
		else if (killerLevel < 20)
			return 9;
		else if (killerLevel < 30)
			return 11;
		else if (killerLevel < 40)
			return 12;
		else if (killerLevel < 45)
			return 13;
		else if (killerLevel < 50)
			return 14;
		else if (killerLevel < 55)
			return 15;
		else if (killerLevel < 60)
			return 16;

		return 17;
	}

	UInt32 getGrayLevel(UInt32 killerLevel)
	{
		if (killerLevel < 6)
			return 0;
		else if (killerLevel < 50)
			return killerLevel - ::floor(killerLevel / 10) - 5;
		else if (killerLevel == 50)
			return killerLevel - 10;
		else if (killerLevel < 60)
			return killerLevel - ::floor(killerLevel / 5) - 1;
		
		return killerLevel - 9;
	}

}
