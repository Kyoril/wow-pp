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

#include "tiled_unit_finder.h"
#include "tiled_unit_watcher.h"
#include "tiled_unit_finder_tile.h"
#include "data/map_entry.h"
#include "game_unit.h"
#include "common/make_unique.h"
#include "log/default_log_levels.h"
#include <cassert>

namespace wowpp
{
	namespace
	{
		size_t getGridLength(game::Distance worldLength, game::Distance tileWidth)
		{
			return std::max<size_t>(1, static_cast<size_t>(worldLength / tileWidth) * 64);
		}

		game::Position getUnitPosition(const GameUnit &unit)
		{
			float o;
			game::Position pos;
			unit.getLocation(pos[0], pos[1], pos[2], o);
			
			return pos;
		}
	}

	TiledUnitFinder::TiledUnitFinder(const MapEntry &map, game::Distance tileWidth)
		: UnitFinder(map)
		, m_grid(getGridLength(533.33333f, tileWidth), getGridLength(533.33333f, tileWidth))
		, m_tileWidth(tileWidth)
	{
	}

	void TiledUnitFinder::addUnit(GameUnit &findable)
	{
		assert(m_units.count(&findable) == 0);
		const game::Position unitPos = getUnitPosition(findable);
		const auto position = getTilePosition(game::planar(unitPos));
		auto &tile = m_grid(position[0], position[1]);
		tile.addUnit(findable);

		if (findable.getTypeId() == object_type::Character)
		{
			DLOG("Player added to tile " << position[0] << ", " << position[1]);
		}

		UnitRecord &record = *m_units.insert(std::make_pair(&findable, make_unique<UnitRecord>())).first->second;
		record.moved = findable.moved.connect([this, &findable](GameObject &obj, float x, float y, float z, float o)
		{
			this->onUnitMoved(findable);
		});
		record.lastTile = &tile;
	}

	void TiledUnitFinder::removeUnit(GameUnit &findable)
	{
		const auto i = m_units.find(&findable);
		assert(i != m_units.end());
		UnitRecord &record = *i->second;
		record.lastTile->removeUnit(findable);
		m_units.erase(i);
	}

	void TiledUnitFinder::updatePosition(GameUnit &updated,
	                                        const game::Position &previousPos)
	{
	}

	void TiledUnitFinder::findUnits(
	    const Circle &shape,
	    const std::function<bool (GameUnit &)> &resultHandler) const
	{
		const auto boundingBox = shape.getBoundingRect();
		const auto topLeft = getTilePosition(boundingBox[0]);
		const auto bottomRight = getTilePosition(boundingBox[1]);

		Tile::UnitSet iterationCopyTile;

		for (auto x = topLeft[0]; x <= bottomRight[0]; ++x)
		{
			for (auto y = topLeft[1]; y <= bottomRight[1]; ++y)
			{
				iterationCopyTile = getTile(TileIndex2D(x, y)).getUnits();

				for (GameUnit * const element : iterationCopyTile.getElements())
				{
					assert(element);
					const game::Position elementPos = getUnitPosition(*element);
					if (shape.isPointInside(game::planar(elementPos)))
					{
						if (!resultHandler(*element))
						{
							return;
						}
					}
				}
			}
		}
	}

	std::unique_ptr<UnitWatcher> TiledUnitFinder::watchUnits(const Circle &shape)
	{
		return make_unique<TiledUnitWatcher>(shape, *this);
	}


	TiledUnitFinder::Tile &TiledUnitFinder::getTile(const TileIndex2D &position)
	{
		return m_grid(position[0], position[1]);
	}

	const TiledUnitFinder::Tile &TiledUnitFinder::getTile(const TileIndex2D &position) const
	{
		return m_grid(position[0], position[1]);
	}

	TileIndex2D TiledUnitFinder::getTilePosition(const Vector<game::Distance, 2> &point) const
	{
		TileIndex2D output;

		// Calculate grid coordinates
		output[0] = static_cast<TileIndex>(floor((static_cast<double>(m_grid.width()) * 0.5 - (static_cast<double>(point[0]) / m_tileWidth))));
		output[1] = static_cast<TileIndex>(floor((static_cast<double>(m_grid.height()) * 0.5 - (static_cast<double>(point[1]) / m_tileWidth))));

		return output;
	}

	TiledUnitFinder::Tile &TiledUnitFinder::getUnitsTile(const GameUnit &findable)
	{
		const game::Position position = getUnitPosition(findable);
		const TileIndex2D index = getTilePosition(game::planar(position));
		return getTile(index);
	}

	void TiledUnitFinder::onUnitMoved(GameUnit &findable)
	{
		Tile &currentTile = getUnitsTile(findable);
		UnitRecord &record = requireRecord(findable);
		if (&currentTile == record.lastTile)
		{
			(*currentTile.moved)(findable);
			return;
		}

		//TODO optimize
		removeUnit(findable);
		addUnit(findable);
	}

	TiledUnitFinder::UnitRecord &TiledUnitFinder::requireRecord(GameUnit &findable)
	{
		const auto i = m_units.find(&findable);
		assert(i != m_units.end());
		UnitRecord &record = *i->second;
		return record;
	}
}
