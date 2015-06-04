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

#include "solid_visibility_grid.h"
#include "visibility_tile.h"
#include <cassert>

namespace wowpp
{
	namespace constants
	{
		static const float MapWidth = 533.3333f;
		static const size_t MapZonesInParallel = 16;
		static const size_t ZonesPerMap = MapZonesInParallel * MapZonesInParallel;
		static const float ZoneWidth = MapWidth / MapZonesInParallel;
	}

	namespace
	{
		size_t getGridLength(size_t worldWidth, float tileWidth)
		{
			return std::max<size_t>(1,
				static_cast<size_t>(static_cast<float>(worldWidth)* constants::MapWidth / tileWidth));
		}
	}

	SolidVisibilityGrid::SolidVisibilityGrid(const TileIndex2D &worldSize)
		: VisibilityGrid()
		, m_tiles(getGridLength(worldSize[0], constants::MapWidth / 16.0f), getGridLength(worldSize[1], constants::MapWidth / 16.0f))
	{
		for (size_t y = 0; y < m_tiles.height(); ++y)
		{
			for (size_t x = 0; x < m_tiles.width(); ++x)
			{
				m_tiles(x, y).setPosition(TileIndex2D(x, y));
			}
		}
	}

	SolidVisibilityGrid::~SolidVisibilityGrid()
	{
	}

	VisibilityTile *SolidVisibilityGrid::getTile(const TileIndex2D &position)
	{
		if ((position[0] >= 0) &&
			(position[0] < static_cast<TileIndex>(m_tiles.width()) &&
			(position[1] >= 0) &&
			(position[1] < static_cast<TileIndex>(m_tiles.height()))))
		{
			return &m_tiles(position[0], position[1]);
		}

		return nullptr;
	}
	VisibilityTile &SolidVisibilityGrid::requireTile(const TileIndex2D &position)
	{
		assert(m_tiles.width());
		assert(m_tiles.height());

		const auto x = limit<TileIndex>(position[0], 0, m_tiles.width() - 1);
		const auto y = limit<TileIndex>(position[1], 0, m_tiles.height() - 1);
		return m_tiles(x, y);
	}
}
