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
#include <boost/filesystem.hpp>
#include "world_navigation.h"
#include "tile_index.h"
#include "common/grid.h"

namespace wowpp
{
	/// Header
	struct MapHeaderChunk
	{
		UInt32 fourCC;
		UInt32 size;
		UInt32 version;
		UInt32 offsAreaTable;
		UInt32 areaTableSize;
		UInt32 offsHeight;
		UInt32 heightSize;
	};

	/// Map area chunk.
	struct MapAreaChunk
	{
		UInt32 fourCC;
		UInt32 size;
		struct AreaInfo
		{
			UInt32 areaId;
			UInt32 flags;

			///
			AreaInfo()
				: areaId(0)
				, flags(0)
			{
			}
		};
		std::array<AreaInfo, 16 * 16> cellAreas;
	};

	/// Map size chunk.
	struct MapHeightChunk
	{
		UInt32 fourCC;
		UInt32 size;
		std::array<std::array<float, 145>, 16 * 16> heights;
	};

	struct MapEntry;

	/// Stores map-specific tiled data informations like nav mesh data, height maps and such things.
	struct MapDataTile final
	{
		MapAreaChunk areas;
	};

	/// This class represents a map with additional geometry and navigation data.
	class Map final
	{
	public:

		/// Creates a new instance of the map class and initializes it.
		/// @entry The base entry of this map.
		explicit Map(const MapEntry &entry, boost::filesystem::path dataPath);

		/// Gets the map entry data of this map.
		const MapEntry &getEntry() const { return m_entry; }

		/// Tries to get a specific data tile if it's loaded.
		MapDataTile *getTile(const TileIndex2D &position);

	private:

		const MapEntry &m_entry;
		const boost::filesystem::path m_dataPath;
		Grid<MapDataTile> m_tiles;
	};
}
