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

#pragma once

#include "shared/proto_data/spells.pb.h"
#include "common/countdown.h"
#include "spell_target_map.h"

namespace wowpp
{
	// Forwards
	class GameObject;
	class GameUnit;
	class AuraSpellSlot;

	/// Represents an instance of a spell aura.
	class AuraEffect : public std::enable_shared_from_this<AuraEffect>
	{
		typedef std::function<void(std::function<void()>)> PostFunction;

	public:

		simple::signal<void()> misapplied;

	public:

		/// Initializes a new instance of the AuraEffect class.
		explicit AuraEffect(
			AuraSpellSlot &slot, 
			const proto::SpellEffect &effect,
			Int32 basePoints, 
			GameUnit &caster, 
			GameUnit &target, 
			SpellTargetMap targetMap,
			bool isPersistent);

		/// Gets the unit target.
		GameUnit &getTarget() {
			return m_target;
		}
		/// Gets the caster of this aura (if exists).
		GameUnit *getCaster() {
			return m_caster.get();
		}
		/// Gets the caster guid (or 0 if no caster was set).
		UInt64 getCasterGuid() const;
		/// Applies this aura and initializes everything.
		void applyAura();
		/// This method is the counterpart of applyAura(). It exists so that
		/// handleModifier(false) is not called in the destructor of the aura, since this causes crashes if
		/// somehow the AuraContainer is accessed.
		void misapplyAura();
		/// Executes the aura modifier and applies or removes the aura effects to/from the target.
		void handleModifier(bool apply);
		///
		void handleProcModifier(UInt8 attackType, bool canRemove, UInt32 amount, GameUnit *attacker = nullptr);

		AuraSpellSlot &getSlot() {
			return m_spellSlot;
		}
		/// Gets the spell effect which created this aura.
		const proto::SpellEffect &getEffect() const {
			return m_effect;
		}
		/// Gets this auras base points. Note that these points aren't constant, and can change while this
		/// aura is in use (for example absorb auras).
		Int32 getBasePoints() {
			return m_basePoints;
		}
		/// Updates the auras base points.
		/// @
		void setBasePoints(Int32 basePoints);

		UInt32 getEffectSchoolMask();

		Int32 getTotalDuration() const { 
			return m_duration; 
		}

		bool isStealthAura() const;

		void update();

		UInt32 getTickCount() {
			return m_tickCount;
		}

		UInt32 getMaxTickCount() {
			return m_totalTicks;
		}

		UInt32 getStackCount() {
			return m_stackCount;
		}

		void updateStackCount(Int32 points);

		/// Executed when the target of this aura moved.
		void onTargetMoved(const math::Vector3 &oldPosition, float oldO);
		/// Executed when the aura expires.
		void onExpired();

	protected:
		
		/// Updates the counter display (stack) of this aura.
		void updateAuraApplication();

	protected:

		// AuraEffect effect handlers implemented in aura_effects.cpp

		/// 0
		void handleModNull(bool apply);
		/// 3
		void handlePeriodicDamage(bool apply);
		/// 4
		void handleDummy(bool apply);
		/// 5
		void handleModConfuse(bool apply);
		/// 7
		void handleModFear(bool apply);
		/// 8
		void handlePeriodicHeal(bool apply);
		/// 10
		void handleModThreat(bool apply);
		/// 12
		void handleModStun(bool apply);
		/// 13
		void handleModDamageDone(bool apply);
		/// 14
		void handleModDamageTaken(bool apply);
		/// 15
		void handleDamageShield(bool apply);
		/// 16
		void handleModStealth(bool apply);
		/// 20
		void handleObsModHealth(bool apply);
		/// 21
		void handleObsModMana(bool apply);
		/// 22
		void handleModResistance(bool apply);
		/// 23
		void handlePeriodicTriggerSpell(bool apply);
		/// 24
		void handlePeriodicEnergize(bool apply);
		/// 26
		void handleModRoot(bool apply);
		/// 29
		void handleModStat(bool apply);
		/// 31, 33, 129, 171
		void handleRunSpeedModifier(bool apply);
		/// 33, 58, 171
		void handleSwimSpeedModifier(bool apply);
		/// 33, 171, 206, 208, 210
		void handleFlySpeedModifier(bool apply);
		/// 36
		void handleModShapeShift(bool apply);
		/// 44
		void handleTrackCreatures(bool apply);
		/// 45
		void handleTrackResources(bool apply);
		/// 47
		void handleModParryPercent(bool apply);
		/// 49
		void handleModDodgePercent(bool apply);
		/// 52
		void handleModCritPercent(bool apply);
		/// 53
		void handlePeriodicLeech(bool apply);
		/// 56
		void handleTransform(bool apply);
		/// 65
		void handleModCastingSpeed(bool apply);
		/// 69
		void handleSchoolAbsorb(bool apply);
		/// 72
		void handleModPowerCostSchoolPct(bool apply);
		/// 77
		void handleMechanicImmunity(bool apply);
		/// 78
		void handleMounted(bool apply);
		/// 79
		void handleModDamagePercentDone(bool apply);
		/// 85
		void handleModPowerRegen(bool apply);
		/// 86
		void handleChannelDeathItem(bool apply);
		/// 97
		void handleManaShield(bool apply);
		/// 99
		void handleModAttackPower(bool apply);
		/// 101
		void handleModResistancePct(bool apply);
		/// 103
		void handleModTotalThreat(bool apply);
		/// 104
		void handleWaterWalk(bool apply);
		/// 105
		void handleFeatherFall(bool apply);
		/// 106
		void handleHover(bool apply);
		/// 107 & 108
		void handleAddModifier(bool apply);
		/// 109
		void handleAddTargetTrigger(bool apply);
		/// 118
		void handleModHealingPct(bool apply);
		/// 123
		void handleModTargetResistance(bool apply);
		/// 132
		void handleModEnergyPercentage(bool apply);
		/// 133
		void handleModHealthPercentage(bool apply);
		/// 134
		void handleModManaRegenInterrupt(bool apply);
		/// 135
		void handleModHealingDone(bool apply);
		/// 137
		void handleModTotalStatPercentage(bool apply);
		/// 138
		void handleModHaste(bool apply);
		/// 140
		void handleModRangedHaste(bool apply);
		/// 141
		void handleModRangedAmmoHaste(bool apply);
		/// 142
		void handleModBaseResistancePct(bool apply);
		/// 143
		void handleModResistanceExclusive(bool apply);
		/// 182
		void handleModResistanceOfStatPercent(bool apply);
		/// 189
		void handleModRating(bool apply);
		/// 201
		void handleFly(bool apply);
		/// 226
		void handlePeriodicDummy(bool apply);

	protected:

		/// general
		void handleTakenDamage(GameUnit *attacker);
		/// 4
		void handleDummyProc(GameUnit *victim, UInt32 amount);
		/// 15
		void handleDamageShieldProc(GameUnit *attacker);
		/// 42
		void handleTriggerSpellProc(GameUnit *attacker, UInt32 amount);

	protected:

		// Periodic tick effects
		void calculatePeriodicDamage(UInt32 &out_damage, UInt32 &out_absorbed, UInt32 &out_resisted);
		void periodicLeechEffect();

	private:

		/// Starts the periodic tick timer.
		void startPeriodicTimer();
		/// Executed when this aura ticks.
		void onTick();
		/// 
		bool checkProc(bool active, GameUnit *target, UInt32 procFlag, UInt32 procEx, proto::SpellEntry const *procSpell, UInt8 attackType, bool isVictim);
		/// Starts periodic ticks.
		void handlePeriodicBase();

	private:

		AuraSpellSlot &m_spellSlot;
		const proto::SpellEffect &m_effect;
		simple::scoped_connection m_onTick, m_takenDamage;
		simple::scoped_connection m_targetMoved, m_targetEnteredWater, m_targetStartedAttacking, m_targetStartedCasting, m_onTargetKilled;
		simple::scoped_connection m_procKilled, m_onDamageBreak, m_onProc, m_onTakenAutoAttack;
		std::shared_ptr<GameUnit> m_caster;
		GameUnit &m_target;
		SpellTargetMap m_targetMap;
		UInt32 m_tickCount;
		GameTime m_applyTime;
		Int32 m_basePoints;
		UInt8 m_procCharges;
		Countdown m_tickCountdown;
		bool m_isPeriodic;
		bool m_expired;
		UInt32 m_totalTicks;
		Int32 m_duration;
		bool m_isPersistent;
		UInt32 m_stackCount;
	};
}
