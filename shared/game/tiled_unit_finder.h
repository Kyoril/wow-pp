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

#include "unit_finder.h"
#include "tile_index.h"
#include "common/grid.h"
#include "defines.h"
#include "common/simple.hpp"

namespace wowpp
{
	class TiledUnitFinder final : public UnitFinder
	{
	public:

		explicit TiledUnitFinder(const proto::MapEntry &map, game::Distance tileWidth);
		virtual void addUnit(GameUnit &findable) override;
		virtual void removeUnit(GameUnit &findable) override;
		virtual void updatePosition(GameUnit &updated,
		                            const math::Vector3 &previousPos) override;
		virtual void findUnits(const Circle &shape,
		                       const std::function<bool(GameUnit &)> &resultHandler) override;
		virtual std::unique_ptr<UnitWatcher> watchUnits(const Circle &shape) override;

	private:

		class TiledUnitWatcher;
		class Tile;

		struct UnitRecord
		{
			simple::scoped_connection moved;
			Tile *lastTile;
		};

		//TODO: avoid the unique_ptr
		typedef std::unordered_map<GameUnit *, std::unique_ptr<UnitRecord>> UnitRecordsByIdentity;
		typedef Grid<std::unique_ptr<Tile>> TileGrid;

		TileGrid m_grid;
		UnitRecordsByIdentity m_units;
		const game::Distance m_tileWidth;

		Tile &getTile(const TileIndex2D &position);
		//const Tile &getTile(const TileIndex2D &position) const;
		TileIndex2D getTilePosition(const Vector<game::Distance, 2> &point) const;
		Tile &getUnitsTile(const GameUnit &findable);
		void onUnitMoved(GameUnit &findable);
		UnitRecord &requireRecord(GameUnit &findable);
	};
}
