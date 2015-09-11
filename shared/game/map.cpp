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

#include "map.h"
#include "data/map_entry.h"
#include "detour_world_navigation.h"
#include "log/default_log_levels.h"
#include "common/make_unique.h"

namespace wowpp
{
	Map::Map(const MapEntry &entry, boost::filesystem::path dataPath)
		: m_entry(entry)
		, m_dataPath(std::move(dataPath))
		, m_tiles(64, 64)
	{
	}

	MapDataTile * Map::getTile(const TileIndex2D &position)
	{
		if ((position[0] >= 0) &&
			(position[0] < static_cast<TileIndex>(m_tiles.width()) &&
			(position[1] >= 0) &&
			(position[1] < static_cast<TileIndex>(m_tiles.height()))))
		{
			auto &tile = m_tiles(position[0], position[1]);

			std::ostringstream strm;
			strm << m_dataPath.string() << "/maps/" << m_entry.id << "/" << position[0] << "_" << position[1] << ".map";

			const String file = strm.str();
			if (!boost::filesystem::exists(file))
			{
				// File does not exist
				DLOG("Could not load map file " << file << ": File does not exist");
				return nullptr;
			}

			// Open file for reading
			std::ifstream mapFile(file.c_str(), std::ios::in);
			if (!mapFile)
			{
				return nullptr;
			}

			// Read map header
			MapHeaderChunk mapHeaderChunk;
			mapFile.read(reinterpret_cast<char*>(&mapHeaderChunk), sizeof(MapHeaderChunk));
			if (mapHeaderChunk.fourCC != 0x50414D57)
			{
				ELOG("Could not load map file " << file << ": Invalid four-cc code!");
				return nullptr;
			}
			if (mapHeaderChunk.size != sizeof(MapHeaderChunk) - 8)
			{
				ELOG("Could not load map file " << file << ": Unexpected header chunk size (" << (sizeof(MapHeaderChunk) - 8) << " expected)!");
				return nullptr;
			}
			if (mapHeaderChunk.version != 0x100)
			{
				ELOG("Could not load map file " << file << ": Unsupported file format version!");
				return nullptr;
			}

			// Read area table (ignore the rest for now)
			mapFile.seekg(mapHeaderChunk.offsAreaTable, std::ios::beg);

			// Create new tile and read area data
			mapFile.read(reinterpret_cast<char*>(&tile.areas), sizeof(MapAreaChunk));
			if (tile.areas.fourCC != 0x52414D57 || tile.areas.size != sizeof(MapAreaChunk) - 8)
			{
				WLOG("Map file " << file << " might be corrupted and may contain corrupt data");
				//TODO: Should we cancel the loading process?
			}

			return &tile;
		}

		return nullptr;
	}
}
