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

#include "tiled_unit_watcher.h"
#include "tiled_unit_finder_tile.h"
#include "game_unit.h"
#include <cassert>

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

		for (TileIndex z = shapeArea.topLeft[1]; z <= shapeArea.bottomRight[1]; ++z)
		{
			for (TileIndex x = shapeArea.topLeft[0]; x <= shapeArea.bottomRight[0]; ++x)
			{
				auto &tile = m_finder.getTile(TileIndex2D(x, z));
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
		const auto topLeft     = m_finder.getTilePosition(boundingBox[0]);
		const auto bottomRight = m_finder.getTilePosition(boundingBox[1]);
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
			float x, y, z, o;
			unit->getLocation(x, y, z, o);

			if (getShape().isPointInside(game::Point(x, y)))
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
			float x, y, z, o;
			unit->getLocation(x, y, z, o);

			if (getShape().isPointInside(game::Point(x, y)))
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
		float x, y, z, o;
		unit.getLocation(x, y, z, o);

		if (getShape().isPointInside(game::Point(x, y)))
		visibilityChanged(unit, getShape().isPointInside(game::Point(x, y)));
	}

	bool TiledUnitFinder::TiledUnitWatcher::updateTile(Tile &tile)
	{
		for (GameUnit * const unit : tile.getUnits().getElements())
		{
			float x, y, z, o;
			unit->getLocation(x, y, z, o);

			const auto planarPos = game::Point(x, y);
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
