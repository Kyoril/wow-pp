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

namespace wowpp
{
	class CreatureAI;
	class GameCreature;
	class GameUnit;

	/// Represents a specific AI state of a creature (for example the idle state or the combat state).
	class CreatureAIState
	{
	public:

		/// Initializes a new instance of the CreatureAIState class.
		/// @param ai The ai class instance this state belongs to.
		explicit CreatureAIState(CreatureAI &ai);
		/// Default destructor.
		virtual ~CreatureAIState();

		/// Gets a reference of the AI this state belongs to.
		CreatureAI &getAI() const;
		/// Gets a reference of the controlled creature.
		GameCreature &getControlled() const;
		/// Executed when the AI state is activated.
		virtual void onEnter();
		/// Executed when the AI state becomes inactive.
		virtual void onLeave();
		/// Executed when the controlled unit was damaged by a known attacker (not executed when
		/// the attacker is unknown, for example in case of Auras where the caster is despawned)
		virtual void onDamage(GameUnit &attacker);
		/// Executed when the controlled unit was healed by a known healer (same as onDamage).
		virtual void onHeal(GameUnit &healer);
		/// Executed when the controlled unit dies.
		virtual void onControlledDeath();
		/// Executed when combat movement for the controlled unit is enabled or disabled.
		virtual void onCombatMovementChanged();
		/// 
		virtual void onCreatureMovementChanged();

	private:

		CreatureAI &m_ai;
	};
}
