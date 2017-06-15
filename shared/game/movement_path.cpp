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
		template<typename T = MovementPath::PositionMap>
		static math::Vector3 doInterpolation(MovementPath::Timestamp timestamp, const T &map)
		{
			// No data! What should we do? ...
			if (map.empty())
				return math::Vector3();

			// Check if timestamp is less than the end point
			auto endIt = map.rbegin();
			if (endIt->first < timestamp)
				return endIt->second;

			// Get start and end iterator
			typename T::const_iterator t1 = map.end(), t2 = map.end();

			// Get first position and check if it is ahead in time or just the right time
			typename T::const_iterator it = map.begin();
			if (it->first >= timestamp)
				return it->second;

			MovementPath::Timestamp startTime = 0;

			// Iterate through all saved positions
			while (it != map.end())
			{
				// Check if this position lies in the past
				if (it->first < timestamp &&
					it->first > startTime)
				{
					t1 = it;
					startTime = it->first;
				}

				// If we have a valid start point, and don't have an end point yet, we can
				// use it as the target point
				if (t1 != map.end() &&
					t2 == map.end())
				{
					if (it->first >= timestamp)
					{
						t2 = it;
						break;
					}
				}

				// Next
				it++;
			}

			// By now we should have a valid start value
			ASSERT(t1 != map.end());
			ASSERT(t2 != map.end());
			
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
		: m_firstPosTimestamp(std::numeric_limits<MovementPath::Timestamp>::max())	// first value will definitly be smaller
		, m_lastPosTimestamp(0)	// use the minimum value of course.
	{
	}

	MovementPath::~MovementPath()
	{
	}

	void MovementPath::clear()
	{
		m_position.clear();
		m_firstPosTimestamp = std::numeric_limits<MovementPath::Timestamp>::max();
		m_lastPosTimestamp = 0;
	}

	void MovementPath::addPosition(MovementPath::Timestamp timestamp, const math::Vector3 &position)
	{
		// Save some min/max values
		if (timestamp < m_firstPosTimestamp)
			m_firstPosTimestamp = timestamp;
		if (timestamp > m_lastPosTimestamp)
			m_lastPosTimestamp = timestamp;

		ASSERT(m_lastPosTimestamp >= m_firstPosTimestamp);

		// Store position info
		m_position[timestamp] = position;
	}

	math::Vector3 MovementPath::getPosition(MovementPath::Timestamp timestamp) const
	{
		// Interpolate between the two values
		return doInterpolation<PositionMap>(timestamp, m_position);
	}

	void MovementPath::printDebugInfo()
	{
		DLOG("MovementPath Debug Info");
		{
			DLOG("\tPosition Elements:\t" << m_position.size());
			Timestamp prev = 0;
			for (auto it : m_position)
			{
				Timestamp diff = 0;
				if (prev > 0) diff = it.first - prev;
				prev = it.first;

				DLOG("\t\t" << std::setw(5) << it.first << ": " << it.second << " [Duration: " << diff << "]");
			}
		}
	}
}
