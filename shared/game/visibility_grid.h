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
#include "tile_area.h"
#include "math/vector3.h"

namespace wowpp
{
	class VisibilityTile;

	/// Base class of a visibility grid. A visibility grid manages objects in a world
	/// instance and decides, which objects are visible for other objects.
	class VisibilityGrid
	{
	public:
		explicit VisibilityGrid();
		virtual ~VisibilityGrid();

		bool getTilePosition(const math::Vector3 &position, Int32 &outX, Int32 &outY) const;
		virtual VisibilityTile *getTile(const TileIndex2D &position) = 0;
		virtual VisibilityTile &requireTile(const TileIndex2D &position) = 0;
	};

	template <class Handler>
	void forEachTileInArea(
	    VisibilityGrid &grid,
	    const TileArea &area,
	    const Handler &handler)
	{
		for (TileIndex z = area.topLeft[1]; z <= area.bottomRight[1]; ++z)
		{
			for (TileIndex x = area.topLeft[0]; x <= area.bottomRight[0]; ++x)
			{
				auto *const tile = grid.getTile(TileIndex2D(x, z));
				if (tile)
				{
					handler(*tile);
				}
			}
		}
	}

	template <class OutputIterator>
	void copyTilePtrsInArea(
	    VisibilityGrid &grid,
	    const TileArea &area,
	    OutputIterator &dest)
	{
		forEachTileInArea(grid, area,
		                  [&dest](VisibilityTile & tile)
		{
			*dest = &tile;
			++dest;
		});
	}
}
