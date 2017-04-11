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

#include "pch.h"
#include "movement_path.h"
#include "common/macros.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace
	{
		template <typename T>
		static math::Vector3 doInterpolation(MovementPath::Timestamp timestamp, MovementPath::Timestamp firstTimestamp, MovementPath::Timestamp lastTimestamp, const T &map)
		{
			// No data! What should we do? ...
			if (map.empty())
				return math::Vector3();

			// Get start and end iterator
			typename T::const_iterator t1 = map.end(), t2 = map.end();

			// Get first position
			typename T::const_iterator it = map.begin();

			// Iterate through all saved positions
			while (it != map.end())
			{
				// Check if the value is bigger and if the new value
				// is not the last value
				if (it->first <= timestamp &&
					it->first < lastTimestamp)
				{
					t1 = it;
				}


				if (t1 != map.end() &&
					t2 == map.end())
				{
					if (it->first > timestamp)
					{
						t2 = it;
					}
				}

				// Next
				it++;
			}

			// If there is no start value...
			if (t1 == map.end())
			{
				t1 = map.begin();

				// T2 = T1 + 1
				t2 = t1;
				std::advance(t2, 1);
			}

			// If no end value was found (maybe it is out of space), use the last 
			// known value as end point
			if (t2 == map.end())
			{
				// No end point found, use last point available (source point)
				return t1->second;
			}

			// Normalize end time value (end - start) for a range of 0 to END
			MovementPath::Timestamp nEnd = static_cast<MovementPath::Timestamp>(t2->first - t1->first);
			ASSERT(nEnd != 0);

			// Determine normalized position
			MovementPath::Timestamp nPos = static_cast<MovementPath::Timestamp>(timestamp - t1->first);
			
			// Interpolate between the two values
			return t1->second.lerp(t2->second,
				static_cast<double>(nPos) / static_cast<double>(nEnd));
		}
	}

	MovementPath::MovementPath()
		: m_firstPosTimestamp(std::numeric_limits<int>::max())	// first value will definitly be smaller
		, m_lastPosTimestamp(std::numeric_limits<int>::min())	// use the minimum value of course.
	{
	}

	MovementPath::~MovementPath()
	{
	}

	void MovementPath::clear()
	{
		m_position.clear();
	}

	void MovementPath::addPosition(MovementPath::Timestamp timestamp, const math::Vector3 &position)
	{
		// Save some min/max values
		if (timestamp < m_firstPosTimestamp)
			m_firstPosTimestamp = timestamp;
		if (timestamp > m_lastPosTimestamp)
			m_lastPosTimestamp = timestamp;

		// Store position info
		m_position[timestamp] = position;
	}

	math::Vector3 MovementPath::getPosition(MovementPath::Timestamp timestamp) const
	{
		// Interpolate between the two values
		return doInterpolation<PositionMap>(timestamp, m_firstPosTimestamp, m_lastPosTimestamp, m_position);
	}

	void MovementPath::printDebugInfo()
	{
		DLOG("MovementPath Debug Info");
		{
			DLOG("\tPosition Elements:\t" << m_position.size());
			for (auto it : m_position)
			{
				DLOG("\t\t" << std::setw(5) << it.first << ": " << it.second);
			}
		}
	}
}
