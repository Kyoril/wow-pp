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

#include "game_object.h"
#include "data/race_entry.h"
#include "data/class_entry.h"
#include "data/level_entry.h"
#include "data/spell_entry.h"
#include "game/defines.h"
#include "data/data_load_context.h"
#include "common/timer_queue.h"
#include "common/countdown.h"
#include "spell_cast.h"
#include "spell_target_map.h"
#include <boost/signals2.hpp>

namespace wowpp
{
	namespace unit_stand_state
	{
		enum Enum
		{
			Stand			= 0x00,
			Sit				= 0x01,
			SitChair		= 0x02,
			Sleep			= 0x03,
			SitLowChair		= 0x04,
			SitMediumChais	= 0x05,
			SitHighChair	= 0x06,
			Dead			= 0x07,
			Kneel			= 0x08
		};
	}

	typedef unit_stand_state::Enum UnitStandState;

	namespace unit_fields
	{
		enum Enum
		{
			Charm						= 0x00 + object_fields::ObjectFieldCount,
			Summon						= 0x02 + object_fields::ObjectFieldCount,
			CharmedBy					= 0x04 + object_fields::ObjectFieldCount,
			SummonedBy					= 0x06 + object_fields::ObjectFieldCount,
			CreatedBy					= 0x08 + object_fields::ObjectFieldCount,
			Target						= 0x0A + object_fields::ObjectFieldCount,
			Persuaded					= 0x0C + object_fields::ObjectFieldCount,
			ChannelObject				= 0x0E + object_fields::ObjectFieldCount,
			Health						= 0x10 + object_fields::ObjectFieldCount,
			Power1						= 0x11 + object_fields::ObjectFieldCount,
			Power2						= 0x12 + object_fields::ObjectFieldCount,
			Power3						= 0x13 + object_fields::ObjectFieldCount,
			Power4						= 0x14 + object_fields::ObjectFieldCount,
			Power5						= 0x15 + object_fields::ObjectFieldCount,
			MaxHealth					= 0x16 + object_fields::ObjectFieldCount,
			MaxPower1					= 0x17 + object_fields::ObjectFieldCount,
			MaxPower2					= 0x18 + object_fields::ObjectFieldCount,
			MaxPower3					= 0x19 + object_fields::ObjectFieldCount,
			MaxPower4					= 0x1A + object_fields::ObjectFieldCount,
			MaxPower5					= 0x1B + object_fields::ObjectFieldCount,
			Level						= 0x1C + object_fields::ObjectFieldCount,
			FactionTemplate				= 0x1D + object_fields::ObjectFieldCount,
			Bytes0						= 0x1E + object_fields::ObjectFieldCount,
			VirtualItemSlotDisplay		= 0x1F + object_fields::ObjectFieldCount,
			VirtualItemInfo				= 0x22 + object_fields::ObjectFieldCount,
			UnitFlags					= 0x28 + object_fields::ObjectFieldCount,
			UnitFlags2					= 0x29 + object_fields::ObjectFieldCount,
			Aura						= 0x2A + object_fields::ObjectFieldCount,
			AuraList					= 0x58 + object_fields::ObjectFieldCount,
			AuraFlags					= 0x62 + object_fields::ObjectFieldCount,
			AuraLevels					= 0x70 + object_fields::ObjectFieldCount,
			AuraApplications			= 0x7E + object_fields::ObjectFieldCount,
			AuraState					= 0x8C + object_fields::ObjectFieldCount,
			BaseAttackTime				= 0x8D + object_fields::ObjectFieldCount,
			RangedAttackTime			= 0x8F + object_fields::ObjectFieldCount,
			BoundingRadius				= 0x90 + object_fields::ObjectFieldCount,
			CombatReach					= 0x91 + object_fields::ObjectFieldCount,
			DisplayId					= 0x92 + object_fields::ObjectFieldCount,
			NativeDisplayId				= 0x93 + object_fields::ObjectFieldCount,
			MountDisplayId				= 0x94 + object_fields::ObjectFieldCount,
			MinDamage					= 0x95 + object_fields::ObjectFieldCount,
			MaxDamage					= 0x96 + object_fields::ObjectFieldCount,
			MinOffHandDamage			= 0x97 + object_fields::ObjectFieldCount,
			MaxOffHandDamage			= 0x98 + object_fields::ObjectFieldCount,
			Bytes1						= 0x99 + object_fields::ObjectFieldCount,
			PetNumber					= 0x9A + object_fields::ObjectFieldCount,
			PetNameTimestamp			= 0x9B + object_fields::ObjectFieldCount,
			PetExperience				= 0x9C + object_fields::ObjectFieldCount,
			PetNextLevelExp				= 0x9D + object_fields::ObjectFieldCount,
			DynamicFlags				= 0x9E + object_fields::ObjectFieldCount,
			ChannelSpell				= 0x9F + object_fields::ObjectFieldCount,
			ModCastSpeed				= 0xA0 + object_fields::ObjectFieldCount,
			CreatedBySpell				= 0xA1 + object_fields::ObjectFieldCount,
			NpcFlags					= 0xA2 + object_fields::ObjectFieldCount,
			NpcEmoteState				= 0xA3 + object_fields::ObjectFieldCount,
			TrainingPoints				= 0xA4 + object_fields::ObjectFieldCount,
			Stat0						= 0xA5 + object_fields::ObjectFieldCount,
			Stat1						= 0xA6 + object_fields::ObjectFieldCount,
			Stat2						= 0xA7 + object_fields::ObjectFieldCount,
			Stat3						= 0xA8 + object_fields::ObjectFieldCount,
			Stat4						= 0xA9 + object_fields::ObjectFieldCount,
			PosStat0					= 0xAA + object_fields::ObjectFieldCount,
			PosStat1					= 0xAB + object_fields::ObjectFieldCount,
			PosStat2					= 0xAC + object_fields::ObjectFieldCount,
			PosStat3					= 0xAD + object_fields::ObjectFieldCount,
			PosStat4					= 0xAE + object_fields::ObjectFieldCount,
			NegStat0					= 0xAF + object_fields::ObjectFieldCount,
			NegStat1					= 0xB0 + object_fields::ObjectFieldCount,
			NegStat2					= 0xB1 + object_fields::ObjectFieldCount,
			NegStat3					= 0xB2 + object_fields::ObjectFieldCount,
			NegStat4					= 0xB3 + object_fields::ObjectFieldCount,
			Resistances					= 0xB4 + object_fields::ObjectFieldCount,
			ResistancesBuffModsPositive	= 0xBB + object_fields::ObjectFieldCount,
			ResistancesBuffModsNegative	= 0xC2 + object_fields::ObjectFieldCount,
			BaseMana					= 0xC9 + object_fields::ObjectFieldCount,
			BaseHealth					= 0xCA + object_fields::ObjectFieldCount,
			Bytes2						= 0xCB + object_fields::ObjectFieldCount,
			AttackPower					= 0xCC + object_fields::ObjectFieldCount,
			AttackPowerMods				= 0xCD + object_fields::ObjectFieldCount,
			AttackPowerMultiplier		= 0xCE + object_fields::ObjectFieldCount,
			RangedAttackPower			= 0xCF + object_fields::ObjectFieldCount,
			RangedAttackPowerMods		= 0xD0 + object_fields::ObjectFieldCount,
			RangedAttackPowerMultiplier	= 0xD1 + object_fields::ObjectFieldCount,
			MinRangedDamage				= 0xD2 + object_fields::ObjectFieldCount,
			MaxRangedDamage				= 0xD3 + object_fields::ObjectFieldCount,
			PowerCostModifier			= 0xD4 + object_fields::ObjectFieldCount,
			PowerCostMultiplier			= 0xDB + object_fields::ObjectFieldCount,
			MaxHealthModifier			= 0xE2 + object_fields::ObjectFieldCount,
			Padding						= 0xE3 + object_fields::ObjectFieldCount,

			UnitFieldCount				= 0xE4 + object_fields::ObjectFieldCount,
		};
	}

	/// 
	class GameUnit : public GameObject
	{
		friend io::Writer &operator << (io::Writer &w, GameUnit const& object);
		friend io::Reader &operator >> (io::Reader &r, GameUnit& object);

	public:

		/// Fired when this unit was killed. Parameter: GameUnit* killer (may be nullptr if killer 
		/// information is not available (for example due to environmental damage))
		boost::signals2::signal<void(GameUnit*)> killed;

	public:

		/// 
		explicit GameUnit(
			TimerQueue &timers,
			DataLoadContext::GetRace getRace, 
			DataLoadContext::GetClass getClass,
			DataLoadContext::GetLevel getLevel);
		~GameUnit();

		virtual void initialize() override;

		void setRace(UInt8 raceId);
		void setClass(UInt8 classId);
		void setGender(game::Gender gender);
		void setLevel(UInt8 level);
		UInt8 getRace() const { return getByteValue(unit_fields::Bytes0, 0); }
		UInt8 getClass() const { return getByteValue(unit_fields::Bytes0, 1); }
		UInt8 getGender() const { return getByteValue(unit_fields::Bytes0, 2); }
		UInt32 getLevel() const { return getUInt32Value(unit_fields::Level); }

		/// Gets the timer queue object needed for countdown events.
		TimerQueue &getTimers() { return m_timers; }

		const RaceEntry *getRaceEntry() const {  return m_raceEntry; }
		const ClassEntry *getClassEntry() const { return m_classEntry; }

		virtual ObjectType getTypeId() const override { return object_type::Unit; }

		void castSpell(SpellTargetMap target, const SpellEntry &spell, GameTime castTime);
		void cancelCast();

		/// TODO: Move the logic of this method somewhere else.
		void triggerDespawnTimer(GameTime despawnDelay);

	protected:

		virtual void levelChanged(const LevelEntry &levelInfo);

	private:

		void raceUpdated();
		void classUpdated();
		void updateDisplayIds();
		void onDespawnTimer();

	private:

		TimerQueue &m_timers;
		DataLoadContext::GetRace m_getRace;
		DataLoadContext::GetClass m_getClass;
		DataLoadContext::GetLevel m_getLevel;
		const RaceEntry *m_raceEntry;
		const ClassEntry *m_classEntry;
		std::unique_ptr<SpellCast> m_spellCast;
		Countdown m_despawnCountdown;
	};

	io::Writer &operator << (io::Writer &w, GameUnit const& object);
	io::Reader &operator >> (io::Reader &r, GameUnit& object);
}
