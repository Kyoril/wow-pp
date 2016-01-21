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
#include "common/timer_queue.h"
#include "common/countdown.h"
#include "math/vector3.h"
#include "movement_info.h"
#include <boost/signals2.hpp>

namespace wowpp
{
	class GameUnit;

	/// This class is meant to control a units movement. This class should be
	/// inherited, so that, for example, a player character will be controlled
	/// by network input, while a creature is controlled by AI input.
	class UnitMover
	{
	public:

		static const GameTime UpdateFrequency;

	public:

		boost::signals2::signal<void()> targetReached;
		boost::signals2::signal<void()> movementStopped;
		boost::signals2::signal<void()> targetChanged;

	public:
		
		/// 
		explicit UnitMover(GameUnit &unit);
		/// 
		virtual ~UnitMover();

		/// Called when the units movement speed changed.
		void onMoveSpeedChanged(MovementType moveType);
		/// Moves this unit to a specific location if possible. This does not teleport
		/// the unit, but makes it walk / fly / swim to the target.
		bool moveTo(const math::Vector3 &target);
		/// Moves this unit to a specific location if possible. This does not teleport
		/// the unit, but makes it walk / fly / swim to the target.
		bool moveTo(const math::Vector3 &target, float customSpeed);
		/// Stops the current movement if any.
		void stopMovement();
		/// Gets the new movement target.
		const math::Vector3 &getTarget() const { return m_target; }
		/// 
		GameUnit &getMoved() const { return m_unit; }
		/// 
		bool isMoving() const { return m_moveUpdated.running; };
		/// 
		math::Vector3 getCurrentLocation() const;

	private:

		GameUnit &m_unit;
		Countdown m_moveReached, m_moveUpdated;
		math::Vector3 m_start, m_target;
		GameTime m_moveStart, m_moveEnd;
		bool m_customSpeed;
	};
}
