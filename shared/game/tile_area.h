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
#include "tile_index.h"

namespace wowpp
{
	namespace constants
	{
		static const size_t PlayerZoneSight = 2;
		static const size_t PlayerScopeWidth = 1 + 2 * PlayerZoneSight;
		static const size_t PlayerScopeAreaCount = PlayerScopeWidth * PlayerScopeWidth;
		static const size_t PlayerScopeSurroundingAreaCount = PlayerScopeAreaCount - 1;
	}

	class TileArea
	{
	public:
		TileIndex2D topLeft, bottomRight;

		TileArea()
		{
		}
		explicit TileArea(const TileIndex2D &topLeft, const TileIndex2D &bottomRight)
			: topLeft(topLeft)
			, bottomRight(bottomRight)
		{
		}

		inline bool isInside(const TileIndex2D &point) const
		{
			return
				(point.x() >= topLeft.x()) &&
				(point.y() >= topLeft.y()) &&
				(point.x() < bottomRight.x()) &&
				(point.y() < bottomRight.y());
		}
	};

	inline TileArea getSightArea(const TileIndex2D &center)
	{
		static const TileIndex2D sight(constants::PlayerZoneSight, constants::PlayerZoneSight);
		return TileArea(
			center - sight,
			center + sight);
	}
}
