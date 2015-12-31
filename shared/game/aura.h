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
#include "boost/signals2.hpp"

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
		explicit Aura(const proto::SpellEntry &spell, const proto::SpellEffect &effect, Int32 basePoints, GameUnit &caster, GameUnit &target, PostFunction post, std::function<void(Aura&)> onDestroy);
		~Aura();

		/// Gets the unit target.
		GameUnit &getTarget() { return m_target; }
		/// Gets the caster of this aura (if exists).
		GameUnit *getCaster() { return m_caster; }
		/// Applies this aura and initializes everything.
		void applyAura();
		/// Executes the aura modifier and applies or removes the aura effects to/from the target.
		void handleModifier(bool apply);
		/// 
		void handleProcModifier(GameUnit *target = nullptr);
		/// Determines whether this is a passive spell aura.
		bool isPassive() const { return (m_spell.attributes(0) & game::spell_attributes::Passive) != 0; }
		/// Determines whether this is a positive spell aura.
		bool isPositive() const;
		/// Gets the current aura slot.
		UInt8 getSlot() const { return m_slot; }
		/// Sets the new aura slot to be used.
		void setSlot(UInt8 newSlot);
		/// Forced aura remove.
		void onForceRemoval();

		/// Gets the spell which created this aura and hold's it's values.
		const proto::SpellEntry &getSpell() const { return m_spell; }
		/// Gets the spell effect which created this aura.
		const proto::SpellEffect &getEffect() const { return m_effect; }

	protected:

		/// 0
		void handleModNull(bool apply);
		/// 3
		void handlePeriodicDamage(bool apply);
		/// 4
		void handleDummy(bool apply);
		/// 8
		void handlePeriodicHeal(bool apply);
		/// 12
		void handleModStun(bool apply);
		/// 15
		void handleDamageShield(bool apply);
		/// 16
		void handleModStealth(bool apply);
		/// 22
		void handleModResistance(bool apply);
		/// 24
		void handlePeriodicEnergize(bool apply);
		/// 29
		void handleModStat(bool apply);
		/// 36
		void handleModShapeShift(bool apply);
		/// 99
		void handleModAttackPower(bool apply);
		/// 118
		void handleModHealingPct(bool apply);
		/// 137
		void handleModTotalStatPercentage(bool apply);
		/// 142
		void handleModBaseResistancePct(bool apply);

	protected:

		/// general
		void handleTakenDamage(GameUnit *attacker);
		/// 4
		void handleDummyProc(GameUnit *victim);
		/// 15
		void handleDamageShieldProc(GameUnit *attacker);
		/// 42
		void handleTriggerSpellProc(GameUnit *attacker);

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
		void onTargetMoved(GameObject &, float oldX, float oldY, float oldZ, float oldO);
		/// 
		void setRemoved(GameUnit *remover);

	private:

		const proto::SpellEntry &m_spell;
		const proto::SpellEffect &m_effect;
		boost::signals2::scoped_connection m_casterDespawned, m_targetMoved, m_onExpire, m_onTick, m_onTargetKilled;
		boost::signals2::scoped_connection m_procAutoAttack, m_procTakenAutoAttack, m_doneSpellMagicDmgClassNeg, m_takenDamage;
		GameUnit *m_caster;
		GameUnit &m_target;
		UInt32 m_tickCount;
		GameTime m_applyTime;
		Int32 m_basePoints;
		Countdown m_expireCountdown;
		Countdown m_tickCountdown;
		bool m_isPeriodic;
		bool m_expired;
		UInt32 m_attackerLevel;		// Needed for damage calculation
		UInt8 m_slot;
		PostFunction m_post;
		std::function<void(Aura&)> m_destroy;
		UInt32 m_totalTicks;
	};
}
