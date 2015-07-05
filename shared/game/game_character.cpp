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
	{
		// Resize values field
		m_values.resize(character_fields::CharacterFieldCount);
		m_valueBitset.resize((character_fields::CharacterFieldCount + 31) / 32);
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
	}

	void GameCharacter::setName(const String &name)
	{
		m_name = name;
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

