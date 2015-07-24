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
#include "aura.h"
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

	namespace attack_swing_error
	{
		enum Type
		{
			/// Can't auto attack while moving (used for ranged auto attacks)
			NotStanding			= 0,
			/// Target is out of range (or too close in case of ranged auto attacks).
			OutOfRange			= 1,
			/// Can't attack that target (invalid target).
			CantAttack			= 2,
			/// Target has to be in front of us (we need to look at the target).
			WrongFacing			= 3,
			/// The target is dead and thus can not be attacked.
			TargetDead			= 4
		};
	}

	typedef attack_swing_error::Type AttackSwingError;

	namespace unit_mod_type
	{
		enum Type
		{
			/// Absolute base value of this unit based on it's level.
			BaseValue			= 0,
			/// Base value mulitplier (1.0 = 100%)
			BasePct				= 1,
			/// Absolute total value. Final value: BaseValue * BasePct + TotalValue * TotalPct;
			TotalValue			= 2,
			/// Total value multiplier.
			TotalPct			= 3,

			End					= 4
		};
	}

	typedef unit_mod_type::Type UnitModType;

	namespace unit_mods
	{
		enum Type
		{
			/// Strength stat value modifier.
			StatStrength			= 0,
			/// Agility stat value modifier.
			StatAgility				= 1,
			/// Stamina stat value modifier.
			StatStamina				= 2,
			/// Intellect stat value modifier.
			StatIntellect			= 3,
			/// Spirit stat value modifier.
			StatSpirit				= 4,
			/// Health value modifier.
			Health					= 5,
			/// Mana power value modifier.
			Mana					= 6,
			/// Rage power value modifier.
			Rage					= 7,
			/// Focus power value modifier.
			Focus					= 8,
			/// Energy power value modifier.
			Energy					= 9,
			/// Happiness power value modifier.
			Happiness				= 10,
			/// Armor resistance value modifier.
			Armor					= 11,
			/// Holy resistance value modifier.
			ResistanceHoly			= 12,
			/// Fire resistance value modifier.
			ResistanceFire			= 13,
			/// Nature resistance value modifier.
			ResistanceNature		= 14,
			/// Frost resistance value modifier.
			ResistanceFrost			= 15,
			/// Shadow resistance value modifier.
			ResistanceShadow		= 16,
			/// Arcane resistance value modifier.
			ResistanceArcane		= 17,
			/// Melee attack power value modifier.
			AttackPower				= 18,
			/// Ranged attack power value modifier.
			AttackPowerRanged		= 19,
			/// Main hand weapon damage modifier.
			DamageMainHand			= 20,
			/// Off hand weapon damage modifier.
			DamageOffHand			= 21,
			/// Ranged weapon damage modifier.
			DamageRanged			= 22,

			End						= 23,
			
			/// Start of stat value modifiers. Used for iteration.
			StatStart				= StatStrength,
			/// End of stat value modifiers. Used for iteration.
			StatEnd					= StatSpirit + 1,
			/// Start of resistance value modifiers. Used for iteration.
			ResistanceStart			= Armor,
			/// End of resistance value modifiers. Used for iteration.
			ResistanceEnd			= ResistanceArcane + 1,
			/// Start of power value modifiers. Used for iteration.
			PowerStart				= Mana,
			/// End of power value modifiers. Used for iteration.
			PowerEnd				= Happiness + 1
		};
	}

	typedef unit_mods::Type UnitMods;

	namespace base_mod_group
	{
		enum Type
		{
			/// 
			CritPercentage			= 0,
			/// 
			RangedCritPercentage	= 1,
			/// 
			OffHandCritPercentage	= 2,
			/// 
			ShieldBlockValue		= 3,
			
			End						= 4
		};
	}

	typedef base_mod_group::Type BaseModGroup;

	namespace base_mod_type
	{
		enum Type
		{
			/// Absolute modifier value type.
			Flat		= 0,
			/// Percentual modifier value type (float, where 1.0 = 100%).
			Percentage	= 1,
			
			End			= 2
		};
	}

	typedef base_mod_type::Type BaseModType;

	/// 
	class GameUnit : public GameObject
	{
		friend io::Writer &operator << (io::Writer &w, GameUnit const& object);
		friend io::Reader &operator >> (io::Reader &r, GameUnit& object);

	public:

		typedef std::function<void(game::SpellCastResult)> SpellSuccessCallback;

		/// Fired when this unit was killed. Parameter: GameUnit* killer (may be nullptr if killer 
		/// information is not available (for example due to environmental damage))
		boost::signals2::signal<void(GameUnit*)> killed;
		/// Fired when an auto attack error occurred. Used in World Node by the Player class to
		/// send network packets based on the error code.
		boost::signals2::signal<void(AttackSwingError)> autoAttackError;

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

		/// Starts to cast a spell using the given target map.
		void castSpell(SpellTargetMap target, const SpellEntry &spell, GameTime castTime, const SpellSuccessCallback &callback);
		/// Stops the current cast (if any).
		void cancelCast();
		/// Starts auto attack on the given target.
		void startAttack(GameUnit &target);
		/// Stops auto attacking the given target.
		void stopAttack();
		/// 
		GameUnit *getVictim() { return m_victim; }

		/// TODO: Move the logic of this method somewhere else.
		void triggerDespawnTimer(GameTime despawnDelay);
		/// Starts the regeneration countdown.
		void startRegeneration();
		/// Stops the regeneration countdown.
		void stopRegeneration();
		/// 
		GameTime getLastManaUse() const { return m_lastManaUse; }
		/// 
		void notifyManaUse();

		/// Gets the specified unit modifier value.
		float getModifierValue(UnitMods mod, UnitModType type) const;
		/// Sets the unit modifier value to the given value.
		void setModifierValue(UnitMods mod, UnitModType type, float value);
		/// Modifies the given unit modifier value by adding or subtracting it from the current value.
		void updateModifierValue(UnitMods mod, UnitModType type, float amount, bool apply);

		/// Calculates the stat based on the specified modifier.
		static UInt8 getStatByUnitMod(UnitMods mod);
		/// Calculates the power based on the specified unit modifier.
		static PowerType getPowerTypeByUnitMod(UnitMods mod);
		/// 
		static UnitMods getUnitModByStat(UInt8 stat);
		/// 
		static UnitMods getUnitModByPower(PowerType power);

		/// Adds the specified aura to the list of aura effects.
		bool addAura(std::unique_ptr<Aura> aura);

	protected:

		virtual void levelChanged(const LevelEntry &levelInfo);
		virtual void updateAllStats();
		virtual void updateMaxHealth();
		virtual void updateMaxPower(PowerType power);
		virtual void updateArmor();
		virtual void updateDamage();
		virtual void updateManaRegen();
		virtual void updateStats(UInt8 stat);

		float getHealthBonusFromStamina() const;
		float getManaBonusFromIntellect() const;

	private:

		void raceUpdated();
		void classUpdated();
		void updateDisplayIds();
		void onDespawnTimer();
		void onKilled(GameUnit *killer);
		void onVictimKilled(GameUnit *killer);
		void onVictimDespawned();
		void onAttackSwing();
		void onRegeneration();
		void regenerateHealth();
		void regeneratePower(PowerType power);
		void onAuraExpired(Aura &aura);

	private:

		typedef std::array<float, unit_mod_type::End> UnitModTypeArray;
		typedef std::array<UnitModTypeArray, unit_mods::End> UnitModArray;

		typedef std::vector<std::unique_ptr<Aura>> AuraVector;

		TimerQueue &m_timers;
		DataLoadContext::GetRace m_getRace;
		DataLoadContext::GetClass m_getClass;
		DataLoadContext::GetLevel m_getLevel;
		const RaceEntry *m_raceEntry;
		const ClassEntry *m_classEntry;
		std::unique_ptr<SpellCast> m_spellCast;
		Countdown m_despawnCountdown;
		boost::signals2::scoped_connection m_victimDespawned, m_victimDied;
		GameUnit *m_victim;
		Countdown m_attackSwingCountdown;
		GameTime m_lastAttackSwing;
		Countdown m_regenCountdown;
		GameTime m_lastManaUse;
		UnitModArray m_unitMods;
		AuraVector m_auras;
	};

	UInt32 calculateArmorReducedDamage(UInt32 attackerLevel, const GameUnit &victim, UInt32 damage);

	io::Writer &operator << (io::Writer &w, GameUnit const& object);
	io::Reader &operator >> (io::Reader &r, GameUnit& object);
}
