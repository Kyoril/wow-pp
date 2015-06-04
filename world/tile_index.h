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
#include "common/vector.h"

namespace wowpp
{
	typedef Int32 TileIndex;
	typedef Vector<TileIndex, 2> TileIndex2D;
	typedef Vector<TileIndex2D, 2> TileIndex2DPair;

	/// Determines whether a tile is inside a tile area.
	/// @param point The tile coordinates.
	/// @param rect The tile area. First coordinate is the top-left tile, second is the bottom-right tile.
	static inline bool isInside(
		const TileIndex2D &point,
		const TileIndex2DPair &rect)
	{
		const auto &topLeft = rect[0];
		const auto &bottomRight = rect[1];

		return
			(point.x() >= topLeft.x()) &&
			(point.y() >= topLeft.y()) &&
			(point.x() < bottomRight.x()) &&
			(point.y() < bottomRight.y());
	}

	/// Builds a tile area pair based of a center tile and a tile range.
	/// @param center The tile coordinate of the center.
	/// @param range The range in tile indices.
	static inline TileIndex2DPair getTileAreaAround(
		const TileIndex2D &center,
		TileIndex range)
	{
		const TileIndex2D topLeft(
			center.x() - range,
			center.y() - range);

		const TileIndex2D bottomRight(
			center.x() + range + 1,
			center.y() + range + 1);

		return TileIndex2DPair(topLeft, bottomRight);
	}
}
