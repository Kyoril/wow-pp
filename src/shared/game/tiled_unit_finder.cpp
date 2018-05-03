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
#include "tiled_unit_finder.h"
#include "tiled_unit_watcher.h"
#include "tiled_unit_finder_tile.h"
#include "game_unit.h"
#include "common/make_unique.h"
#include "log/default_log_levels.h"

namespace wowpp
{
	namespace
	{
		size_t getFinderGridLength(game::Distance worldLength, game::Distance tileWidth)
		{
			return std::max<size_t>(1, static_cast<size_t>(worldLength / tileWidth) * 64);
		}
	}

	TiledUnitFinder::TiledUnitFinder(const proto::MapEntry &map, game::Distance tileWidth)
		: UnitFinder(map)
		, m_grid(getFinderGridLength(533.33333f, tileWidth), getFinderGridLength(533.33333f, tileWidth))
		, m_tileWidth(tileWidth)
	{
	}

	void TiledUnitFinder::addUnit(GameUnit &findable)
	{
		ASSERT(m_units.count(&findable) == 0);
		const math::Vector3 &unitPos = findable.getLocation();
		const auto position = getTilePosition(game::planar(unitPos));
		auto &tile = m_grid(position[0], position[1]);

		if (!tile) {
			tile.reset(new Tile());
		}
		tile->addUnit(findable);

		UnitRecord &record = *m_units.insert(std::make_pair(&findable, make_unique<UnitRecord>())).first->second;
		/*record.moved = findable.moved.connect([this, &findable](GameObject & obj, math::Vector3 position, float o)
		{
			if (findable.getLocation() != position)
			{
				onUnitMoved(findable);
			}
		});*/
		record.lastTile = tile.get();
	}

	void TiledUnitFinder::removeUnit(GameUnit &findable)
	{
		const auto i = m_units.find(&findable);
		ASSERT(i != m_units.end());
		UnitRecord &record = *i->second;
		record.lastTile->removeUnit(findable);
		m_units.erase(i);
	}

	void TiledUnitFinder::updatePosition(GameUnit &updated, const math::Vector3 &previousPos)
	{
		if (updated.getLocation() != previousPos)
		{
			onUnitMoved(updated);
		}
	}

	void TiledUnitFinder::findUnits(
	    const Circle &shape,
	    const std::function<bool (GameUnit &)> &resultHandler)
	{
		const auto boundingBox = shape.getBoundingRect();
		auto topLeft = getTilePosition(boundingBox[1]);
		auto bottomRight = getTilePosition(boundingBox[0]);

		Tile::UnitSet iterationCopyTile;

		// Crash protection
		if (topLeft[0] < 0) {
			topLeft[0] = 0;
		}
		if (topLeft[1] < 0) {
			topLeft[1] = 0;
		}
		if (bottomRight[0] >= m_grid.getSize()[0]) {
			bottomRight[0] = m_grid.getSize()[0] - 1;
		}
		if (bottomRight[1] >= m_grid.getSize()[1]) {
			bottomRight[1] = m_grid.getSize()[1] - 1;
		}

		for (auto x = topLeft[0]; x <= bottomRight[0]; ++x)
		{
			for (auto y = topLeft[1]; y <= bottomRight[1]; ++y)
			{
				iterationCopyTile = getTile(TileIndex2D(x, y)).getUnits();

				for (GameUnit *const element : iterationCopyTile.getElements())
				{
					ASSERT(element);
					const math::Vector3 &elementPos = element->getLocation();
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

	std::unique_ptr<UnitWatcher> TiledUnitFinder::watchUnits(const Circle &shape, std::function<bool(GameUnit &, bool)> visibilityChanged)
	{
		return make_unique<TiledUnitWatcher>(shape, *this, std::move(visibilityChanged));
	}

	TiledUnitFinder::Tile &TiledUnitFinder::getTile(const TileIndex2D &position)
	{
		auto &tile = m_grid(position[0], position[1]);
		if (!tile) {
			tile.reset(new Tile());
		}

		return *tile;
	}

	TileIndex2D TiledUnitFinder::getTilePosition(const Vector<game::Distance, 2> &point) const
	{
		TileIndex2D output;

		// Calculate grid coordinates
		output[0] = static_cast<TileIndex>(floor((static_cast<double>(m_grid.width()) * 0.5 - floor(static_cast<double>(point[0]) / m_tileWidth))));
		output[1] = static_cast<TileIndex>(floor((static_cast<double>(m_grid.height()) * 0.5 - floor(static_cast<double>(point[1]) / m_tileWidth))));

		return output;
	}

	TiledUnitFinder::Tile &TiledUnitFinder::getUnitsTile(const GameUnit &findable)
	{
		const math::Vector3 &position = findable.getLocation();
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
		ASSERT(i != m_units.end());
		UnitRecord &record = *i->second;
		return record;
	}
}
