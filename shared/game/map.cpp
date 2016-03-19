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
#include "map.h"
#include "shared/proto_data/maps.pb.h"
#include "log/default_log_levels.h"
#include "common/make_unique.h"
#include "math/vector3.h"
#include "common/clock.h"
#include "detour/DetourCommon.h"

namespace wowpp
{
	std::map<UInt32, std::unique_ptr<dtNavMesh, NavMeshDeleter>> Map::navMeshsPerMap;

	Map::Map(const proto::MapEntry &entry, boost::filesystem::path dataPath)
		: m_entry(entry)
		, m_dataPath(std::move(dataPath))
		, m_tiles(64, 64)
		, m_navMesh(nullptr)
		, m_navQuery(nullptr)
	{
		// Allocate navigation mesh
		auto it = navMeshsPerMap.find(entry.id());
		if (it == navMeshsPerMap.end())
		{
			ILOG("Creating navigation mesh instance");

			// Build file name
			std::ostringstream strm;
			strm << (m_dataPath / "maps").string() << "/" << m_entry.id() << ".map";

			const String file = strm.str();
			if (!boost::filesystem::exists(file))
			{
				// File does not exist
				DLOG("Could not load map file " << file << ": File does not exist");
				return;
			}

			// Open file for reading
			std::ifstream mapFile(file.c_str(), std::ios::in | std::ios::binary);
			if (!mapFile)
			{
				ELOG("Could not load map file " << file);
				return;
			}

			dtNavMeshParams params;
			memset(&params, 0, sizeof(dtNavMeshParams));
			if (!(mapFile.read(reinterpret_cast<char*>(&params), sizeof(dtNavMeshParams))))
			{
				ELOG("Map file " << file << " seems to be corrupted!");
				return;
			}

			auto navMesh = std::unique_ptr<dtNavMesh, NavMeshDeleter>(dtAllocNavMesh());
			if (!navMesh)
			{
				ELOG("Could not allocate navigation mesh!");
				return;
			}

			dtStatus result = navMesh->init(&params);
			if (!dtStatusSucceed(result))
			{
				ELOG("Could not initialize navigation mesh: " << result);
				return;
			}

			// At this point, it's just an empty mesh without tiles. Tiles will be loaded
			// when required.
			m_navMesh = navMesh.get();

			// allocate mesh query
			m_navQuery.reset(dtAllocNavMeshQuery());
			assert(m_navQuery);

			dtStatus dtResult = m_navQuery->init(m_navMesh, 1024);
			if (dtStatusFailed(dtResult))
			{
				m_navQuery.reset();
			}

			// Setup filter
			m_filter.setIncludeFlags(NAV_GROUND);

			navMeshsPerMap[entry.id()] = std::move(navMesh);
			ILOG("Navigation mesh for map " << m_entry.id() << " initialized");
		}
	}

	MapDataTile *Map::getTile(const TileIndex2D &position)
	{
		if ((position[0] >= 0) &&
		        (position[0] < static_cast<TileIndex>(m_tiles.width()) &&
		         (position[1] >= 0) &&
		         (position[1] < static_cast<TileIndex>(m_tiles.height()))))
		{
			auto &tile = m_tiles(position[0], position[1]);
			if (!tile)
			{
				tile.reset(new MapDataTile);

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
				std::ifstream mapFile(file.c_str(), std::ios::in | std::ios::binary);
				if (!mapFile)
				{
					return nullptr;
				}

				// Read map header
				MapHeaderChunk mapHeaderChunk;
				mapFile.read(reinterpret_cast<char *>(&mapHeaderChunk), sizeof(MapHeaderChunk));
				if (mapHeaderChunk.fourCC != 0x50414D57)
				{
					//ELOG("Could not load map file " << file << ": Invalid four-cc code!");
					return nullptr;
				}
				if (mapHeaderChunk.size != sizeof(MapHeaderChunk) - 8)
				{
					ELOG("Could not load map file " << file << ": Unexpected header chunk size (" << (sizeof(MapHeaderChunk) - 8) << " expected)!");
					return nullptr;
				}
				if (mapHeaderChunk.version != 0x120)
				{
					ELOG("Could not load map file " << file << ": Unsupported file format version!");
					return nullptr;
				}

				// Read area table
				mapFile.seekg(mapHeaderChunk.offsAreaTable, std::ios::beg);

				// Create new tile and read area data
				mapFile.read(reinterpret_cast<char *>(&tile->areas), sizeof(MapAreaChunk));
				if (tile->areas.fourCC != 0x52414D57 || tile->areas.size != sizeof(MapAreaChunk) - 8)
				{
					WLOG("Map file " << file << " seems to be corrupted: Wrong area chunk");
					//TODO: Should we cancel the loading process?
				}

				// Read collision data
				if (mapHeaderChunk.offsCollision)
				{
					mapFile.seekg(mapHeaderChunk.offsCollision, std::ios::beg);

					// Read collision header
					mapFile.read(reinterpret_cast<char *>(&tile->collision.fourCC), sizeof(UInt32));
					mapFile.read(reinterpret_cast<char *>(&tile->collision.size), sizeof(UInt32));
					mapFile.read(reinterpret_cast<char *>(&tile->collision.vertexCount), sizeof(UInt32));
					mapFile.read(reinterpret_cast<char *>(&tile->collision.triangleCount), sizeof(UInt32));
					if (tile->collision.fourCC != 0x4C434D57 || tile->collision.size < sizeof(UInt32) * 4)
					{
						WLOG("Map file " << file << " seems to be corrupted: Wrong collision chunk (Size: " << tile->collision.size);
						//TODO: Should we cancel the loading process?
					}

					// Read all vertices
					tile->collision.vertices.resize(tile->collision.vertexCount);
					size_t numBytes = sizeof(float) * 3 * tile->collision.vertexCount;
					mapFile.read(reinterpret_cast<char *>(tile->collision.vertices.data()), numBytes);

					// Read all indices
					tile->collision.triangles.resize(tile->collision.triangleCount);
					mapFile.read(reinterpret_cast<char *>(tile->collision.triangles.data()), sizeof(Triangle) * tile->collision.triangleCount);
				}

				// Read navigation data
				if (m_navMesh && mapHeaderChunk.offsNavigation)
				{
					mapFile.seekg(mapHeaderChunk.offsNavigation, std::ios::beg);

					// Read collision header
					mapFile.read(reinterpret_cast<char *>(&tile->navigation.fourCC), sizeof(UInt32));
					mapFile.read(reinterpret_cast<char *>(&tile->navigation.size), sizeof(UInt32));
					mapFile.read(reinterpret_cast<char *>(&tile->navigation.tileRef), sizeof(UInt32));
					
					// Read navigation mesh data
					const UInt32 dataSize = tile->navigation.size - sizeof(UInt32);
					if (dataSize)
					{
						tile->navigation.data.resize(dataSize, 0);
						mapFile.read(tile->navigation.data.data(), dataSize);

						dtTileRef ref = 0;
						dtStatus status = m_navMesh->addTile(reinterpret_cast<unsigned char*>(tile->navigation.data.data()),
							tile->navigation.data.size(), DT_TILE_FREE_DATA, 0, &ref);
						if (dtStatusFailed(status))
						{
							//ELOG("Failed adding nav tile!");
						}
					}
				}
			}

			return tile.get();
		}

		return nullptr;
	}

	float Map::getHeightAt(float x, float y)
	{
		return 0.0f;
	}

	bool Map::isInLineOfSight(const math::Vector3 &posA, const math::Vector3 &posB)
	{
		if (posA == posB) {
			return true;
		}

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

		const float dist = (posB - posA).length();

		// Test: Check every triangle (TODO: Use octree nodes)
		UInt32 triangleIndex = 0;
		for (const auto &triangle : startTile->collision.triangles)
		{
			auto &vA = startTile->collision.vertices[triangle.indexA];
			auto &vB = startTile->collision.vertices[triangle.indexB];
			auto &vC = startTile->collision.vertices[triangle.indexC];
			auto result = ray.intersectsTriangle(vA, vB, vC);
			if (result.first && result.second <= dist)
			{
				return false;
			}

			triangleIndex++;
		}

		// Target is in line of sight
		return true;
	}
	
	bool Map::calculatePath(const math::Vector3 & source, math::Vector3 dest, std::vector<math::Vector3>& out_path)
	{
		math::Vector3 dtStart(source.x, source.z, source.y);
		math::Vector3 dtEnd(dest.x, dest.z, dest.y);

		// No nav mesh loaded for this map?
		if (!m_navMesh || !m_navQuery)
		{
			// Build straight line from start to dest
			out_path.push_back(dest);
			return true;
		}

		int tx, ty;
		m_navMesh->calcTileLoc(&dtStart.x, &tx, &ty);
		if (!m_navMesh->getTileAt(tx, ty, 0))
		{
			out_path.push_back(dest);
			return true;
		}
		m_navMesh->calcTileLoc(&dtEnd.x, &tx, &ty);
		if (!m_navMesh->getTileAt(tx, ty, 0))
		{
			out_path.push_back(dest);
			return true;
		}

		// Load source tile
		TileIndex2D startIndex(
			static_cast<Int32>(floor((32.0 - (static_cast<double>(source.x) / 533.3333333)))),
			static_cast<Int32>(floor((32.0 - (static_cast<double>(source.y) / 533.3333333))))
			);
		auto *startTile = getTile(startIndex);
		if (!startTile)
		{
			out_path.push_back(dest);
			return true;
		}

		// Load dest tile
		TileIndex2D destIntex(
			static_cast<Int32>(floor((32.0 - (static_cast<double>(dest.x) / 533.3333333)))),
			static_cast<Int32>(floor((32.0 - (static_cast<double>(dest.y) / 533.3333333))))
			);
		auto *dstTile = getTile(destIntex);
		if (!dstTile)
		{
			out_path.push_back(dest);
			return true;
		}

		// Pathfinding
		float distToStartPoly, distToEndPoly;
		dtPolyRef startPoly = getPolyByLocation(dtStart, distToStartPoly);
		dtPolyRef endPoly = getPolyByLocation(dtEnd, distToEndPoly);
		if (startPoly == 0 || endPoly == 0)
		{
			out_path.push_back(dest);
			return true;
		}

		const bool isFarFromPoly = distToStartPoly > 7.0f || distToEndPoly > 7.0f;
		if (isFarFromPoly)
		{
			math::Vector3 closestPoint;
			if (dtStatusSucceed(m_navQuery->closestPointOnPoly(endPoly, &dtEnd.x, &closestPoint.x, nullptr)))
			{
				dtEnd = closestPoint;
				dest.y = dtEnd.z;
				dest.z = dtEnd.y;
			}
		}

		if (startPoly == endPoly)
		{
			out_path.push_back(dest);
			return true;
		}

		const int maxPathLength = 74;
		std::vector<dtPolyRef> tempPath(maxPathLength, 0);
		int pathLength;
		dtStatus dtResult = m_navQuery->findPath(
			startPoly,          // start polygon
			endPoly,            // end polygon
			&source.x,         // start position
			&dest.x,           // end position
			&m_filter,           // polygon search filter
			tempPath.data(),     // [out] path
			&pathLength,
			maxPathLength);   // max number of polygons in output path
		if (!pathLength ||
			dtStatusFailed(dtResult))
		{
			out_path.push_back(dest);
			return true;
		}

		// Resize path
		tempPath.resize(pathLength);

		std::vector<float> tempPathCoords(maxPathLength * 3);
		int tempPathCoordsCount = 0;

		// Set to true to generate straight path
		const bool useStraightPath = true;
		if (useStraightPath)
		{
			dtResult = m_navQuery->findStraightPath(
				&dtStart.x,
				&dtEnd.x,
				tempPath.data(),
				static_cast<int>(tempPath.size()),
				tempPathCoords.data(),
				0,
				0,
				&tempPathCoordsCount,
				maxPathLength,
				DT_STRAIGHTPATH_ALL_CROSSINGS
				);
		}

		if (tempPathCoordsCount < 1 ||
			dtStatusFailed(dtResult))
		{
			out_path.push_back(dest);
			return true;
		}
		
		tempPathCoords.resize(tempPathCoordsCount * 3);
		for (auto p = tempPathCoords.begin(); p != tempPathCoords.end(); p += 3)
		{
			auto v = math::Vector3(p[0], p[2], p[1]);
			out_path.push_back(v);
		}

		return true;
	}

	dtPolyRef Map::getPolyByLocation(const math::Vector3 &point, float &out_distance) const
	{
		// TODO: Use cached poly
		dtPolyRef polyRef = 0;

		// we don't have it in our old path
		// try to get it by findNearestPoly()
		// first try with low search box
		math::Vector3 extents(3.0f, 5.0f, 3.0f);    // bounds of poly search area
		math::Vector3 closestPoint(0.0f, 0.0f, 0.0f);
		dtStatus dtResult = m_navQuery->findNearestPoly(&point.x, &extents.x, &m_filter, &polyRef, &closestPoint.x);
		if (dtStatusSucceed(dtResult) && polyRef != 0)
		{
			out_distance = dtVdist(&closestPoint.x, &point.x);
			return polyRef;
		}

		// still nothing ..
		// try with bigger search box
		extents[1] = 200.0f;
		dtResult = m_navQuery->findNearestPoly(&point.x, &extents.x, &m_filter, &polyRef, &closestPoint.x);
		if (dtStatusSucceed(dtResult) && polyRef != 0)
		{
			out_distance = dtVdist(&closestPoint.x, &point.x);
			return polyRef;
		}

		return 0;
	}
}
