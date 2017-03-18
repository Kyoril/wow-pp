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

#include "defines.h"
#include "math/vector3.h"
#include "common/simple.hpp"

namespace wowpp
{
	class GameUnit;
	class GameCreature;
	class CreatureAIState;

	/// This class controls a creature unit. It decides, which target the controlled creature
	/// should attack, and also controls it's movement.
	class CreatureAI
	{
	public:

		typedef std::shared_ptr<CreatureAIState> CreatureAIStatePtr;

	public:

		/// Defines the home of a creature.
		struct Home final
		{
			math::Vector3 position;
			float orientation;
			float radius;

			/// Initializes a new instance of the Home class.
			/// @param pos The position of this home point in world units.
			/// @param o The orientation of this home point in radians.
			/// @param radius The tolerance radius of this home point in world units.
			explicit Home(const math::Vector3 &pos, float o = 0.0f, float radius = 0.0f)
				: position(pos)
				, orientation(o)
				, radius(radius)
			{
			}
			/// Default destructor
			~Home();
		};

	public:

		/// Initializes a new instance of the CreatureAI class.
		/// @param controlled The controlled unit, which should also own this class instance.
		/// @param home The home point of the controlled unit. The unit will always return to it's
		///             home point if it returns to it's idle state.
		explicit CreatureAI(
		    GameCreature &controlled,
		    const Home &home
		);
		/// Default destructor. Overridden in case of inheritance of the CreatureAI class.
		virtual ~CreatureAI();

		/// Gets a reference of the controlled creature.
		GameCreature &getControlled() const;
		/// Gets a reference of the controlled units home.
		const Home &getHome() const;
		/// Sets a new home position
		void setHome(Home home);
		/// Enters the idle state.
		void idle();
		/// Enters the combat state. This is usually called from the creatures idle state.
		void enterCombat(GameUnit &victim);
		/// Makes the creature reset, leaving the combat state, reviving itself and run back
		/// to it's home position.
		void reset();
		/// Executed by the underlying creature when combat movement gets enabled or disabled.
		void onCombatMovementChanged();
		/// 
		void onCreatureMovementChanged();
		/// Called when the controlled unit moved.
		void onControlledMoved();
		/// Determines if this creature's AI is currently in evade mode.
		bool isEvading() const { return m_evading; }

	public:

		// These methods are meant to be called by the AI states on specific events.
		void onThreatened(GameUnit &threat, float amount);

	protected:

		void setState(CreatureAIStatePtr state);
		virtual void onSpawned();

	private:

		GameCreature &m_controlled;
		CreatureAIStatePtr m_state;
		Home m_home;
		simple::scoped_connection m_onSpawned, m_onDamaged;
		boost::signals2::scoped_connection m_onKilled;
		bool m_evading;
	};
}
