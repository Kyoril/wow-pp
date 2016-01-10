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
					//DLOG("Could not load map file " << file << ": File does not exist");
					return nullptr;
				}

				// Open file for reading
				std::ifstream mapFile(file.c_str(), std::ios::in | std::ios::binary);
				if (!mapFile)
				{
					return nullptr;
				}

				// Read map header
				MapHeaderChunk mapHeaderChunk;
				mapFile.read(reinterpret_cast<char*>(&mapHeaderChunk), sizeof(MapHeaderChunk));
				if (mapHeaderChunk.fourCC != 0x50414D57)
				{
					//ELOG("Could not load map file " << file << ": Invalid four-cc code!");
					return nullptr;
				}
				if (mapHeaderChunk.size != sizeof(MapHeaderChunk) - 8)
				{
					//ELOG("Could not load map file " << file << ": Unexpected header chunk size (" << (sizeof(MapHeaderChunk) - 8) << " expected)!");
					return nullptr;
				}
				if (mapHeaderChunk.version != 0x110)
				{
					//ELOG("Could not load map file " << file << ": Unsupported file format version!");
					return nullptr;
				}

				tile.reset(new MapDataTile);

				// Read area table
				mapFile.seekg(mapHeaderChunk.offsAreaTable, std::ios::beg);

				// Create new tile and read area data
				mapFile.read(reinterpret_cast<char*>(&tile->areas), sizeof(MapAreaChunk));
				if (tile->areas.fourCC != 0x52414D57 || tile->areas.size != sizeof(MapAreaChunk) - 8)
				{
					//WLOG("Map file " << file << " seems to be corrupted: Wrong area chunk");
					//TODO: Should we cancel the loading process?
				}
			
				// Read collision data
				if (mapHeaderChunk.offsCollision)
				{
					mapFile.seekg(mapHeaderChunk.offsCollision, std::ios::beg);

					// Read collision header
					mapFile.read(reinterpret_cast<char*>(&tile->collision.fourCC), sizeof(UInt32));
					mapFile.read(reinterpret_cast<char*>(&tile->collision.size), sizeof(UInt32));
					mapFile.read(reinterpret_cast<char*>(&tile->collision.vertexCount), sizeof(UInt32));
					mapFile.read(reinterpret_cast<char*>(&tile->collision.triangleCount), sizeof(UInt32));
					if (tile->collision.fourCC != 0x4C434D57 || tile->collision.size < sizeof(UInt32) * 4)
					{
						//WLOG("Map file " << file << " seems to be corrupted: Wrong collision chunk (Size: " << tile->collision.size);
						//TODO: Should we cancel the loading process?
					}

					// Read all vertices
					tile->collision.vertices.resize(tile->collision.vertexCount);
					size_t numBytes = sizeof(float) * 3 * tile->collision.vertexCount;
					mapFile.read(reinterpret_cast<char*>(tile->collision.vertices.data()), numBytes);

					// Read all indices
					tile->collision.triangles.resize(tile->collision.triangleCount);
					mapFile.read(reinterpret_cast<char*>(tile->collision.triangles.data()), sizeof(Triangle) * tile->collision.triangleCount);
				}

				/*
				// Read height data
				mapFile.seekg(mapHeaderChunk.offsHeight, std::ios::beg);

				// Create new tile and read area data
				mapFile.read(reinterpret_cast<char*>(&tile->heights), sizeof(MapHeightChunk));
				if (tile->heights.fourCC != 0x54484D57 || tile->heights.size != sizeof(MapHeightChunk) - 8)
				{
					WLOG("Map file " << file << " might be corrupted and may contain corrupt data");
					//TODO: Should we cancel the loading process?
				}
				*/
			}

			return tile.get();
		}

		return nullptr;
	}

	float Map::getHeightAt(float x, float y)
	{
		return 0.0f;
	}

	bool Map::isInLineOfSight(const math::Vector3 & posA, const math::Vector3 & posB)
	{
		if (posA == posB)
			return true;

		// Calculate grid x coordinates
		TileIndex2D startTileIdx;
		startTileIdx[0] = static_cast<Int32>(floor((32.0 - (static_cast<double>(posA.x) / 533.3333333))));
		startTileIdx[1] = static_cast<Int32>(floor((32.0 - (static_cast<double>(posA.y) / 533.3333333))));

		auto *startTile = getTile(startTileIdx);
		if (!startTile)
		{
			// Unable to get / load tile - always in line of sight
			return true;
		}

		if (startTile->collision.triangleCount == 0)
		{
			return true;
		}

		// Create a ray
		math::Ray ray(posA, posB);

		// Test: Check every triangle (TODO: Use octree nodes)
		for (const auto &triangle : startTile->collision.triangles)
		{
			auto &vA = startTile->collision.vertices[triangle.indexA];
			auto &vB = startTile->collision.vertices[triangle.indexB];
			auto &vC = startTile->collision.vertices[triangle.indexC];
			auto result = ray.intersectsTriangle(vA, vB, vC);
			if (!result.first)
				return false;
		}

		// Target is in line of sight
		return true;
	}
}
