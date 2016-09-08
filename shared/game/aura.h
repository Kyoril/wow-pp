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

#include "common/typedefs.h"
#include "defines.h"
#include "shared/proto_data/spells.pb.h"
#include "common/countdown.h"
#include "spell_target_map.h"

namespace wowpp
{
	// Forwards
	class GameObject;
	class GameUnit;

	/// Represents an instance of a spell aura.
	class Aura : public std::enable_shared_from_this<Aura>
	{
		typedef std::function<void(std::function<void()>)> PostFunction;

	public:

		/// Initializes a new instance of the Aura class.
		explicit Aura(const proto::SpellEntry &spell, 
					  const proto::SpellEffect &effect, 
					  Int32 basePoints, 
					  GameUnit &caster, 
					  GameUnit &target, 
					  SpellTargetMap targetMap, 
					  UInt64 itemGuid,
					  bool isPersistent,
					  PostFunction post, 
					  std::function<void(Aura &)> onDestroy);
		~Aura();

		/// Gets the unit target.
		GameUnit &getTarget() {
			return m_target;
		}
		/// Gets the caster of this aura (if exists).
		GameUnit *getCaster() {
			return m_caster;
		}
		UInt64 getItemGuid() const {
			return m_itemGuid;
		}
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
		/// Determines whether this is a passive spell aura.
		bool isPassive() const {
			return (m_spell.attributes(0) & game::spell_attributes::Passive) != 0;
		}
		/// Determines whether the target may be positive
		static bool hasPositiveTarget(const proto::SpellEffect &effect);
		/// Determines whether this is a positive spell aura.
		/// @returns true if this is a positive aura, false otherwise.
		bool isPositive() const;
		/// Determines whether an aura by a given spell and effect is positive.
		/// @param spell The spell template to check.
		/// @param effect The spell effect to check.
		/// @returns true if an aura of this spell would be a positive aura, false otherwise.
		static bool isPositive(const proto::SpellEntry &spell, const proto::SpellEffect &effect);
		/// Gets the current aura slot.
		UInt8 getSlot() const {
			return m_slot;
		}
		/// Sets the new aura slot to be used.
		void setSlot(UInt8 newSlot);
		/// Forced aura remove.
		void onForceRemoval();

		/// Gets the spell which created this aura and hold's it's values.
		const proto::SpellEntry &getSpell() const {
			return m_spell;
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

	protected:
		
		/// Updates the counter display (stack) of this aura.
		void updateAuraApplication();

	protected:

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
		/// 142
		void handleModBaseResistancePct(bool apply);
		/// 143
		void handleModResistanceExclusive(bool apply);
		/// 182
		void handleModResistanceOfStatPercent(bool apply);
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

	private:

		/// Starts the periodic tick timer.
		void startPeriodicTimer();
		/// Executed if the caster of this aura is about to despawn.
		void onCasterDespawned(GameObject &object);
		/// Executed when the aura expires.
		void onExpired();
		/// Executed when this aura ticks.
		void onTick();
		/// Executed when the target of this aura moved.
		void onTargetMoved(GameObject &, math::Vector3 oldPosition, float oldO);
		///
		void setRemoved(GameUnit *remover);
		/// 
		bool checkProc(bool active, GameUnit *target, UInt32 procFlag, UInt32 procEx, proto::SpellEntry const *procSpell, UInt8 attackType, bool isVictim);
		/// Starts periodic ticks.
		void handlePeriodicBase();

	private:

		const proto::SpellEntry &m_spell;
		const proto::SpellEffect &m_effect;
		boost::signals2::scoped_connection m_casterDespawned, m_targetMoved, m_targetEnteredWater, m_targetStartedAttacking, m_targetStartedCasting, m_onExpire, m_onTick, m_onTargetKilled;
		boost::signals2::scoped_connection m_takenDamage, m_procKilled, m_onDamageBreak, m_onProc, m_onTakenAutoAttack;
		GameUnit *m_caster;
		GameUnit &m_target;
		SpellTargetMap m_targetMap;
		UInt32 m_tickCount;
		GameTime m_applyTime;
		Int32 m_basePoints;
		UInt8 m_procCharges;
		Countdown m_expireCountdown;
		Countdown m_tickCountdown;
		bool m_isPeriodic;
		bool m_expired;
		UInt32 m_attackerLevel;		// Needed for damage calculation
		UInt8 m_slot;
		PostFunction m_post;
		std::function<void(Aura &)> m_destroy;
		UInt32 m_totalTicks;
		Int32 m_duration;
		UInt64 m_itemGuid;
		bool m_isPersistent;
		UInt32 m_stackCount;
	};
}
