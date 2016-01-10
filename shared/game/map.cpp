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

#include "map.h"
#include "shared/proto_data/maps.pb.h"
#include "detour_world_navigation.h"
#include "log/default_log_levels.h"
#include "common/make_unique.h"
#include "math/vector3.h"
#include "common/clock.h"

namespace wowpp
{
	Map::Map(const proto::MapEntry &entry, boost::filesystem::path dataPath)
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
			if (!tile)
			{
				std::ostringstream strm;
				strm << m_dataPath.string() << "/maps/" << m_entry.id() << "/" << position[0] << "_" << position[1] << ".map";

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

				tile.reset(new MapDataTile);

				// Read area table
				mapFile.seekg(mapHeaderChunk.offsAreaTable, std::ios::beg);

				// Create new tile and read area data
				mapFile.read(reinterpret_cast<char*>(&tile->areas), sizeof(MapAreaChunk));
				if (tile->areas.fourCC != 0x52414D57 || tile->areas.size != sizeof(MapAreaChunk) - 8)
				{
					WLOG("Map file " << file << " might be corrupted and may contain corrupt data");
					//TODO: Should we cancel the loading process?
				}

				// Read height data
				mapFile.seekg(mapHeaderChunk.offsHeight, std::ios::beg);

				// Create new tile and read area data
				mapFile.read(reinterpret_cast<char*>(&tile->heights), sizeof(MapHeightChunk));
				if (tile->heights.fourCC != 0x54484D57 || tile->heights.size != sizeof(MapHeightChunk) - 8)
				{
					WLOG("Map file " << file << " might be corrupted and may contain corrupt data");
					//TODO: Should we cancel the loading process?
				}
			}

			return tile.get();
		}

		return nullptr;
	}

	float Map::getHeightAt(float x, float y)
	{
		// Calculate grid x coordinates
		Int32 tileX = static_cast<Int32>(floor((512.0 - (static_cast<double>(x) / 33.3333))));
		Int32 tileY = static_cast<Int32>(floor((512.0 - (static_cast<double>(y) / 33.3333))));
		
		// Convert to adt tile
		TileIndex2D adtIndex(tileX / 16, tileY / 16);
		auto *tile = getTile(adtIndex);
		if (!tile)
			return 0.0f;

		// We are looking for p.z
		math::Vector3 p(x, y, 0.0f);

		// Determine chunk index
		const UInt32 chunkIndex = (tileY % 16) + (tileX % 16) * 16;
		if (chunkIndex >= 16 * 16)
			return 0.0f;

		// Get height map values
		auto &heights = tile->heights.heights[chunkIndex];
		//DLOG("Chunk index: " << chunkIndex << " (ADT: " << adtIndex[0] << "x" << adtIndex[1] << ")");
		return heights[0];

		// Determine the V8 index and the two V9 indices
		UInt32 v8Index = 0;
		UInt32 v9Index1 = 0;
		UInt32 v9Index2 = 0;

		const float offsX = tileX * 33.3333f;
		const float offsY = tileY * 33.3333f;

		// Determine vertices based on position
		const math::Vector3 v1(offsX, offsY, 0.0f);	float h1 = 0.0f;
		const math::Vector3 v2(offsX, offsY, 0.0f);	float h2 = 0.0f;
		const math::Vector3 v3(offsX, offsY, 0.0f);	float h3 = 0.0f;

		// Calculate vectors from point p to v1, v2 and v3
		const math::Vector3 f1 = v1 - p;
		const math::Vector3 f2 = v2 - p;
		const math::Vector3 f3 = v3 - p;

		// Calculate the areas and factors
		const float a = (v1 - v2).cross(v1 - v3).length();
		const float a1 = f2.cross(f3).length() / a;
		const float a2 = f3.cross(f1).length() / a;
		const float a3 = f1.cross(f2).length() / a;

		// Update height value
		return h1 * a1 + h2 * a2 + h3 * a3;
	}

	bool Map::isInLineOfSight(const math::Vector3 & posA, const math::Vector3 & posB)
	{
		if (posA == posB)
			return true;
		
		auto startTime = getCurrentTime();

		// Create a ray
		math::Ray ray(posA, posB);

		// TEST: Immaginary triangle at elwynn forest
		// TODO: Use real world collision triangles from game files
		math::Vector3 vA(-9033.53f, -92.0307f, 85.0f);
		math::Vector3 vB(-9047.82f, -105.309f, 85.0f);
		math::Vector3 vC(-9042.37f, -99.0209f, 99.0f);

		auto result = ray.intersectsTriangle(vA, vB, vC);
		auto endTime = getCurrentTime();
		DLOG("isInLineOfSight time: " << (endTime - startTime) << " ms");

		return !result.first;
	}
}
