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
		setUInt32Value(unit_fields::BaseHealth, 20);									//TODO
		setUInt32Value(unit_fields::MaxHealth, interpolate(entry.minLevelHealth, entry.maxLevelHealth, t));
		setUInt32Value(unit_fields::Health, getUInt32Value(unit_fields::MaxHealth));
		setUInt32Value(unit_fields::MaxPower1, interpolate(entry.minLevelMana, entry.maxLevelMana, t));
		setUInt32Value(unit_fields::Power1, getUInt32Value(unit_fields::MaxPower1));
		setUInt32Value(unit_fields::UnitFlags, entry.unitFlags);
		setUInt32Value(unit_fields::DynamicFlags, entry.dynamicFlags);
		setUInt32Value(unit_fields::NpcFlags, entry.npcFlags);
		setByteValue(unit_fields::Bytes2, 1, 16);
		setByteValue(unit_fields::Bytes2, 0, 1);		// Sheath State: Melee weapon

		// Setup new entry
		m_entry = &entry;

		// Emit signal
		if (!isInitialize)
		{
			entryChanged();
		}
	}

}
