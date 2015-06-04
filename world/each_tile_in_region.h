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

#include "visibility_tile.h"

namespace wowpp
{
	class VisibilityGrid;

	template <class F>
	void forEachTileInArea(
		VisibilityGrid &grid,
		const TileIndex2DPair &area,
	    const F &function)
	{
		const auto &topLeft = area[0];
		const auto &bottomRight = area[1];

		for (TileIndex y = topLeft.y(); y < bottomRight.y(); ++y)
		{
			for (TileIndex x = topLeft.x(); x < bottomRight.x(); ++x)
			{
				auto *const tile = grid.getTile(TileIndex2D(x, y));
				if (tile)
				{
					function(*tile);
				}
			}
		}
	}

	template <class F>
	void forEachTileInAreaWithout(
	    VisibilityGrid &grid,
		const TileIndex2DPair &area,
		const TileIndex2DPair &without,
	    const F &function)
	{
		forEachTileInArea(grid, area,
		                  [&without, &function](VisibilityTile & tile)
		{
			if (!isInside(tile.getPosition(), without))
			{
				function(tile);
			}
		});
	}

	template <class F>
	void forEachTileInRange(
	    VisibilityGrid &grid,
		const TileIndex2D &center,
	    size_t range,
	    const F &function)
	{
		const auto area = getTileAreaAround(center, range);
		forEachTileInArea(
		    grid,
		    area,
		    function);
	}

	template <class F>
	void forEachTileInSight(
	    VisibilityGrid &grid,
	    const TileIndex2D &center,
	    const F &function)
	{
		forEachTileInRange(
		    grid,
		    center,
		    constants::PlayerZoneSight,
		    function);
	}
}
