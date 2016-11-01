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
					ELOG("Could not load map file " << file);
					return nullptr;
				}

				// Read map header
				MapHeaderChunk mapHeaderChunk;
				mapFile.read(reinterpret_cast<char *>(&mapHeaderChunk), sizeof(MapHeaderChunk));
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
				if (mapHeaderChunk.version != 0x130)
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
	
	namespace
	{
		inline bool inRange(const float* v1, const float* v2, const float r, const float h)
		{
			const float dx = v2[0] - v1[0];
			const float dy = v2[1] - v1[1];
			const float dz = v2[2] - v1[2];
			return (dx*dx + dz*dz) < r*r && fabsf(dy) < h;
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

		static bool getSteerTarget(dtNavMeshQuery* navQuery, const float* startPos, const float* endPos,
			const float minTargetDist,
			const dtPolyRef* path, const int pathSize,
			float* steerPos, unsigned char& steerPosFlag, dtPolyRef& steerPosRef,
			float* outPoints = 0, int* outPointCount = 0)
		{
			// Find steer target.
			static const int MAX_STEER_POINTS = 3;
			float steerPath[MAX_STEER_POINTS * 3];
			unsigned char steerPathFlags[MAX_STEER_POINTS];
			dtPolyRef steerPathPolys[MAX_STEER_POINTS];
			int nsteerPath = 0;
			navQuery->findStraightPath(startPos, endPos, path, pathSize,
				steerPath, steerPathFlags, steerPathPolys, &nsteerPath, MAX_STEER_POINTS);
			if (!nsteerPath)
				return false;

			if (outPoints && outPointCount)
			{
				*outPointCount = nsteerPath;
				for (int i = 0; i < nsteerPath; ++i)
					dtVcopy(&outPoints[i * 3], &steerPath[i * 3]);
			}

			// Find vertex far enough to steer to.
			int ns = 0;
			while (ns < nsteerPath)
			{
				// Stop at Off-Mesh link or when point is further than slop away.
				if ((steerPathFlags[ns] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ||
					!inRange(&steerPath[ns * 3], startPos, minTargetDist, 1000.0f))
					break;
				ns++;
			}
			// Failed to find good point to steer to.
			if (ns >= nsteerPath)
				return false;

			dtVcopy(steerPos, &steerPath[ns * 3]);
			steerPos[1] = startPos[1];
			steerPosFlag = steerPathFlags[ns];
			steerPosRef = steerPathPolys[ns];

			return true;
		}

	}

	dtStatus Map::getSmoothPath(const math::Vector3 & dtStart, const math::Vector3 & dtEnd, std::vector<dtPolyRef> &polyPath, std::vector<math::Vector3> &out_smoothPath, UInt32 maxPathSize)
	{
		const float SMOOTH_PATH_STEP_SIZE = 4.0f;
		const float SMOOTH_PATH_SLOP = 0.3f;

		// Do we have something to do?
		if (polyPath.empty() || maxPathSize < 2)
		{
			return DT_FAILURE;
		}

		static const UInt32 MAX_POLYS = 74;

		// This is the maximum amount of allowed points. This also has something to do with how
		// the WoW protocol packs coordinates: We only have 11 bits per horizontal axis, where 1 bit
		// is the sign. Since we iterate with a distance of 4 yards, the max path length generated
		// is limited by this value properly.
		const UInt32 MAX_PATH_LENGTH = 74;
		if (maxPathSize > MAX_PATH_LENGTH)
		{
			maxPathSize = MAX_PATH_LENGTH;
		}

		// Iterate over the path to find smooth path on the detail mesh surface.
		dtPolyRef polys[MAX_POLYS];
		memcpy(polys, &polyPath[0], sizeof(dtPolyRef) * polyPath.size());
		int npolys = polyPath.size();

		float iterPos[3], targetPos[3];
		m_navQuery->closestPointOnPoly(polyPath.front(), &dtStart.x, iterPos, 0);
		m_navQuery->closestPointOnPoly(polyPath.back(), &dtEnd.x, targetPos, 0);

		// Speed allocation up a bit
		out_smoothPath.clear();
		out_smoothPath.reserve(maxPathSize);

		// Add first point (starting point)
		out_smoothPath.push_back(math::Vector3(iterPos[0], iterPos[1], iterPos[2]));

		// Move towards target a small advancement at a time until target reached or
		// when ran out of memory to store the path.
		// Move towards target a small advancement at a time until target reached or
		// when ran out of memory to store the path.
		while (npolys && out_smoothPath.size() < MAX_PATH_LENGTH)
		{
			// Find location to steer towards.
			float steerPos[3];
			unsigned char steerPosFlag;
			dtPolyRef steerPosRef;
			if (!getSteerTarget(m_navQuery.get(), iterPos, targetPos, SMOOTH_PATH_SLOP,
				polys, npolys, steerPos, steerPosFlag, steerPosRef))
				break;

			const bool endOfPath = (steerPosFlag & DT_STRAIGHTPATH_END);
			const bool offMeshConnection = (steerPosFlag & DT_STRAIGHTPATH_OFFMESH_CONNECTION);

			// Find movement delta.
			float delta[3], len;
			dtVsub(delta, steerPos, iterPos);
			len = dtMathSqrtf(dtVdot(delta, delta));
			// If the steer target is end of path or off-mesh link, do not move past the location.
			if ((endOfPath || offMeshConnection) && len < SMOOTH_PATH_STEP_SIZE)
				len = 1;
			else
				len = SMOOTH_PATH_STEP_SIZE / len;
			float moveTgt[3];
			dtVmad(moveTgt, iterPos, delta, len);

			// Move
			float result[3];
			dtPolyRef visited[16];
			int nvisited = 0;
			m_navQuery->moveAlongSurface(polys[0], iterPos, moveTgt, &m_filter,
				result, visited, &nvisited, 16);

			npolys = fixupCorridor(polys, npolys, MAX_POLYS, visited, nvisited);
			npolys = fixupShortcuts(polys, npolys, m_navQuery.get());

			float h = 0;
			m_navQuery->getPolyHeight(polys[0], result, &h);
			dtVcopy(iterPos, result);

			// Handle end of path and off-mesh links when close enough.
			if (endOfPath && inRange(iterPos, steerPos, SMOOTH_PATH_SLOP, 1.0f))
			{
				// Reached end of path.
				dtVcopy(iterPos, targetPos);
				if (out_smoothPath.size() < MAX_PATH_LENGTH)
				{
					out_smoothPath.push_back(math::Vector3(iterPos[0], iterPos[1], iterPos[2]));
					//dtVcopy(&m_smoothPath[m_nsmoothPath * 3], iterPos);
				}
				break;
			}
			else if (offMeshConnection && inRange(iterPos, steerPos, SMOOTH_PATH_SLOP, 1.0f))
			{
				// Reached off-mesh connection.
				float startPos[3], endPos[3];

				// Advance the path up to and over the off-mesh connection.
				dtPolyRef prevRef = 0, polyRef = polys[0];
				int npos = 0;
				while (npos < npolys && polyRef != steerPosRef)
				{
					prevRef = polyRef;
					polyRef = polys[npos];
					npos++;
				}
				for (int i = npos; i < npolys; ++i)
					polys[i - npos] = polys[i];
				npolys -= npos;

				// Handle the connection.
				dtStatus status = m_navMesh->getOffMeshConnectionPolyEndPoints(prevRef, polyRef, startPos, endPos);
				if (dtStatusSucceed(status))
				{
					if (out_smoothPath.size() < MAX_PATH_LENGTH)
					{
						out_smoothPath.push_back(math::Vector3(startPos[0], startPos[1], startPos[2]));

						// Hack to make the dotted path not visible during off-mesh connection.
						if (out_smoothPath.size() & 1)
						{
							out_smoothPath.push_back(math::Vector3(startPos[0], startPos[1], startPos[2]));
						}
					}

					// Move position at the other side of the off-mesh link.
					dtVcopy(iterPos, endPos);
					float eh = 0.0f;
					m_navQuery->getPolyHeight(polys[0], iterPos, &eh);
					iterPos[1] = eh;
				}
			}

			// Store results
			if (out_smoothPath.size() < maxPathSize)
			{
				out_smoothPath.push_back(math::Vector3(iterPos[0], iterPos[1], iterPos[2]));
			}
		}

		return DT_SUCCESS;
	}

	bool Map::calculatePath(const math::Vector3 & source, math::Vector3 dest, std::vector<math::Vector3>& out_path)
	{
		// Convert the given start and end point into recast coordinate system
		math::Vector3 dtStart = wowToRecastCoord(source);
		math::Vector3 dtEnd = wowToRecastCoord(dest);

		// No nav mesh loaded for this map?
		if (!m_navMesh || !m_navQuery)
		{
			// Build straight line from start to dest
			out_path.push_back(dest);
			return true;
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
				out_path.push_back(dest);
				return true;
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
					out_path.push_back(dest);
					return true;
				}
			}
		}
		
		// Make sure that source cell is loaded
		int tx, ty;
		m_navMesh->calcTileLoc(&dtStart.x, &tx, &ty);
		if (!m_navMesh->getTileAt(tx, ty, 0))
		{
			// Not loaded (TODO: Handle this error?)
			out_path.push_back(dest);
			return true;
		}

		// Make sure that target cell is loaded
		m_navMesh->calcTileLoc(&dtEnd.x, &tx, &ty);
		if (!m_navMesh->getTileAt(tx, ty, 0))
		{
			// Not loaded (TODO: Handle this error?)
			out_path.push_back(dest);
			return true;
		}

		// Find polygon on start and end point
		float distToStartPoly, distToEndPoly;
		dtPolyRef startPoly = getPolyByLocation(dtStart, distToStartPoly);
		dtPolyRef endPoly = getPolyByLocation(dtEnd, distToEndPoly);
		if (startPoly == 0 || endPoly == 0)
		{
			// Either start or target does not have a valid polygon, so we can't walk
			// from the start point or to the target point at all! (TODO: Handle this error?)
			out_path.push_back(dest);
			return true;
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

#if 0
		// Both points are on the same polygon, so build a shortcut
		// TODO: We don't want to build a shortcut here, but we still want to
		// create a smooth path so that z value will be corrected
		if (startPoly == endPoly)
		{
			out_path.push_back(dest);
			return true;
		}
#endif

		// Buffer to store polygons that need to be used for our path
		const int maxPathLength = 74;
		std::vector<dtPolyRef> tempPath(maxPathLength, 0);

		// This will store the resulting path length (number of polygons)
		int pathLength = 0;
		dtStatus dtResult = m_navQuery->findPath(
			startPoly,				// start polygon
			endPoly,				// end polygon
			&dtStart.x,				// start position
			&dtEnd.x,				// end position
			&m_filter,				// polygon search filter
			tempPath.data(),		// [out] path
			&pathLength,			// number of polygons used by path (<= maxPathLength)
			maxPathLength);			// max number of polygons in output path
		if (!pathLength ||
			dtStatusFailed(dtResult))
		{
			// Could not find path... TODO?
			out_path.push_back(dest);
			return true;
		}

		// Resize path
		tempPath.resize(pathLength);

		// Buffer to store path coordinates
		std::vector<math::Vector3> tempPathCoords(maxPathLength);
		std::vector<dtPolyRef> tempPathPolys(maxPathLength);
		int tempPathCoordsCount = 0;

		// Set to true to generate straight path
		const bool correctPathHeights = true;
		
		dtResult = m_navQuery->findStraightPath(
			&dtStart.x,							// Start position
			&dtEnd.x,							// End position
			tempPath.data(),					// Polygon path
			static_cast<int>(tempPath.size()),	// Number of polygons in path
			&tempPathCoords[0].x,				// [out] Path points
			nullptr,							// [out] unused
			&tempPathPolys[0],					// [out] Polygon id for each point.
			&tempPathCoordsCount,				// [out] used coordinate count in vertices (3 floats = 1 vert)
			maxPathLength,						// max coordinate count
			DT_STRAIGHTPATH_ALL_CROSSINGS		// options
			);

		// Correct actual path length
		tempPathCoords.resize(tempPathCoordsCount);
		tempPathPolys.resize(tempPathCoordsCount);

		if (correctPathHeights)
		{
			// We don't need to fix the first point as this is our starting point
			for (size_t index = 1; index < tempPathCoords.size(); ++index)
			{
				// Get polygon
				dtPolyRef &poly = tempPathPolys[index++];

				// Get points
				auto &thisPoint = tempPathCoords[index];
				const auto &lastPoint = tempPathCoords[index - 1];

				// First, fix this points height
				float height = thisPoint.y;
				if (m_navQuery->getPolyHeight(poly, &thisPoint.x, &height) == DT_SUCCESS)
				{
					thisPoint.y = height;
				}

				// Now calculate distance if there is anything to calculate at all
				if (lastPoint != thisPoint)
				{
					const float dist = (lastPoint - thisPoint).length();
					const size_t iterCount = std::min<size_t>(74, static_cast<size_t>(dist / 4.0f));
					const float iterStep = 1.0f / static_cast<float>(iterCount);

					// Again: We don't need the first point as it is the last point
					for (size_t i = 1; i < iterCount; ++i)
					{
						// Insert new waypoint
						math::Vector3 newPoint = lastPoint.lerp(thisPoint, iterStep * static_cast<float>(i));

						// Fix height for this point, too
						float newHeight = newPoint.y;
						if (m_navQuery->getPolyHeight(poly, &newPoint.x, &newHeight) == DT_SUCCESS)
						{
							newPoint.y = newHeight;
						}

						// Push back
						tempPathCoords.insert(tempPathCoords.begin() + index, std::move(newPoint));
						tempPathPolys.insert(tempPathPolys.begin() + index, poly);
						tempPathCoordsCount++;
						index++;
					}
				}
			}
		}

		// Not enough waypoints generated or waypoint generation completely failed?
		if (tempPathCoordsCount < 1 ||
			dtStatusFailed(dtResult))
		{
			// TODO: Handle this error?
			out_path.push_back(dest);
			return true;
		}
		
		// Append waypoints
		for (const auto &p : tempPathCoords)
		{
			out_path.push_back(
				recastToWoWCoord(p));
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
		dtStatus dtResult = m_navQuery->findRandomPointAroundCircle(startPoly, &dtCenter.x, radius, &m_filter, frand, &endPoly, &out.x);
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
