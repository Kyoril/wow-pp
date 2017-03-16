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
#include "movement_path.h"

namespace wowpp
{
	class GameUnit;
	struct ITileSubscriber;
	struct IShape;

	/// This class is meant to control a units movement. This class should be
	/// inherited, so that, for example, a player character will be controlled
	/// by network input, while a creature is controlled by AI input.
	class UnitMover
	{
	public:

		static const GameTime UpdateFrequency;

	public:

		/// Fired when the unit reached it's target.
		boost::signals2::signal<void()> targetReached;
		/// Fired when the movement was stopped.
		boost::signals2::signal<void()> movementStopped;
		/// Fired when the target changed.
		boost::signals2::signal<void()> targetChanged;

	public:

		/// 
		explicit UnitMover(GameUnit &unit);
		/// 
		virtual ~UnitMover();

		/// Enables or disables movement at any angle on terrain. This is required for creatures
		/// in combat, so that they can move up on terrain to get to players.
		/// @param enabled true to enable terrain movement on any slope angle, false to disable it.
		void setTerrainMovement(bool enabled) { m_canWalkOnTerrain = enabled; }
		/// Called when the units movement speed changed.
		void onMoveSpeedChanged(MovementType moveType);
		/// Moves this unit to a specific location if possible. This does not teleport
		/// the unit, but makes it walk / fly / swim to the target.
		bool moveTo(const math::Vector3 &target, const IShape *clipping = nullptr);
		/// Moves this unit to a specific location if possible. This does not teleport
		/// the unit, but makes it walk / fly / swim to the target.
		bool moveTo(const math::Vector3 &target, float customSpeed, const IShape *clipping = nullptr);
		/// Stops the current movement if any.
		void stopMovement();
		/// Gets the new movement target.
		const math::Vector3 &getTarget() const
		{
			return m_target;
		}
		/// 
		GameUnit &getMoved() const
		{
			return m_unit;
		}
		/// 
		bool isMoving() const
		{
			return m_moveReached.running;
		};
		/// 
		math::Vector3 getCurrentLocation() const;
		/// Enables or disables debug output of generated waypoints and other events.
		void setDebugOutput(bool enable) { m_debugOutputEnabled = enable; }

	public:

		/// Writes the current movement packets to a writer object. This is used for moving
		/// creatures that are spawned for a player.
		void sendMovementPackets(ITileSubscriber &subscriber);

	private:

		GameUnit &m_unit;
		Countdown m_moveReached, m_moveUpdated;
		math::Vector3 m_start, m_target;
		GameTime m_moveStart, m_moveEnd;
		bool m_customSpeed;
		bool m_debugOutputEnabled;
		bool m_canWalkOnTerrain;
		MovementPath m_path;
	};
}
