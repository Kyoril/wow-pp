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
#include "math/vector3.h"

namespace wowpp
{
	class MovementPath
	{
	public:

		typedef GameTime Timestamp;
		/// Maps a position vector to a timestamp.
		typedef std::map<Timestamp, math::Vector3> PositionMap;

	public:

		/// Default constructor.
		MovementPath();
		/// Destructor.
		virtual ~MovementPath();

		/// Clears the current movement path, deleting all informations.
		void clear();
		/// Determines whether some positions have been given to this path.
		virtual bool hasPositions() const { return !m_position.empty(); }
		/// Adds a new position to the path and assigns it to a specific timestamp.
		/// @param timestamp The timestamp of when the position will be or was reached.
		/// @param position The position at the given time.
		virtual void addPosition(Timestamp timestamp, const math::Vector3 &position);
		/// Calculates the units position on the path based on the given timestamp.
		/// @param timestamp The timestamp.
		/// @returns The position of the unit at the given timestamp or a null vector 
		///          if this is an empty path.
		virtual math::Vector3 getPosition(Timestamp timestamp) const;
		/// 
		virtual const PositionMap &getPositions() const { return m_position; }
		/// Prints some debug infos to the log.
		virtual void printDebugInfo();

	private:

		/// Stored position values with timestamp information.
		PositionMap m_position;

		/// The first position timestamp information stored.
		Timestamp m_firstPosTimestamp;
		/// The last position timestamp information stored.
		Timestamp m_lastPosTimestamp;
	};
}
