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

#include "common/typedefs.h"
#include "data/spell_entry.h"
#include "common/countdown.h"
#include "boost/signals2.hpp"

namespace wowpp
{
	// Forwards
	class GameObject;
	class GameUnit;

	/// Represents an instance of a spell aura.
	class Aura
	{
	public:

		boost::signals2::signal<void(Aura &aura)> expired;
		boost::signals2::signal<void(Aura &aura)> removed;

	public:

		/// Initializes a new instance of the Aura class.
		explicit Aura(const SpellEntry &spell, const SpellEntry::Effect &effect, Int32 basePoints, GameUnit &caster, GameUnit &target);
		~Aura();

		/// Gets the unit target.
		GameUnit &getTarget() { return m_target; }
		/// Gets the caster of this aura (if exists).
		GameUnit *getCaster() { return m_caster; }
		/// Applies this aura and initializes everything.
		void applyAura();
		/// Executes the aura modifier and applies or removes the aura effects to/from the target.
		void handleModifier(bool apply);
		/// Determines whether this is a passive spell aura.
		bool isPassive() const { return (m_spell.attributes & spell_attributes::Passive) != 0; }
		/// Determines whether this is a positive spell aura.
		bool isPositive() const;
		/// Gets the current aura slot.
		UInt8 getSlot() const { return m_slot; }
		/// Sets the new aura slot to be used.
		void setSlot(UInt8 newSlot);

		/// Gets the spell which created this aura and hold's it's values.
		const SpellEntry &getSpell() const { return m_spell; }
		/// Gets the spell effect which created this aura.
		const SpellEntry::Effect &getEffect() const { return m_effect; }

	protected:

		/// 
		void handleModNull(bool apply);
		/// 
		void handleModStat(bool apply);
		/// 
		void handleModTotalStatPercentage(bool apply);
		/// 
		void handleModResistance(bool apply);
		/// 
		void handlePeriodicDamage(bool apply);
		/// 
		void handlePeriodicHeal(bool apply);

	private:

		/// Starts the periodic tick timer.
		void startPeriodicTimer();
		/// Executed if the caster of this aura is about to despawn.
		void onCasterDespawned(GameObject &object);
		/// Executed when the aura expires.
		void onExpired();
		/// Executed when this aura ticks.
		void onTick();

	private:

		const SpellEntry &m_spell;
		const SpellEntry::Effect &m_effect;
		boost::signals2::scoped_connection m_casterDespawned;
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
	};
}
