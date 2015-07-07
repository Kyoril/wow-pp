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
#include "visibility_tile.h"
#include "visibility_grid.h"
#include "tile_visibility.h"
#include "tile_visibility_change.h"
#include "game_object.h"
#include <boost/iterator/indirect_iterator.hpp>
#include <vector>

namespace wowpp
{
	template <class T, class OnTile, class OnSubscriber>
	static void forEachTileAndEachSubscriber(
		T tilesBegin,
		T tilesEnd,
		const OnTile &onTile,
		const OnSubscriber &onSubscriber)
	{
		for (T t = tilesBegin; t != tilesEnd; ++t)
		{
			auto &tile = *t;
			onTile(tile);

			for (ITileSubscriber * const subscriber : tile.getWatchers().getElements())
			{
				onSubscriber(*subscriber);
			}
		}
	}

	static inline std::vector<VisibilityTile *> getTilesInSight(
		VisibilityGrid &grid,
		const TileIndex2D &center)
	{
		std::vector<VisibilityTile *> tiles;

		for (TileIndex y = center.y() - constants::PlayerZoneSight;
			y <= static_cast<TileIndex>(center.y() + constants::PlayerZoneSight); ++y)
		{
			for (TileIndex x = center.x() - constants::PlayerZoneSight;
				x <= static_cast<TileIndex>(center.x() + constants::PlayerZoneSight); ++x)
			{
				auto *const tile = grid.getTile(TileIndex2D(x, y));

				if (tile)
				{
					tiles.push_back(tile);
				}
			}
		}

		return tiles;
	}

	template <class T, class OnSubscriber>
	static void forEachSubscriber(
		T tilesBegin,
		T tilesEnd,
		const OnSubscriber &onSubscriber)
	{
		forEachTileAndEachSubscriber(
			tilesBegin,
			tilesEnd,
			[](VisibilityTile &) {},
			onSubscriber);
	}

	template <class OnSubscriber>
	void forEachSubscriberInSight(
		VisibilityGrid &grid,
		const TileIndex2D &center,
		const OnSubscriber &onSubscriber)
	{
		const auto tiles = getTilesInSight(grid, center);
		forEachSubscriber(
			boost::make_indirect_iterator(tiles.begin()),
			boost::make_indirect_iterator(tiles.end()),
			onSubscriber);
	}

	static inline bool isInSight(
		const TileIndex2D &first,
		const TileIndex2D &second)
	{
		const auto diff = abs(second - first);
		return
			(static_cast<size_t>(diff.x()) <= constants::PlayerZoneSight) &&
			(static_cast<size_t>(diff.y()) <= constants::PlayerZoneSight);
	}

	template <class OnTile>
	void forEachTileInSightWithout(
		VisibilityGrid &grid,
		const TileIndex2D &center,
		const TileIndex2D &excluded,
		const OnTile &onTile
		)
	{
		for (TileIndex y = center.y() - constants::PlayerZoneSight;
			y <= static_cast<TileIndex>(center.y() + constants::PlayerZoneSight); ++y)
		{
			for (TileIndex x = center.x() - constants::PlayerZoneSight;
				x <= static_cast<TileIndex>(center.x() + constants::PlayerZoneSight); ++x)
			{
				auto *const tile = grid.getTile(TileIndex2D(x, y));

				if (tile &&
					!isInSight(excluded, tile->getPosition()))
				{
					onTile(*tile);
				}
			}
		}
	}
}
