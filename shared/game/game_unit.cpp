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

#include "game_unit.h"
#include <cassert>

namespace wowpp
{
	GameUnit::GameUnit(DataLoadContext::GetRace getRace,
		DataLoadContext::GetClass getClass,
		DataLoadContext::GetLevel getLevel)
		: GameObject()
		, m_getRace(getRace)
		, m_getClass(getClass)
		, m_getLevel(getLevel)
		, m_raceEntry(nullptr)
		, m_classEntry(nullptr)
	{
		// Resize values field
		m_values.resize(unit_fields::UnitFieldCount);
		m_valueBitset.resize((unit_fields::UnitFieldCount + 31) / 32);
	}

	GameUnit::~GameUnit()
	{
	}

	void GameUnit::initialize()
	{
		GameObject::initialize();

		// Initialize some values
		setUInt32Value(object_fields::Type, 9);							//OBJECT_FIELD_TYPE				(TODO: Flags)
		setFloatValue(object_fields::ScaleX, 1.0f);						//OBJECT_FIELD_SCALE_X			(Float)

		// Set unit values
		setUInt32Value(unit_fields::Health, 60);						//UNIT_FIELD_HEALTH
		setUInt32Value(unit_fields::MaxHealth, 60);						//UNIT_FIELD_MAXHEALTH

		setUInt32Value(unit_fields::Power1, 100);						//UNIT_FIELD_POWER1		Mana
		setUInt32Value(unit_fields::Power2, 0);							//UNIT_FIELD_POWER2		Rage
		setUInt32Value(unit_fields::Power3, 100);						//UNIT_FIELD_POWER3		Focus
		setUInt32Value(unit_fields::Power4, 100);						//UNIT_FIELD_POWER4		Energy
		setUInt32Value(unit_fields::Power5, 0);							//UNIT_FIELD_POWER5		Happiness

		setUInt32Value(unit_fields::MaxPower1, 100);					//UNIT_FIELD_MAXPOWER1	Mana
		setUInt32Value(unit_fields::MaxPower2, 1000);					//UNIT_FIELD_MAXPOWER2	Rage
		setUInt32Value(unit_fields::MaxPower3, 100);					//UNIT_FIELD_MAXPOWER3	Focus
		setUInt32Value(unit_fields::MaxPower4, 100);					//UNIT_FIELD_MAXPOWER4	Energy
		setUInt32Value(unit_fields::MaxPower5, 100);					//UNIT_FIELD_MAXPOWER4	Happiness

		// Init some values
		setRace(1);
		setClass(1);
		setGender(game::gender::Male);
		setLevel(1);

		setUInt32Value(unit_fields::UnitFlags, 0x00001000);				//UNIT_FIELD_FLAGS				(TODO: Flags)	UNIT_FLAG_PVP_ATTACKABLE
		setUInt32Value(unit_fields::Aura, 0x0999);						//UNIT_FIELD_AURA				(TODO: Flags)
		setUInt32Value(unit_fields::AuraFlags, 0x09);					//UNIT_FIELD_AURAFLAGS			(TODO: Flags)
		setUInt32Value(unit_fields::AuraLevels, 0x01);					//UNIT_FIELD_AURALEVELS			(TODO: Flags)*/
		setUInt32Value(unit_fields::BaseAttackTime, 2000);				//UNIT_FIELD_BASEATTACKTIME		
		setUInt32Value(unit_fields::BaseAttackTime + 1, 2000);			//UNIT_FIELD_OFFHANDATTACKTIME	
		setUInt32Value(unit_fields::RangedAttackTime, 2000);			//UNIT_FIELD_RANGEDATTACKTIME	
		setUInt32Value(unit_fields::BoundingRadius, 0x3e54fdf4);		//UNIT_FIELD_BOUNDINGRADIUS		(TODO: Float)
		setUInt32Value(unit_fields::CombatReach, 0xf3c00000);			//UNIT_FIELD_COMBATREACH		(TODO: Float)
		setUInt32Value(unit_fields::MinDamage, 0x40a49249);				//UNIT_FIELD_MINDAMAGE			(TODO: Float)
		setUInt32Value(unit_fields::MaxDamage, 0x40c49249);				//UNIT_FIELD_MAXDAMAGE			(TODO: Float)
		setUInt32Value(unit_fields::Bytes1, 0x00110000);				//UNIT_FIELD_BYTES_1

		setFloatValue(unit_fields::ModCastSpeed, 1.0f);					//UNIT_MOD_CAST_SPEED
		setUInt32Value(unit_fields::Resistances, 40);					//UNIT_FIELD_RESISTANCES
		setUInt32Value(unit_fields::BaseHealth, 20);					//UNIT_FIELD_BASE_HEALTH
		setUInt32Value(unit_fields::Bytes2, 0x00002800);				//UNIT_FIELD_BYTES_2
		setUInt32Value(unit_fields::AttackPower, 29);					//UNIT_FIELD_ATTACK_POWER
		setUInt32Value(unit_fields::RangedAttackPower, 11);				//UNIT_FIELD_RANGED_ATTACK_POWER
		setUInt32Value(unit_fields::MinRangedDamage, 0x40249249);		//UNIT_FIELD_MINRANGEDDAMAGE	(TODO: Float)
		setUInt32Value(unit_fields::MaxRangedDamage, 0x40649249);		//UNIT_FIELD_MAXRANGEDDAMAGE	(TODO: Float)
	}

	void GameUnit::raceUpdated()
	{
		m_raceEntry = m_getRace(getRace());
		assert(m_raceEntry);

		// Update faction template
		setUInt32Value(unit_fields::FactionTemplate, m_raceEntry->factionID);	//UNIT_FIELD_FACTIONTEMPLATE
	}

	void GameUnit::classUpdated()
	{
		m_classEntry = m_getClass(getClass());
		assert(m_classEntry);

		// Update power type
		setByteValue(unit_fields::Bytes0, 3, m_classEntry->powerType);

		// Unknown what this does...
		if (m_classEntry->powerType == power_type::Rage ||
			m_classEntry->powerType == power_type::Mana)
		{
			setByteValue(unit_fields::Bytes1, 1, 0xEE);
		}
		else
		{
			setByteValue(unit_fields::Bytes1, 1, 0x00);
		}
	}

	void GameUnit::setRace(UInt8 raceId)
	{
		setByteValue(unit_fields::Bytes0, 0, raceId);
		raceUpdated();
	}

	void GameUnit::setClass(UInt8 classId)
	{
		setByteValue(unit_fields::Bytes0, 1, classId);
		classUpdated();
	}

	void GameUnit::setGender(game::Gender gender)
	{
		setByteValue(unit_fields::Bytes0, 2, gender);
		updateDisplayIds();
	}

	void GameUnit::updateDisplayIds()
	{
		//UNIT_FIELD_DISPLAYID && UNIT_FIELD_NATIVEDISPLAYID
		if (getGender() == game::gender::Male)
		{
			setUInt32Value(unit_fields::DisplayId, m_raceEntry->maleModel);
			setUInt32Value(unit_fields::NativeDisplayId, m_raceEntry->maleModel);
		}
		else
		{
			setUInt32Value(unit_fields::DisplayId, m_raceEntry->femaleModel);
			setUInt32Value(unit_fields::NativeDisplayId, m_raceEntry->femaleModel);
		}
	}

	void GameUnit::setLevel(UInt8 level)
	{
		setUInt32Value(unit_fields::Level, level);

		// Get level information
		const auto *levelInfo = m_getLevel(level);
		if (!levelInfo) return;	// Creatures can have a level higher than 70
		
		// Level info changed
		levelChanged(*levelInfo);
	}

	void GameUnit::levelChanged(const LevelEntry &levelInfo)
	{
		// Get race and class
		const auto race = getRace();
		const auto cls = getClass();

		const auto raceIt = levelInfo.stats.find(race);
		if (raceIt == levelInfo.stats.end()) return;

		const auto classIt = raceIt->second.find(cls);
		if (classIt == raceIt->second.end()) return;

		// Update stats based on level information
		const LevelEntry::StatArray &stats = classIt->second;
		for (UInt32 i = 0; i < stats.size(); ++i)
		{
			setUInt32Value(unit_fields::Stat0 + i, stats[i]);			//UNIT_FIELD_STAT0 + stat
		}
	}

	io::Writer & operator<<(io::Writer &w, GameUnit const& object)
	{
		return w
			<< reinterpret_cast<GameObject const&>(object);
	}

	io::Reader & operator>>(io::Reader &r, GameUnit& object)
	{
		// Read values
		r
			>> reinterpret_cast<GameObject &>(object);

		// Update internals based on received values
		object.raceUpdated();
		object.classUpdated();
		object.updateDisplayIds();

		return r;
	}

}
