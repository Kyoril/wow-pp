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

#include "tiled_unit_watcher.h"
#include "tiled_unit_finder_tile.h"
#include "game_unit.h"
#include <cassert>
#include "log/default_log_levels.h"

namespace wowpp
{
	TiledUnitFinder::TiledUnitWatcher::TiledUnitWatcher(
	        const Circle &shape,
			TiledUnitFinder &finder)
		: UnitWatcher(shape)
		, m_finder(finder)
	{
	}

	TiledUnitFinder::TiledUnitWatcher::~TiledUnitWatcher()
	{
		for (const auto & conn : m_connections)
		{
			conn.second.disconnect();
		}
	}

	void TiledUnitFinder::TiledUnitWatcher::start()
	{
		const auto shapeArea = getTileIndexArea(getShape());
		assert(shapeArea.topLeft[1] <= shapeArea.bottomRight[1]);
		assert(shapeArea.topLeft[0] <= shapeArea.bottomRight[0]);

		for (TileIndex y = shapeArea.topLeft[1]; y <= shapeArea.bottomRight[1]; ++y)
		{
			for (TileIndex x = shapeArea.topLeft[0]; x <= shapeArea.bottomRight[0]; ++x)
			{
				auto &tile = m_finder.getTile(TileIndex2D(x, y));
				if (watchTile(tile))
				{
					return;
				}
			}
		}
	}

	TileArea TiledUnitFinder::TiledUnitWatcher::getTileIndexArea(const Circle &shape) const
	{
		const auto boundingBox = shape.getBoundingRect();
		// WoW's Coordinate System sucks... topLeft >= bottomRight
		const auto topLeft     = m_finder.getTilePosition(boundingBox[1]);
		const auto bottomRight = m_finder.getTilePosition(boundingBox[0]);
		return TileArea(topLeft, bottomRight);
	}

	bool TiledUnitFinder::TiledUnitWatcher::watchTile(Tile &tile)
	{
		assert(m_connections.count(&tile) == 0);

		auto connection = tile.moved->connect(
			std::bind(&TiledUnitWatcher::onUnitMoved,
			  this, std::placeholders::_1));

		m_connections[&tile] = connection;

		for (GameUnit * const unit : tile.getUnits().getElements())
		{
			math::Vector3 location(unit->getLocation());

			if (getShape().isPointInside(game::Point(location.x, location.y)))
			{
				if (visibilityChanged(*unit, true))
				{
					return true;
				}
			}
		}

		return false;
	}

	bool TiledUnitFinder::TiledUnitWatcher::unwatchTile(Tile &tile)
	{
		assert(m_connections.count(&tile) == 1);

		{
			const auto i = m_connections.find(&tile);
			i->second.disconnect();
			m_connections.erase(i);
		}

		for (GameUnit * const unit : tile.getUnits().getElements())
		{
			math::Vector3 location(unit->getLocation());

			if (getShape().isPointInside(game::Point(location.x, location.y)))
			{
				if (visibilityChanged(*unit, false))
				{
					return true;
				}
			}
		}

		return false;
	}

	void TiledUnitFinder::TiledUnitWatcher::onUnitMoved(GameUnit &unit)
	{
		math::Vector3 location(unit.getLocation());

		const bool isInside = getShape().isPointInside(game::Point(location.x, location.y));
		visibilityChanged(unit, isInside);
	}

	bool TiledUnitFinder::TiledUnitWatcher::updateTile(Tile &tile)
	{
		for (GameUnit * const unit : tile.getUnits().getElements())
		{
			math::Vector3 location(unit->getLocation());

			const auto planarPos = game::Point(location.x, location.y);
			const bool wasInside = m_previousShape.isPointInside(planarPos);
			const bool isInside = getShape().isPointInside(planarPos);

			if (wasInside != isInside)
			{
				if (visibilityChanged(*unit, isInside))
				{
					return true;
				}
			}
		}

		return false;
	}

	void TiledUnitFinder::TiledUnitWatcher::onShapeUpdated()
	{
		const auto previousArea = getTileIndexArea(m_previousShape);
		const auto currentArea = getTileIndexArea(getShape());

		for (TileIndex y = previousArea.topLeft[1]; y <= previousArea.bottomRight[1]; ++y)
		{
			for (TileIndex x = previousArea.topLeft[0]; x <= previousArea.bottomRight[0]; ++x)
			{
				const TileIndex2D pos(x, y);
				auto &tile = m_finder.getTile(pos);

				if (currentArea.isInside(pos))
				{
					if (updateTile(tile))
					{
						return;
					}
				}
				else
				{
					if (unwatchTile(tile))
					{
						return;
					}
				}
			}
		}

		for (TileIndex y = currentArea.topLeft[1]; y <= currentArea.bottomRight[1]; ++y)
		{
			for (TileIndex x = currentArea.topLeft[0]; x <= currentArea.bottomRight[0]; ++x)
			{
				const TileIndex2D pos(x, y);
				auto &tile = m_finder.getTile(pos);

				if (previousArea.isInside(pos))
				{
					if (updateTile(tile))
					{
						return;
					}
				}
				else
				{
					if (watchTile(tile))
					{
						return;
					}
				}
			}
		}

		m_previousShape = getShape();
	}
}
