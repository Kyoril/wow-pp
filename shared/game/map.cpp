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
#include "common/macros.h"
#include "shared/proto_data/maps.pb.h"
#include "log/default_log_levels.h"
#include "common/make_unique.h"
#include "math/vector3.h"
#include "common/clock.h"
#include "detour/DetourCommon.h"
#include "recast/Recast.h"

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
			ASSERT(m_navQuery);

			dtStatus dtResult = m_navQuery->init(m_navMesh, 1024);
			if (dtStatusFailed(dtResult))
			{
				m_navQuery.reset();
			}

			// Setup filter
			m_filter.setIncludeFlags(1 | 2 | 4 | 8 | 16 | 32);		// Testing...
			m_adtSlopeFilter.setIncludeFlags(1 | 2 | 4 | 8 | 16);
			m_adtSlopeFilter.setExcludeFlags(32);

			navMeshsPerMap[entry.id()] = std::move(navMesh);
			ILOG("Navigation mesh for map " << m_entry.id() << " initialized");
		}
	}

	void Map::loadAllTiles()
	{
		// Load all tiles
		for (size_t x = 0; x < 64; ++x)
		{
			for (size_t y = 0; y < 64; ++y)
			{
				getTile(TileIndex2D(x, y));
			}
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
					//DLOG("Could not load map file " << file << ": File does not exist");
					return nullptr;
				}

				// Open file for reading
				std::ifstream mapFile(file.c_str(), std::ios::in | std::ios::binary);
				if (!mapFile)
				{
					ELOG("Could not load map file " << file);
					return nullptr;
				}

				// Read map header
				MapHeaderChunk mapHeaderChunk;
				mapFile.read(reinterpret_cast<char *>(&mapHeaderChunk), sizeof(MapHeaderChunk));
				if (mapHeaderChunk.header.fourCC != MapHeaderChunkCC)
				{
					ELOG("Could not load map file " << file << ": Invalid four-cc code!");
					return nullptr;
				}
				if (mapHeaderChunk.header.size != sizeof(MapHeaderChunk) - 8)
				{
					ELOG("Could not load map file " << file << ": Unexpected header chunk size (" << (sizeof(MapHeaderChunk) - 8) << " expected)!");
					return nullptr;
				}
				if (mapHeaderChunk.version != MapHeaderChunk::MapFormat)
				{
					ELOG("Could not load map file " << file << ": Unsupported file format version!");
					return nullptr;
				}

				// Read area table
				mapFile.seekg(mapHeaderChunk.offsAreaTable, std::ios::beg);

				// Create new tile and read area data
				mapFile.read(reinterpret_cast<char *>(&tile->areas), sizeof(MapAreaChunk));
				if (tile->areas.header.fourCC != MapAreaChunkCC || tile->areas.header.size != sizeof(MapAreaChunk) - 8)
				{
					WLOG("Map file " << file << " seems to be corrupted: Wrong area chunk");
					return nullptr;
				}

				// Read navigation data
				if (m_navMesh && mapHeaderChunk.offsNavigation)
				{
					mapFile.seekg(mapHeaderChunk.offsNavigation, std::ios::beg);

					// Read chunk header
					mapFile.read(reinterpret_cast<char *>(&tile->navigation.header.fourCC), sizeof(UInt32));
					if (tile->navigation.header.fourCC != MapNavChunkCC)
					{
						WLOG("Map file " << file << " seems to be corrupted: Wrong nav chunk header chunk");
						return nullptr;
					}
					mapFile.read(reinterpret_cast<char *>(&tile->navigation.header.size), sizeof(UInt32));
					mapFile.read(reinterpret_cast<char *>(&tile->navigation.tileCount), sizeof(UInt32));
					
					// Read navigation meshes if any
					tile->navigation.tiles.resize(tile->navigation.tileCount);
					for (UInt32 i = 0; i < tile->navigation.tileCount; ++i)
					{
						auto &data = tile->navigation.tiles[i];
						mapFile.read(reinterpret_cast<char *>(&data.size), sizeof(UInt32));

						// Finally read tile data
						if (data.size)
						{
							// Reserver and read
							data.data.resize(data.size);
							mapFile.read(data.data.data(), data.size);

							// Add tile to navmesh
							dtTileRef ref = 0;
							dtStatus status = m_navMesh->addTile(reinterpret_cast<unsigned char*>(data.data.data()),
								data.data.size(), DT_TILE_FREE_DATA, 0, &ref);
							if (dtStatusFailed(status))
							{
								ELOG("Failed adding nav tile at " << position << ": 0x" << std::hex << (status & DT_STATUS_DETAIL_MASK));
							}
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

#if 0
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
#endif

		// Target is in line of sight
		return true;
	}
	
#define MAX_PATH_LENGTH         74
#define MAX_POINT_PATH_LENGTH   74
#define SMOOTH_PATH_STEP_SIZE   4.0f
#define SMOOTH_PATH_SLOP        0.08f
#define VERTEX_SIZE       3
#define INVALID_POLYREF   0

	namespace
	{
		bool inRangeYZX(const float* v1, const float* v2, float r, float h)
		{
			const float dx = v2[0] - v1[0];
			const float dy = v2[1] - v1[1]; // elevation
			const float dz = v2[2] - v1[2];
			return (dx * dx + dz * dz) < r * r && fabsf(dy) < h;
		}

		static int fixupCorridor(dtPolyRef* path, const int npath, const int maxPath,
			const dtPolyRef* visited, const int nvisited)
		{
			int furthestPath = -1;
			int furthestVisited = -1;

			// Find furthest common polygon.
			for (int i = npath - 1; i >= 0; --i)
			{
				bool found = false;
				for (int j = nvisited - 1; j >= 0; --j)
				{
					if (path[i] == visited[j])
					{
						furthestPath = i;
						furthestVisited = j;
						found = true;
					}
				}
				if (found)
					break;
			}

			// If no intersection found just return current path. 
			if (furthestPath == -1 || furthestVisited == -1)
				return npath;

			// Concatenate paths.	

			// Adjust beginning of the buffer to include the visited.
			const int req = nvisited - furthestVisited;
			const int orig = rcMin(furthestPath + 1, npath);
			int size = rcMax(0, npath - orig);
			if (req + size > maxPath)
				size = maxPath - req;
			if (size)
				memmove(path + req, path + orig, size * sizeof(dtPolyRef));

			// Store visited
			for (int i = 0; i < req; ++i)
				path[i] = visited[(nvisited - 1) - i];

			return req + size;
		}

		static int fixupShortcuts(dtPolyRef* path, int npath, dtNavMeshQuery* navQuery)
		{
			if (npath < 3)
				return npath;

			// Get connected polygons
			static const int maxNeis = 16;
			dtPolyRef neis[maxNeis];
			int nneis = 0;

			const dtMeshTile* tile = 0;
			const dtPoly* poly = 0;
			if (dtStatusFailed(navQuery->getAttachedNavMesh()->getTileAndPolyByRef(path[0], &tile, &poly)))
				return npath;

			for (unsigned int k = poly->firstLink; k != DT_NULL_LINK; k = tile->links[k].next)
			{
				const dtLink* link = &tile->links[k];
				if (link->ref != 0)
				{
					if (nneis < maxNeis)
						neis[nneis++] = link->ref;
				}
			}

			// If any of the neighbour polygons is within the next few polygons
			// in the path, short cut to that polygon directly.
			static const int maxLookAhead = 6;
			int cut = 0;
			for (int i = dtMin(maxLookAhead, npath) - 1; i > 1 && cut == 0; i--) {
				for (int j = 0; j < nneis; j++)
				{
					if (path[i] == neis[j]) {
						cut = i;
						break;
					}
				}
			}
			if (cut > 1)
			{
				int offset = cut - 1;
				npath -= offset;
				for (int i = 1; i < npath; i++)
					path[i] = path[i + offset];
			}

			return npath;
		}

		static bool getSteerTarget(dtNavMeshQuery *navQuery, const float* startPos, const float* endPos,
			float minTargetDist, const dtPolyRef* path, UInt32 pathSize,
			float* steerPos, unsigned char& steerPosFlag, dtPolyRef& steerPosRef)
		{
			// Find steer target.
			static const UInt32 MAX_STEER_POINTS = 3;
			float steerPath[MAX_STEER_POINTS * VERTEX_SIZE];
			unsigned char steerPathFlags[MAX_STEER_POINTS];
			dtPolyRef steerPathPolys[MAX_STEER_POINTS];
			UInt32 nsteerPath = 0;
			dtStatus dtResult = navQuery->findStraightPath(startPos, endPos, path, pathSize,
				steerPath, steerPathFlags, steerPathPolys, (int*)&nsteerPath, MAX_STEER_POINTS);
			if (!nsteerPath || dtStatusFailed(dtResult))
				return false;

			// Find vertex far enough to steer to.
			UInt32 ns = 0;
			while (ns < nsteerPath)
			{
				// Stop at Off-Mesh link or when point is further than slop away.
				if ((steerPathFlags[ns] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ||
					!inRangeYZX(&steerPath[ns * VERTEX_SIZE], startPos, minTargetDist, 1000.0f))
					break;
				++ns;
			}
			// Failed to find good point to steer to.
			if (ns >= nsteerPath)
				return false;

			dtVcopy(steerPos, &steerPath[ns * VERTEX_SIZE]);
			steerPos[1] = startPos[1];  // keep Z value
			steerPosFlag = steerPathFlags[ns];
			steerPosRef = steerPathPolys[ns];

			return true;
		}
	}

	dtStatus findSmoothPath(dtNavMeshQuery *query, dtNavMesh *navMesh, dtQueryFilter *filter, const float* startPos, const float* endPos,
		const dtPolyRef* polyPath, UInt32 polyPathSize,
		float* smoothPath, int* smoothPathSize, UInt32 maxSmoothPathSize)
	{
		*smoothPathSize = 0;
		UInt32 nsmoothPath = 0;

		dtPolyRef polys[MAX_PATH_LENGTH];
		memcpy(polys, polyPath, sizeof(dtPolyRef)*polyPathSize);
		UInt32 npolys = polyPathSize;

		float iterPos[VERTEX_SIZE], targetPos[VERTEX_SIZE];
		dtStatus dtResult = query->closestPointOnPolyBoundary(polys[0], startPos, iterPos);
		if (dtStatusFailed(dtResult))
		{
			return DT_FAILURE;
		}

		dtResult = query->closestPointOnPolyBoundary(polys[npolys - 1], endPos, targetPos);
		if (dtStatusFailed(dtResult))
		{
			return DT_FAILURE;
		}

		dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
		++nsmoothPath;

		// Move towards target a small advancement at a time until target reached or
		// when ran out of memory to store the path.
		while (npolys && nsmoothPath < maxSmoothPathSize)
		{
			// Find location to steer towards.
			float steerPos[VERTEX_SIZE];
			unsigned char steerPosFlag;
			dtPolyRef steerPosRef = 0;

			if (!getSteerTarget(query, iterPos, targetPos, SMOOTH_PATH_STEP_SIZE, polys, npolys, steerPos, steerPosFlag, steerPosRef))
				break;

			const bool endOfPath = !!(steerPosFlag & DT_STRAIGHTPATH_END);
			const bool offMeshConnection = !!(steerPosFlag & DT_STRAIGHTPATH_OFFMESH_CONNECTION);

			// Find movement delta.
			float delta[VERTEX_SIZE];
			dtVsub(delta, steerPos, iterPos);
			float len = sqrtf(dtVdot(delta, delta));
			// If the steer target is end of path or off-mesh link, do not move past the location.
			if ((endOfPath || offMeshConnection) && len < SMOOTH_PATH_STEP_SIZE)
				len = 1.0f;
			else
				len = SMOOTH_PATH_STEP_SIZE / len;

			float moveTgt[VERTEX_SIZE];
			dtVmad(moveTgt, iterPos, delta, len);

			// Move
			float result[VERTEX_SIZE];
			const static UInt32 MAX_VISIT_POLY = 16;
			dtPolyRef visited[MAX_VISIT_POLY];

			UInt32 nvisited = 0;
			query->moveAlongSurface(polys[0], iterPos, moveTgt, filter, result, visited, (int*)&nvisited, MAX_VISIT_POLY);
			npolys = fixupCorridor(polys, npolys, MAX_PATH_LENGTH, visited, nvisited);

			query->getPolyHeight(polys[0], result, &result[1]);
			result[1] += 0.5f;
			dtVcopy(iterPos, result);

			// Handle end of path and off-mesh links when close enough.
			if (endOfPath && inRangeYZX(iterPos, steerPos, SMOOTH_PATH_SLOP, 1.0f))
			{
				// Reached end of path.
				dtVcopy(iterPos, targetPos);
				if (nsmoothPath < maxSmoothPathSize)
				{
					dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
					++nsmoothPath;
				}
				break;
			}
			else if (offMeshConnection && inRangeYZX(iterPos, steerPos, SMOOTH_PATH_SLOP, 1.0f))
			{
				// Advance the path up to and over the off-mesh connection.
				dtPolyRef prevRef = INVALID_POLYREF;
				dtPolyRef polyRef = polys[0];
				UInt32 npos = 0;
				while (npos < npolys && polyRef != steerPosRef)
				{
					prevRef = polyRef;
					polyRef = polys[npos];
					++npos;
				}

				for (UInt32 i = npos; i < npolys; ++i)
					polys[i - npos] = polys[i];

				npolys -= npos;

				// Handle the connection.
				float newStartPos[VERTEX_SIZE], newEndPos[VERTEX_SIZE];
				dtResult = navMesh->getOffMeshConnectionPolyEndPoints(prevRef, polyRef, newStartPos, newEndPos);
				if (dtStatusSucceed(dtResult))
				{
					if (nsmoothPath < maxSmoothPathSize)
					{
						dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], newStartPos);
						++nsmoothPath;
					}
					// Move position at the other side of the off-mesh link.
					dtVcopy(iterPos, newEndPos);

					query->getPolyHeight(polys[0], iterPos, &iterPos[1]);
					iterPos[1] += 0.5f;
				}
			}

			// Store results.
			if (nsmoothPath < maxSmoothPathSize)
			{
				dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
				++nsmoothPath;
			}
		}

		*smoothPathSize = nsmoothPath;

		// this is most likely a loop
		return nsmoothPath < MAX_POINT_PATH_LENGTH ? DT_SUCCESS : DT_FAILURE;
	}

	dtStatus smoothPath(dtNavMeshQuery &query, dtNavMesh &navMesh, dtQueryFilter &filter, std::vector<dtPolyRef> &polyPath, std::vector<math::Vector3> &waypoints)
	{
		static constexpr float PointDist = 4.0f;	// One point every PointDist units
		static constexpr float Extents[3] = { 1.0f, 50.0f, 1.0f };

		// Travel along the path and insert new points in between, start with the second point
		size_t polyIndex = 0;
		for (size_t p = 1; p < waypoints.size(); ++p)
		{
			// Get the previous point
			const auto prevPoint = waypoints[p - 1];
			const auto thisPoint = waypoints[p];

			// Get the direction vector and the distance
			auto dir = thisPoint - prevPoint;
			auto dist = dir.normalize();
			const Int32 count = dist / PointDist;
			const float step = dist / count;

			// Now insert new points
			for (Int32 n = 1; n < count; n++)
			{
				const float d = n * step;
				auto newPoint = prevPoint + (dir * d);

				math::Vector3 closestPoint;
				dtPolyRef nearestPoly = 0;
				if (dtStatusFailed(query.findNearestPoly(&newPoint.x, Extents, &filter, &nearestPoly, &closestPoint.x)))
				{
					//WLOG("Could not find nearest poly");
				}

				// Now do the height correction
				if (dtStatusFailed(query.getPolyHeight(nearestPoly, &closestPoint.x, &newPoint.y)))
				{
					/*
					int resultCount = 0;
					std::vector<dtPolyRef> refs(10);
					if (dtStatusFailed(query.findPolysAroundCircle(nearestPoly, &closestPoint.x, 4.0f, &filter, &refs[0], nullptr, nullptr, &resultCount, refs.size())))
					{
						//WLOG("Failed to find polys around circle!");
					}
					else
					{
						bool found = false;
						for (int i = 0; i < resultCount; ++i)
						{
							if (!dtStatusFailed(query.getPolyHeight(refs[i], &closestPoint.x, &newPoint.y)))
							{
								newPoint.y += 0.375f;
								found = true;
								break;
							}
						}

						if (!found)
						{
							//WLOG("Could not find height on neighbor polys...");
						}
					}*/
				}
				else
				{
					newPoint.y += 0.375f;
				}

				waypoints.insert(waypoints.begin() + p, newPoint);

				// Next point
				p++;
			}

			polyIndex++;
		}

		return DT_SUCCESS;
	}

	bool Map::calculatePath(const math::Vector3 & source, math::Vector3 dest, std::vector<math::Vector3>& out_path, bool ignoreAdtSlope/* = true*/)
	{
		// Convert the given start and end point into recast coordinate system
		math::Vector3 dtStart = wowToRecastCoord(source);
		math::Vector3 dtEnd = wowToRecastCoord(dest);

		// No nav mesh loaded for this map?
		if (!m_navMesh)
		{
			ELOG("Could not find nav mesh!");
			return false;
		}

		// TODO: Better solution for this, as there could be more tiles between which could eventually
		// still be unloaded after this block
		{
			// Load source tile
			TileIndex2D startIndex(
				static_cast<Int32>(floor((32.0 - (static_cast<double>(source.x) / 533.3333333)))),
				static_cast<Int32>(floor((32.0 - (static_cast<double>(source.y) / 533.3333333))))
			);
			if (!getTile(startIndex))
			{
				//ELOG("Could not get source tile!");
				return false;
			}

			// Load dest tile
			TileIndex2D destIndex(
				static_cast<Int32>(floor((32.0 - (static_cast<double>(dest.x) / 533.3333333)))),
				static_cast<Int32>(floor((32.0 - (static_cast<double>(dest.y) / 533.3333333))))
			);
			if (destIndex != startIndex)
			{
				if (!getTile(destIndex))
				{
					//ELOG("Could not get dest tile!");
					return false;
				}
			}
		}
		
		// Make sure that source cell is loaded
		int tx, ty;
		m_navMesh->calcTileLoc(&dtStart.x, &tx, &ty);
		if (!m_navMesh->getTileAt(tx, ty, 0))
		{
			// Not loaded (TODO: Handle this error?)
			//ELOG("Could not get source tile on nav mesh");
			return false;
		}

		// Make sure that target cell is loaded
		m_navMesh->calcTileLoc(&dtEnd.x, &tx, &ty);
		if (!m_navMesh->getTileAt(tx, ty, 0))
		{
			// Not loaded (TODO: Handle this error?)
			//ELOG("Could not get dest tile on nav mesh");
			return false;
		}

		// Find polygon on start and end point
		float distToStartPoly, distToEndPoly;
		dtPolyRef startPoly = getPolyByLocation(dtStart, distToStartPoly);
		dtPolyRef endPoly = getPolyByLocation(dtEnd, distToEndPoly);
		if (startPoly == 0 || endPoly == 0)
		{
			// Either start or target does not have a valid polygon, so we can't walk
			// from the start point or to the target point at all! (TODO: Handle this error?)
			//ELOG("Could not get source poly or dest poly");
			return false;
		}

		// We check if the distance to the start or end polygon is too far and eventually correct the target
		// location
		const bool isFarFromPoly = distToStartPoly > 7.0f || distToEndPoly > 7.0f;
		if (isFarFromPoly)
		{
			math::Vector3 closestPoint;
			if (dtStatusSucceed(m_navQuery->closestPointOnPoly(endPoly, &dtEnd.x, &closestPoint.x, nullptr)))
			{
				dtEnd = closestPoint;
				dest = recastToWoWCoord(dtEnd);
			}
		}

		// Set to true to generate straight path
		const bool correctPathHeights = false;
		dtStatus dtResult;

		// Buffer to store polygons that need to be used for our path
		const int maxPathLength = 74;
		std::vector<dtPolyRef> tempPath(maxPathLength, 0);

		// This will store the resulting path length (number of polygons)
		int pathLength = 0;

		if (startPoly != endPoly)
		{
			dtResult = m_navQuery->findPath(
				startPoly,											// start polygon
				endPoly,											// end polygon
				&dtStart.x,											// start position
				&dtEnd.x,											// end position
				ignoreAdtSlope ? &m_filter : &m_adtSlopeFilter,		// polygon search filter
				tempPath.data(),									// [out] path
				&pathLength,										// number of polygons used by path (<= maxPathLength)
				maxPathLength);										// max number of polygons in output path
			if (!pathLength ||
				dtStatusFailed(dtResult))
			{
				// Could not find path... TODO?
				ELOG("findPath failed with result " << dtResult);
				return false;
			}
		}
		else
		{
			// Build shortcut
			tempPath[0] = startPoly;
			tempPath[1] = endPoly;
			pathLength = 2;
		}
		
		// Resize path
		tempPath.resize(pathLength);

		if (!correctPathHeights)
		{
			// Buffer to store path coordinates
			std::vector<math::Vector3> tempPathCoords(maxPathLength);
			std::vector<dtPolyRef> tempPathPolys(maxPathLength);
			int tempPathCoordsCount = 0;

			if (startPoly != endPoly)
			{
				// Find a straight path
				dtResult = m_navQuery->findStraightPath(
					&dtStart.x,							// Start position
					&dtEnd.x,							// End position
					tempPath.data(),					// Polygon path
					static_cast<int>(tempPath.size()),	// Number of polygons in path
					&tempPathCoords[0].x,				// [out] Path points
					nullptr,							// [out] Path point flags (unused)
					&tempPathPolys[0],					// [out] Polygon id for each point.
					&tempPathCoordsCount,				// [out] used coordinate count in vertices (3 floats = 1 vert)
					maxPathLength,						// max coordinate count
					0									// options
				);
				if (dtStatusFailed(dtResult))
				{
					ELOG("findStraightPath failed");
					return false;
				}
			}
			else
			{
				// Build shortcut
				tempPathCoords[0] = dtStart;
				tempPathCoords[1] = dtEnd;				
				tempPathPolys[0] = startPoly;
				tempPathPolys[1] = endPoly;
				tempPathCoordsCount = 2;
			}

			// Correct actual path length
			tempPathCoords.resize(tempPathCoordsCount);
			tempPathPolys.resize(tempPathCoordsCount);

			// Smooth out the path
			dtResult = smoothPath(*m_navQuery, *m_navMesh, ignoreAdtSlope ? m_filter : m_adtSlopeFilter, tempPathPolys, tempPathCoords);
			if (dtStatusFailed(dtResult))
			{
				ELOG("Failed to smooth out existing path.");
				return false;
			}

			// Append waypoints
			for (const auto &p : tempPathCoords)
			{
				out_path.push_back(
					recastToWoWCoord(p));
			}
		}
		else
		{
			int smoothPathSize = 0;
			float smoothPath[VERTEX_SIZE * MAX_POINT_PATH_LENGTH];
			dtResult = findSmoothPath(m_navQuery.get(), m_navMesh, ignoreAdtSlope ? &m_filter : &m_adtSlopeFilter, &dtStart.x, &dtEnd.x, &tempPath[0], pathLength, smoothPath, &smoothPathSize, 74);
			if (dtStatusFailed(dtResult))
			{
				//ELOG("Could not get smooth path: " << dtResult);
				return false;
			}
			else
			{
				for (int i = 0; i < smoothPathSize * VERTEX_SIZE; i += VERTEX_SIZE)
				{
					out_path.push_back(
						recastToWoWCoord(math::Vector3(smoothPath[i], smoothPath[i + 1], smoothPath[i + 2])));
				}
				return true;
			}
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

	namespace
	{
		static float frand()
		{
			return (float)rand() / (float)RAND_MAX;
		}
	}

	bool Map::getRandomPointOnGround(const math::Vector3 & center, float radius, math::Vector3 & out_point)
	{
		math::Vector3 dtCenter = wowToRecastCoord(center);

		// No nav mesh loaded for this map?
		if (!m_navMesh || !m_navQuery)
		{
			return false;
		}

		// Load source tile
		TileIndex2D startIndex(
			static_cast<Int32>(floor((32.0 - (static_cast<double>(center.x) / 533.3333333)))),
			static_cast<Int32>(floor((32.0 - (static_cast<double>(center.y) / 533.3333333))))
			);
		auto *startTile = getTile(startIndex);
		if (!startTile)
		{
			return false;
		}

		int tx, ty;
		m_navMesh->calcTileLoc(&dtCenter.x, &tx, &ty);
		if (!m_navMesh->getTileAt(tx, ty, 0))
		{
			return false;
		}

		float distToStartPoly = 0.0f;
		dtPolyRef startPoly = getPolyByLocation(dtCenter, distToStartPoly);
		dtPolyRef endPoly = 0;

		const bool isFarFromPoly = distToStartPoly > 7.0f;
		if (isFarFromPoly)
		{
			math::Vector3 closestPoint;
			if (dtStatusSucceed(m_navQuery->closestPointOnPoly(startPoly, &dtCenter.x, &closestPoint.x, nullptr)))
			{
				dtCenter = closestPoint;
			}
		}

		math::Vector3 out;
		dtStatus dtResult = m_navQuery->findRandomPointAroundCircle(startPoly, &dtCenter.x, radius, &m_adtSlopeFilter, frand, &endPoly, &out.x);
		if (dtStatusSucceed(dtResult))
		{
			out_point = recastToWoWCoord(out);
			return true;
		}

		return false;
	}

	Vertex recastToWoWCoord(const Vertex & in_recastCoord)
	{
		return Vertex(-in_recastCoord.z, -in_recastCoord.x, in_recastCoord.y);
	}

	Vertex wowToRecastCoord(const Vertex & in_wowCoord)
	{
		return Vertex(-in_wowCoord.y, in_wowCoord.z, -in_wowCoord.x);
	}
}
