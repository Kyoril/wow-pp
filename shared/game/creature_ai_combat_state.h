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
#include "creature_ai_state.h"
#include "math/vector3.h"
#include "common/countdown.h"
#include "game_unit.h"
#include "shared/proto_data/spells.pb.h"
#include "shared/proto_data/units.pb.h"
#include "defines.h"

// This file contains the probably most important state of the AI: The combat state.
// Any creature that is currently in combat with any other unit uses this AI state.
//
// Combat works as follows:
//   When entering this combat state, a victim is required. This is the unit that
//   triggered the combat. This unit is added to the threat list, which is also
//   stored in this class instance.
//   Until the creature is alive and has at least one attackable target in the
//   threat list, it repeats the following steps:
//   * Check if any spells are in the list of creature spells, ordered by 
//     priority, descending
//   * If at least one spell was found, whose soft-conditions are fullfilled
//     (No cooldown, enough resources, valid target/s), the spell will be casted
//   * Otherwise, auto attack is triggered
//   * If a spell should be cast, or an auto attack should be executed, this class
//     checks, if the controlled unit is in range of it's target to execute the 
//     action
//   The above list is repeated periodically

namespace wowpp
{
	/// Handle the idle state of a creature AI. In this state, most units
	/// watch for hostile units which come close enough, and start attacking these
	/// units.
	class CreatureAICombatState : public CreatureAIState
	{
		/// Represents an entry in the threat list of this unit.
		struct ThreatEntry
		{
			/// Threatening unit.
			std::weak_ptr<GameUnit> threatener;
			float amount;

			explicit ThreatEntry(GameUnit &threatener, float amount = 0.0f)
				: threatener(std::static_pointer_cast<GameUnit>(threatener.shared_from_this()))
				, amount(amount)
			{
			}
		};

		typedef std::map<UInt64, ThreatEntry> ThreatList;
		typedef std::map<UInt64, simple::scoped_connection> UnitSignals;
		typedef std::map<UInt64, simple::scoped_connection_container> UnitSignals2;

	public:

		/// Initializes a new instance of the CreatureAIIdleState class.
		/// @param ai The ai class instance this state belongs to.
		explicit CreatureAICombatState(CreatureAI &ai, GameUnit &victim);
		/// Default destructor.
		virtual ~CreatureAICombatState();

		/// @copydoc CreatureAIState::onEnter()
		virtual void onEnter() override;
		/// @copydoc CreatureAIState::onLeave()
		virtual void onLeave() override;
		/// @copydoc CreatureAIState::onDamage()
		virtual void onDamage(GameUnit &attacker) override;
		/// @copydoc CreatureAIState::onCombatMovementChanged()
		virtual void onCombatMovementChanged() override;
		/// @copydoc CreatureAIState::onControlledMoved()
		virtual void onControlledMoved() override;

	private:

		/// Adds threat of an attacker to the threat list.
		/// @param threatener The unit which generated threat.
		/// @param amount Amount of threat which will be added. May be 0.0 to simply
		///               add the unit to the threat list.
		void addThreat(GameUnit &threatener, float amount);
		/// Removes a unit from the threat list. This may change the AI state.
		/// @param threatener The unit that will be removed from the threat list.
		void removeThreat(GameUnit &threatener);
		/// Gets the amount of threat of an attacking unit. Returns 0.0f, if that unit
		/// does not exist, so don't use this method to check if a unit is on the threat
		/// list at all.
		/// @param threatener The unit whose threat-value is requested.
		/// @returns Threat value of the attacking unit, which may be 0
		float getThreat(GameUnit &threatener);
		/// Sets the amount of threat of an attacking unit. This also adds the unit to
		/// the threat list if it isn't already added. Setting the amount to 0 will not
		/// remove the unit from the threat list!
		/// @param threatener The attacking unit whose threat value should be set.
		/// @param amount The new total threat amount, which may be 0.
		void setThreat(GameUnit &threatener, float amount);
		/// Determines the unit with the most amount of threat. May return nullptr if no
		/// unit available.
		/// @returns Pointer of the unit with the highest threat-value or nullptr.
		GameUnit *getTopThreatener();
		/// Updates the current victim of the controlled unit based on the current threat table.
		/// A call of this method may change the AI state, in which case this state can be deleted!
		/// So be careful here.
		void updateVictim();
		/// Starts chasing a unit so that the controlled unit is in melee hit range.
		/// @param target The unit to chase.
		void chaseTarget(GameUnit &target);
		/// Determines the next action to execute. This method calls updateVictim, and thus may
		/// change the AI state. So be careful as well.
		void chooseNextAction();
		/// Executed after a spell cast was sucessfully executed.
		/// @param result Result of the spell cast.
		void onSpellCast(game::SpellCastResult result);
		/// Executed when a unit state of the controlled creature was changed.
		/// @param state The unit state which changed.
		/// @param apply True if the state was applied, false if it was removed.
		void onUnitStateChanged(UInt32 state, bool apply);
		/// Executed when the controlled unit is now stunned or no longer stunned.
		/// @param apply True if now stunned, false otherwise.
		void onStunStateChanged(bool apply);
		/// Executed when the controlled unit is now confused or no longer confused.
		/// @param apply True if now confused, false otherwise.
		void onConfuseStateChanged(bool apply);
		/// Executed when the controlled unit is now feared or no longer feared.
		/// @param apply True if now feared, false otherwise.
		void onFearStateChanged(bool apply);
		/// Executed when the controlled unit is now rooted or no longer rooted.
		/// @param apply True if now stunned, false otherwise.
		void onRootStateChanged(bool apply);

	private:

		GameUnit *m_combatInitiator;
		ThreatList m_threat;
		UnitSignals m_killedSignals;
		UnitSignals2 m_miscSignals;
		simple::scoped_connection m_onThreatened, m_onMoveTargetChanged;
		simple::scoped_connection m_getThreat, m_setThreat, m_getTopThreatener;
		simple::scoped_connection m_onUnitStateChanged;
		simple::scoped_connection m_onAutoAttackDone;
		GameTime m_lastThreatTime;
		Countdown m_nextActionCountdown;

		const proto::UnitSpellEntry *m_lastSpellEntry;
		const proto::SpellEntry *m_lastSpell;
		GameTime m_lastCastTime;
		UInt32 m_customCooldown;
		game::SpellCastResult m_lastCastResult;
		bool m_isCasting;
		bool m_entered;
		bool m_isRanged;
	};
}
