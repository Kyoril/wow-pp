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

// 
// Part of this code taken from namreeb's (https://github.com/namreeb) and tripleslash's (https://github.com/tripleslash)
// nav lib project, to allow units to walk on terrain of any slope angle.
// 

#include "pch.h"
#include "common/typedefs.h"
#include "log/default_log_levels.h"
#include "log/log_std_stream.h"
#include "mpq_file.h"
#include "dbc_file.h"
#include "wdt_file.h"
#include "adt_file.h"
#include "wmo_file.h"
#include "m2_file.h"
#include "mesh_settings.h"
#include "mesh_data.h"
#include "file_io.h"
#include "binary_io/writer.h"
#include "binary_io/stream_sink.h"
#include "common/make_unique.h"
#include "game/map.h"
#include "math/matrix3.h"
#include "math/matrix4.h"
#include "math/vector3.h"
#include "math/quaternion.h"
#include "math/aabb_tree.h"
#include "common/linear_set.h"
using namespace std;
using namespace wowpp;

//////////////////////////////////////////////////////////////////////////
// Calls:
//	For Each Map... (separate thread for each map, up to #cpu-cores)
//		convertMap
//			createNavMesh
//			For Each Tile...
//				convertADT
//					createNavTile

//////////////////////////////////////////////////////////////////////////
// Shortcuts
namespace fs = boost::filesystem;
namespace po = boost::program_options;

//////////////////////////////////////////////////////////////////////////
// Path variables
static const fs::path inputPath(".");
static const fs::path outputPath("maps");
static const fs::path bvhOutputPath("bvh");

//////////////////////////////////////////////////////////////////////////
// DBC files
static std::unique_ptr<DBCFile> dbcMap;
static std::unique_ptr<DBCFile> dbcAreaTable;
static std::unique_ptr<DBCFile> dbcLiquidType;

//////////////////////////////////////////////////////////////////////////
// Command line arguments
static Int32 buildOnlyMap = -1;
static Int32 buildOnlyTileX = -1;
static Int32 buildOnlyTileY = -1;
static bool generateDebugFiles = false;

//////////////////////////////////////////////////////////////////////////
// Caches
std::map<UInt32, UInt32> areaFlags;
std::map<UInt32, LinearSet<UInt32>> tilesByMap;

// Loaded WMO and M2 models used for navigation mesh calculations
std::map<UInt32, std::shared_ptr<math::AABBTree>> wmoTrees;
std::map<UInt32, std::shared_ptr<math::AABBTree>> doodadTrees;

std::mutex wmoMutex, doodadMutex;
LinearSet<String> serializedWMOs, serializedDoodads;
std::map<String, std::shared_ptr<math::AABBTree>> doodadIdsByFile, wmoIdsByFile;

//////////////////////////////////////////////////////////////////////////
// Helper functions
namespace
{
	enum AreaFlags : unsigned char
	{
		Walkable = 1 << 0,
		ADT = 1 << 1,
		Liquid = 1 << 2,
		WMO = 1 << 3,
		Doodad = 1 << 4,
		ADTUnwalkable = 1 << 5,
	};
	using PolyFlags = AreaFlags;

	using SmartHeightFieldPtr = std::unique_ptr<rcHeightfield, decltype(&rcFreeHeightField)>;
	using SmartCompactHeightFieldPtr = std::unique_ptr<rcCompactHeightfield, decltype(&rcFreeCompactHeightfield)>;
	using SmartContourSetPtr = std::unique_ptr<rcContourSet, decltype(&rcFreeContourSet)>;
	using SmartPolyMeshPtr = std::unique_ptr<rcPolyMesh, decltype(&rcFreePolyMesh)>;
	using SmartPolyMeshDetailPtr = std::unique_ptr<rcPolyMeshDetail, decltype(&rcFreePolyMeshDetail)>;

	/// Converts a tiles x and y coordinate into a single number (tile id).
	/// @param tileX The x coordinate of the tile.
	/// @param tileY The y coordinate of the tile.
	/// @returns The generated tile id.
	static UInt32 packTileID(UInt32 tileX, UInt32 tileY) 
	{
		return tileX << 16 | tileY;
	}
	
	/// Converts a tile id into x and y coordinates.
	/// @param tile The packed tile id.
	/// @param out_x Tile x coordinate will be stored here.
	/// @param out_y Tile y coordinate will be stored here.
	static void unpackTileID(UInt32 tile, UInt32& out_x, UInt32& out_y)
	{ 
		out_x = tile >> 16;
		out_y = tile & 0xFF;
	}

	/// This method calculates the boundaries of a given map.
	static void calculateMapBounds(UInt32 mapId, UInt32 &out_minX, UInt32 &out_minY, UInt32 &out_maxX, UInt32 &out_maxY)
	{
		auto it = tilesByMap.find(mapId);
		if (it == tilesByMap.end())
			return;

		out_minX = std::numeric_limits<UInt32>::max();
		out_maxX = std::numeric_limits<UInt32>::lowest();
		out_minY = std::numeric_limits<UInt32>::max();
		out_maxY = std::numeric_limits<UInt32>::lowest();

		for (auto &tile : it->second)
		{
			UInt32 tileX = 0, tileY = 0;
			unpackTileID(tile, tileX, tileY);

			if (tileX < out_minX) out_minX = tileX;
			if (tileY < out_minY) out_minY = tileY;
			if (tileX > out_maxX) out_maxX = tileX;
			if (tileY > out_maxY) out_maxY = tileY;
		}
	}
	
	/// Calculates tile boundaries in world units, but converted to the recast coordinate system.
	static void calculateADTTileBounds(UInt32 tileX, UInt32 tileY, float* bmin, float* bmax)
	{
		bmin[0] = (32 - int(tileY)) * -MeshSettings::AdtSize;
		bmin[1] = std::numeric_limits<float>::lowest();
		bmin[2] = (32 - int(tileX)) * -MeshSettings::AdtSize;

		bmax[0] = bmin[0] + MeshSettings::AdtSize;
		bmax[1] = std::numeric_limits<float>::max();
		bmax[2] = bmin[2] + MeshSettings::AdtSize;
	}

	// Code taken from tripleslash
	/// Manually disconnects all tile connections which cannot be climbed by the characters.
	/// This is required because at the time of navmesh generation, the values were set to infinite.
	/// @param chf Compact heightfield object which will be updated.
	/// @param walkableClime The walkable climb height value in voxels.
	static void selectivelyEnforceWalkableClimb(rcCompactHeightfield &chf, int walkableClimb)
	{
		for (int y = 0; y < chf.height; ++y)
		{
			for (int x = 0; x < chf.width; ++x)
			{
				auto const &cell = chf.cells[y*chf.width + x];

				// for each span in this cell of the compact heightfield...
				for (int i = static_cast<int>(cell.index), ni = static_cast<int>(cell.index + cell.count); i < ni; ++i)
				{
					auto &span = chf.spans[i];
					const NavTerrain spanArea = static_cast<NavTerrain>(chf.areas[i]);

					// check all four directions for this span
					for (int dir = 0; dir < 4; ++dir)
					{
						// there will be at most one connection for this span in this direction
						auto const k = rcGetCon(span, dir);

						// if there is no connection, do nothing
						if (k == RC_NOT_CONNECTED)
							continue;

						auto const nx = x + rcGetDirOffsetX(dir);
						auto const ny = y + rcGetDirOffsetY(dir);

						// this should never happen since we already know there is a connection in this direction
						ASSERT(nx >= 0 && ny >= 0 && nx < chf.width && ny < chf.height);

						auto const &neighborCell = chf.cells[ny*chf.width + nx];
						auto const &neighborSpan = chf.spans[k + neighborCell.index];

						// if the span height difference is <= the walkable climb, nothing else matters.  skip it
						if (rcAbs(static_cast<int>(neighborSpan.y) - static_cast<int>(span.y)) <= walkableClimb)
							continue;

						const NavTerrain neighborSpanArea = static_cast<NavTerrain>(chf.areas[k + neighborCell.index]);

						// if both the current span and the neighbor span are ADTs, do nothing
						const bool thisIsAdt = (spanArea & PolyFlags::ADT) != 0 || (spanArea & PolyFlags::ADTUnwalkable) != 0;
						const bool otherIsAdt = (neighborSpanArea & PolyFlags::ADT) != 0 || (neighborSpanArea & PolyFlags::ADTUnwalkable) != 0;
						if (thisIsAdt && otherIsAdt)
							continue;

						//std::cout << "Removing connection for span " << i << " dir " << dir << " to " << k << std::endl;
						rcSetCon(span, dir, RC_NOT_CONNECTED);
					}
				}
			}
		}
	}

	/// Finishes the nav mesh generation.
	/// @param ctx Recast context object.
	/// @param config Recast configuration structure.
	/// @param tileX X coordinate of the map tile (matches ADT cell size).
	/// @param tileY Y coordinate of the map tile (matches ADT cell size).
	/// @param tx X coordinate of the navmesh tile (each cell has 4x4 navmesh tiles).
	/// @param ty Y coordinate of the navmesh tile (each cell has 4x4 navmesh tiles).
	/// @param out_chunk Navigation chunk which will hold the serialized navmesh data which will be written to the generated map file.
	/// @param solid Solid heightfield which will hold the generated nav mesh data.
	/// @returns true on success, false on error.
	bool finishMesh(rcContext &ctx, const rcConfig &config, int tileX, int tileY, size_t tx, size_t ty, MapNavigationChunk &out_chunk, rcHeightfield &solid)
	{
		// Allocate and build compact height field
		SmartCompactHeightFieldPtr chf(rcAllocCompactHeightfield(), rcFreeCompactHeightfield);
		if (!rcBuildCompactHeightfield(&ctx, config.walkableHeight, std::numeric_limits<int>::max(), solid, *chf))
		{
			ELOG("Could not build compact height field (rcBuildCompactHeightfield failed)");
			return false;
		}

		// Since we used infinite walkable climb, we need to manually cut connections for everything non-ADT and 
		// non-Liquid-related polygons
		selectivelyEnforceWalkableClimb(*chf, config.walkableClimb);

		// Build distance field
		if (!rcBuildDistanceField(&ctx, *chf))
		{
			ELOG("Could not build distance field (rcBuildDistanceField failed)");
			return false;
		}

		// Build regions
		if (!rcBuildRegions(&ctx, *chf, config.borderSize, config.minRegionArea, config.mergeRegionArea))
		{
			ELOG("Could not build regions (rcBuildRegions failed)");
			return false;
		}

		// Allocate contour set and generate contours
		SmartContourSetPtr cset(rcAllocContourSet(), rcFreeContourSet);
		if (!rcBuildContours(&ctx, *chf, config.maxSimplificationError, config.maxEdgeLen, *cset))
		{
			ELOG("Could not build contour set (rcBuildContours failed)");
			return false;
		}

		// If this happens, it's most likely because no geometry was inside the bounding box...
		ASSERT(!!cset->nconts);

		// Allocate and build poly mesh
		SmartPolyMeshPtr polyMesh(rcAllocPolyMesh(), rcFreePolyMesh);
		if (!rcBuildPolyMesh(&ctx, *cset, config.maxVertsPerPoly, *polyMesh))
		{
			ELOG("Could not build poly mesh (rcBuildPolyMesh failed)");
			return false;
		}

		// Allocate and build poly mesh details
		SmartPolyMeshDetailPtr polyMeshDetail(rcAllocPolyMeshDetail(), rcFreePolyMeshDetail);
		if (!rcBuildPolyMeshDetail(&ctx, *polyMesh, *chf, config.detailSampleDist, config.detailSampleMaxError, *polyMeshDetail))
		{
			ELOG("Could not build poly mesh detail (rcBuildPolyMeshDetail failed)");
			return false;
		}

		// Free no longer needed objects manually simply to be more memory-friendly (would be removed later anyway)
		chf.reset(nullptr);
		cset.reset(nullptr);

		// Check for too many vertices
		if (polyMesh->nverts >= 0xFFFF)
		{
			ELOG("Too many mesh vertices produces for tile (" << tileX << ", " << tileY << ")");
			return false;
		}

		// Mark these flags walkable and apply custom flags, too
		for (int i = 0; i < polyMesh->npolys; ++i)
		{
			if (!polyMesh->areas[i])
				continue;

			polyMesh->flags[i] = static_cast<unsigned short>(PolyFlags::Walkable | polyMesh->areas[i]);
		}
		
		// Prepare detour navmesh parameters
		dtNavMeshCreateParams params;
		memset(&params, 0, sizeof(params));
		params.verts = polyMesh->verts;
		params.vertCount = polyMesh->nverts;
		params.polys = polyMesh->polys;
		params.polyAreas = polyMesh->areas;
		params.polyFlags = polyMesh->flags;
		params.polyCount = polyMesh->npolys;
		params.nvp = polyMesh->nvp;
		params.detailMeshes = polyMeshDetail->meshes;
		params.detailVerts = polyMeshDetail->verts;
		params.detailVertsCount = polyMeshDetail->nverts;
		params.detailTris = polyMeshDetail->tris;
		params.detailTriCount = polyMeshDetail->ntris;
		params.walkableHeight = MeshSettings::WalkableHeight;
		params.walkableRadius = MeshSettings::WalkableRadius;
		params.walkableClimb = MeshSettings::WalkableClimb;
		params.tileX = (tileY * MeshSettings::TilesPerADT) + tx;
		params.tileY = (tileX * MeshSettings::TilesPerADT) + ty;
		params.tileLayer = 0;
		memcpy(params.bmin, polyMesh->bmin, sizeof(polyMesh->bmin));
		memcpy(params.bmax, polyMesh->bmax, sizeof(polyMesh->bmax));
		params.cs = config.cs;
		params.ch = config.ch;
		params.buildBvTree = true;

		// Finally, serialize our mesh data
		unsigned char *outData;
		int outDataSize;
		if (!dtCreateNavMeshData(&params, &outData, &outDataSize))
		{
			ELOG("Could not create nav mesh data (dtCreateNavMeshData failed)");
			return false;
		}

		// Create tile data
		MapNavigationChunk::TileData tile;
		tile.size = outDataSize;
		tile.data.resize(outDataSize);
		std::memcpy(&tile.data[0], outData, outDataSize);

		// Add tile to the list of tiles
		out_chunk.tiles.push_back(std::move(tile));

		// Increase and validate tile count
		out_chunk.tileCount++;
		ASSERT(out_chunk.tileCount == out_chunk.tiles.size() && "Navigation chunks tile count does not match the actual tile count!");

		// Eventually generate debug files
		if (generateDebugFiles)
		{
			std::unique_ptr<FileIO> debugFile(new FileIO());
			std::ostringstream strm;
			strm << "meshes/nav/poly/tile_" << tileX << "_" << tileY << "-" << tx << "_" << ty << "_poly.obj";

			debugFile->openForWrite(strm.str().c_str());
			duDumpPolyMeshToObj(*polyMesh, debugFile.get());

			std::unique_ptr<FileIO> debugFileDetail(new FileIO());
			std::ostringstream strmDetail;
			strmDetail << "meshes/nav/detail/tile_" << tileX << "_" << tileY << "-" << tx << "_" << ty << "_detail.obj";

			debugFileDetail->openForWrite(strmDetail.str().c_str());
			duDumpPolyMeshDetailToObj(*polyMeshDetail, debugFileDetail.get());
		}

		dtFree(outData);
		return true;
	}

	/// Restores the area flags of all listed ADT spans in case there flags have been changed.
	/// @param spans List of spans whose flags will be restored.
	/// @param area Area flag to apply to all given spans.
	void restoreAdtSpans(const std::vector<rcSpan *> &spans, unsigned int area)
	{
		for (auto s : spans)
			s->area |= area;
	}

	/// Initializes a rcConfig struct without settings it's boundaries.
	/// @param config The config structure that will be initialized / reset.
	void initializeRecastConfig(rcConfig &config)
	{
		memset(&config, 0, sizeof(config));
		config.cs = MeshSettings::CellSize;
		config.ch = MeshSettings::CellHeight;
		config.walkableSlopeAngle = MeshSettings::WalkableSlope;
		config.walkableClimb = MeshSettings::VoxelWalkableClimb;
		config.walkableHeight = MeshSettings::VoxelWalkableHeight;
		config.walkableRadius = MeshSettings::VoxelWalkableRadius;
		config.maxEdgeLen = config.walkableRadius * 4;
		config.maxSimplificationError = MeshSettings::MaxSimplificationError;
		config.minRegionArea = MeshSettings::MinRegionSize;
		config.mergeRegionArea = MeshSettings::MergeRegionSize;
		config.maxVertsPerPoly = MeshSettings::VerticesPerPolygon;
		config.tileSize = MeshSettings::TileVoxelSize;
		config.borderSize = config.walkableRadius + 3;
		config.width = config.tileSize + config.borderSize * 2;
		config.height = config.tileSize + config.borderSize * 2;
		config.detailSampleDist = MeshSettings::DetailSampleDistance;
		config.detailSampleMaxError = MeshSettings::DetailSampleMaxError;
	}

	/// Taken from recast library to calculate the normal of a triangle. Used to filter "unwalkable adt" areas afterwards,
	/// since we don't want to remove them from the nav mesh but mark them.
	/// @param v0 Pointer to three floats which represent the first vertex of the triangle.
	/// @param v1 Pointer to three floats which represent the second vertex of the triangle.
	/// @param v2 Pointer to three floats which represent the third vertex of the triangle.
	/// @param norm The three vector components of the normal vector will be stored there.
	static void calcTriNormal(const float* v0, const float* v1, const float* v2, float* norm)
	{
		assert(norm);
		float e0[3], e1[3];
		rcVsub(e0, v1, v0);
		rcVsub(e1, v2, v0);
		rcVcross(norm, e0, e1);
		rcVnormalize(norm);
	}

	/// Rasterizes the given mesh data for the nav mesh generation and may do some filtering.
	/// @param ctx Recast context object.
	/// @param heightField Recast heightfield which will be updated.
	/// @param filterWalkable If true, unwalkable triangles based on the given slope will not be added to the nav mesh.
	/// @param slope Maximum walkable slope for this mesh type.
	/// @param mesh The source mesh to rasterize.
	/// @param areaFlags Global area flags for to use for all triangles generated by this source mesh.
	/// @returns true if everything was okay.
	bool rasterize(rcContext &ctx, rcHeightfield &heightField, bool filterWalkable, float slope, const MeshData &mesh, unsigned char areaFlags)
	{
		if (!mesh.solidVerts.size() || !mesh.solidTris.size())
			return true;

		std::vector<unsigned char> areas(mesh.solidTris.size() / 3, areaFlags);
		if (!!(areaFlags & ADT))
		{
			// Special case for ADT: Mark unwalkable ADT triangles as a special area for filter usage
			const float walkableThr = cosf(slope / 180.0f*RC_PI);

			float norm[3];
			for (int i = 0; i < areas.size(); ++i)
			{
				const int* tri = &mesh.solidTris[i * 3];
				calcTriNormal(&mesh.solidVerts[tri[0] * 3], &mesh.solidVerts[tri[1] * 3], &mesh.solidVerts[tri[2] * 3], norm);

				// Check if the face is walkable.
				if (norm[1] <= walkableThr)
				{
					areas[i] = ADTUnwalkable;
				}
			}
		}

		if (filterWalkable)
			rcClearUnwalkableTriangles(&ctx, slope, &mesh.solidVerts[0], static_cast<int>(mesh.solidVerts.size() / 3), &mesh.solidTris[0], static_cast<int>(mesh.solidTris.size() / 3), &areas[0]);

		rcRasterizeTriangles(&ctx, &mesh.solidVerts[0], static_cast<int>(mesh.solidVerts.size() / 3), &mesh.solidTris[0], &areas[0], static_cast<int>(mesh.solidTris.size() / 3), heightField);
		return true;
	}

	/// Prepares the navmesh of a given map id by calculating the map bounds and the tile count.
	/// @param mapId Id of the map this nav mesh belongs to.
	/// @param navMesh The nav mesh that will be initialized.
	/// @returns false if something went wrong.
	static bool createNavMesh(UInt32 mapId, dtNavMesh &navMesh)
	{
		// Look for tiles
		auto &tiles = tilesByMap[mapId];
		if (tiles.empty())
		{
			ILOG("No tiles found for map " << mapId);
			return false;
		}

		UInt32 minX = 0, minY = 0, maxX = 0, maxY = 0;
		calculateMapBounds(mapId, minX, minY, maxX, maxY);

		float bmin[3], bmax[3];
		calculateADTTileBounds(minX, minY, bmin, bmax);

		bmin[0] = -(32 * MeshSettings::AdtSize);
		bmin[2] = -(32 * MeshSettings::AdtSize);

		int maxTiles = tiles.size() * (MeshSettings::TilesPerADT * MeshSettings::TilesPerADT);
		const int maxPolysPerTile = 1 << DT_POLY_BITS;

		// Setup navigation mesh creation parameters
		dtNavMeshParams navMeshParams;
		memset(&navMeshParams, 0, sizeof(dtNavMeshParams));
		navMeshParams.tileWidth = MeshSettings::TileSize;
		navMeshParams.tileHeight = MeshSettings::TileSize;
		rcVcopy(navMeshParams.orig, bmin);
		navMeshParams.maxTiles = maxTiles;
		navMeshParams.maxPolys = maxPolysPerTile;

		// Allocate our navigation mesh
		ILOG("[Map " << mapId << "] Creating navigation mesh...");
		if (!navMesh.init(&navMeshParams))
		{
			ELOG("[Map " << mapId << "] Failed to create navigation mesh!")
			return false;
		}
		else
		{
			DLOG("\t[Map " << mapId << "] bounds: " << minX << "x" << minY << " - " << maxX << "x" << maxY);
			DLOG("\t[Map " << mapId << "] origin: " << bmin[0] << " " << bmin[1] << " " << bmin[2]);
		}

		return true;
	}

	/// Generates a navigation tile.
	/// @param mapName Name of the map which is used for debug file generation.
	/// @param mapId Id of the map.
	/// @param tileX X tile coordinate of the adt cell.
	/// @param tileY Y tile coordinate of the adt cell.
	/// @param navMesh The nav mesh which will be updated.
	/// @param adt The parsed adt file.
	/// @param out_chunk Navigation chunk which will hold the serialized nav mesh data of this till and
	///                  will be written to the generated map file.
	/// @return false on error, true on success.
	static bool createNavChunk(const String &mapName, UInt32 mapId, UInt32 tileX, UInt32 tileY, dtNavMesh &navMesh, const ADTFile &adt, const MapWMOChunk &wmos, const MapDoodadChunk &doodads, MapNavigationChunk &out_chunk)
	{
		// Min and max height values used for recast
		float minZ = std::numeric_limits<float>::max(), maxZ = std::numeric_limits<float>::lowest();

		// Reset chunk data
		out_chunk.header.fourCC = MapNavChunkCC;
		out_chunk.header.size = 0;
		out_chunk.tiles.clear();
		out_chunk.tileCount = 0;

		// Process WMOs
		MeshData wmoMesh;
		{
			size_t indexOffset = 0;
			for (const auto &wmo : wmos.entries)
			{
#define WOWPP_DEG_TO_RAD(x) (3.14159265358979323846 * (x) / -180.0)
				math::Matrix3 rotMat = math::Matrix3::fromEulerAnglesXYZ(
					WOWPP_DEG_TO_RAD(-wmo.rotation[2]), WOWPP_DEG_TO_RAD(-wmo.rotation[0]), WOWPP_DEG_TO_RAD(-wmo.rotation[1] - 180));

				math::Vector3 position(wmo.position.z, wmo.position.x, wmo.position.y);
				position.x = (32 * 533.3333f) - position.x;
				position.y = (32 * 533.3333f) - position.y;
#undef WOWPP_DEG_TO_RAD

				auto it = wmoTrees.find(wmo.uniqueId);
				if (it != wmoTrees.end())
				{
					for (const auto &vert : it->second->getVertices())
					{
						math::Vector3 transformed = (rotMat * vert) + position;
						auto recastCoord = wowToRecastCoord(transformed);
						wmoMesh.solidVerts.push_back(recastCoord.x);
						wmoMesh.solidVerts.push_back(recastCoord.y);
						wmoMesh.solidVerts.push_back(recastCoord.z);
					}

					size_t i = 0;
					for (auto &tri : it->second->getIndices())
					{
						wmoMesh.solidTris.push_back(tri + indexOffset);
						if (++i == 3)
						{
							wmoMesh.triangleFlags.push_back(AreaFlags::WMO);
							i = 0;
						}
					}
				}
				else
				{
					WLOG("Could not find WMO by unique id " << wmo.uniqueId);
				}

				indexOffset = wmoMesh.solidVerts.size() / 3;
			}
		}

		// Process ADTs
		MeshData adtMesh;
		{
			// Now we add adts
			boost::filesystem::path adtPath;
			if (addTerrainMesh(adt, tileX, tileY, ENTIRE, adtMesh))
			{
				std::unique_ptr<ADTFile> adtInst;

#define LOAD_TERRAIN(x, y, spot) \
				adtPath = boost::filesystem::path("World\\Maps") / mapName / mapName; \
				adtInst = make_unique<ADTFile>(adtPath.leaf().append(fmt::sprintf("%d_%d.adt", y, x)).string()); \
				if (adtInst->load()) \
				{ \
					addTerrainMesh(*adtInst, (x), (y), spot, adtMesh); \
				}

				LOAD_TERRAIN(tileX + 1, tileY, LEFT);
				LOAD_TERRAIN(tileX - 1, tileY, RIGHT);
				LOAD_TERRAIN(tileX, tileY + 1, TOP);
				LOAD_TERRAIN(tileX, tileY - 1, BOTTOM);
#undef LOAD_TERRAIN
			}
		}

		// Process Doodads
		MeshData doodadMesh;
		{
			size_t indexOffset = 0;
			for (const auto &doodad : doodads.entries)
			{
#define WOWPP_DEG_TO_RAD(x) (3.14159265358979323846 * (x) / 180.0)
				constexpr float mid = 32.f * MeshSettings::AdtSize;

				const float rotX = WOWPP_DEG_TO_RAD(doodad.rotation[2]);
				const float rotY = WOWPP_DEG_TO_RAD(doodad.rotation[0]);
				const float rotZ = WOWPP_DEG_TO_RAD(doodad.rotation[1] + 180.0f);
				const float scale = doodad.scale;

				math::Matrix4 matZ; matZ.fromAngleAxis(math::Vector3(0.0f, 0.0f, 1.0f), rotZ);
				math::Matrix4 matY; matY.fromAngleAxis(math::Vector3(0.0f, 1.0f, 0.0f), rotY);
				math::Matrix4 matX; matX.fromAngleAxis(math::Vector3(1.0f, 0.0f, 0.0f), rotX);
				math::Matrix4 matFinal =
					math::Matrix4::getTranslation(mid - doodad.position[2], mid - doodad.position[0], doodad.position[1]) *
					math::Matrix4::getScale(scale, scale, scale) *
					matZ *
					matY *
					matX;
#undef WOWPP_DEG_TO_RAD

				auto it = doodadTrees.find(doodad.uniqueId);
				if (it != doodadTrees.end())
				{
					for (const auto &vert : it->second->getVertices())
					{
						math::Vector3 transformed = (matFinal * vert);
						auto recastCoord = wowToRecastCoord(transformed);
						doodadMesh.solidVerts.push_back(recastCoord.x);
						doodadMesh.solidVerts.push_back(recastCoord.y);
						doodadMesh.solidVerts.push_back(recastCoord.z);
					}

					size_t i = 0;
					for (auto &tri : it->second->getIndices())
					{
						doodadMesh.solidTris.push_back(tri + indexOffset);
						if (++i == 3)
						{
							doodadMesh.triangleFlags.push_back(AreaFlags::WMO);
							i = 0;
						}
					}

					indexOffset = doodadMesh.solidVerts.size() / 3;
				}
				else
				{
					WLOG("Could not find doodad by unique id " << doodad.uniqueId);
				}
			}
		}

		if (generateDebugFiles)
		{
			// Serialize mesh data for debugging purposes
			wowpp::serializeMeshData("_adt", mapId, tileX, tileY, adtMesh);
			wowpp::serializeMeshData("_wmo", mapId, tileX, tileY, wmoMesh);
			wowpp::serializeMeshData("_doodad", mapId, tileX, tileY, doodadMesh);
			DLOG("Serialized!");
		}

		// Adjust min and max z values
		for (UInt32 i = 0; i < adtMesh.solidVerts.size(); i += 3)
		{
			const float &z = adtMesh.solidVerts[i + 1];
			if (z < minZ)
				minZ = z;
			if (z > maxZ)
				maxZ = z;
		}
		for (UInt32 i = 0; i < wmoMesh.solidVerts.size(); i += 3)
		{
			const float &z = wmoMesh.solidVerts[i + 1];
			if (z < minZ)
				minZ = z;
			if (z > maxZ)
				maxZ = z;
		}
		for (UInt32 i = 0; i < doodadMesh.solidVerts.size(); i += 3)
		{
			const float &z = doodadMesh.solidVerts[i + 1];
			if (z < minZ)
				minZ = z;
			if (z > maxZ)
				maxZ = z;
		}

		// Create the context object
		rcContext ctx(false);

		// Initialize config
		rcConfig config;
		initializeRecastConfig(config);
		config.bmin[1] = minZ;
		config.bmax[1] = maxZ;

		// Determine all nav tiles of this ADT cell
		for (size_t ty = 0; ty < MeshSettings::TilesPerADT; ++ty)
		{
			for (size_t tx = 0; tx < MeshSettings::TilesPerADT; ++tx)
			{
				// Calculate tile bounds and apply them
				float bmin[3], bmax[3];
				calculateADTTileBounds(tileX, tileY, bmin, bmax);

				// Adjust to tile position
				bmin[0] += MeshSettings::TileSize * tx;
				bmin[2] += MeshSettings::TileSize * ty;
				bmax[0] += MeshSettings::TileSize * tx;
				bmax[2] += MeshSettings::TileSize * ty;

				rcVcopy(config.bmin, bmin);
				rcVcopy(config.bmax, bmax);
				config.bmin[1] = minZ;
				config.bmax[1] = maxZ;

				// Apply border size
				config.bmin[0] -= config.borderSize * config.cs;
				config.bmin[2] -= config.borderSize * config.cs;
				config.bmax[0] += config.borderSize * config.cs;
				config.bmax[2] += config.borderSize * config.cs;

				// Allocate and build the recast heightfield
				SmartHeightFieldPtr solid(rcAllocHeightfield(), rcFreeHeightField);
				if (!rcCreateHeightfield(&ctx, *solid, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch))
				{
					ELOG("\t\t\tCould not create heightfield (rcCreateHeightfield failed)");
					return false;
				}

				// Rasterize adt terrain mesh
				if (!rasterize(ctx, *solid, false, config.walkableSlopeAngle, adtMesh, AreaFlags::ADT))
				{
					ELOG("\t\t\tCould not rasterize ADT data");
					return false;
				}

				// Rasterize adt wmo object meshes
				if (!rasterize(ctx, *solid, true, config.walkableSlopeAngle, wmoMesh, AreaFlags::WMO))
				{
					ELOG("\t\t\tCould not rasterize WMO data");
					return false;
				}

				// Rasterize adt doodad object meshes
				if (!rasterize(ctx, *solid, true, config.walkableSlopeAngle, doodadMesh, AreaFlags::WMO))
				{
					ELOG("\t\t\tCould not rasterize DOODAD data");
					return false;
				}

				// Remember all ADT flagged spans as the information may get lost after the next step
				std::vector<rcSpan *> adtSpans, unwalkableAdtSpans;
				adtSpans.reserve(solid->width*solid->height);
				for (int i = 0; i < solid->width * solid->height; ++i)
					for (rcSpan *s = solid->spans[i]; s; s = s->next)
					{
						if (!!(s->area & AreaFlags::ADT))
							adtSpans.push_back(s);
						else if (!!(s->area & AreaFlags::ADTUnwalkable))
							unwalkableAdtSpans.push_back(s);
					}

				// Filter ledge spans
				rcFilterLedgeSpans(&ctx, config.walkableHeight, config.walkableClimb, *solid);

				// Restore ADT flags for previously remembered spans
				restoreAdtSpans(adtSpans, AreaFlags::ADT);
				restoreAdtSpans(unwalkableAdtSpans, AreaFlags::ADTUnwalkable);

				// Apply more geometry filtering
				rcFilterWalkableLowHeightSpans(&ctx, config.walkableHeight, *solid);
				rcFilterLowHangingWalkableObstacles(&ctx, config.walkableClimb, *solid);

				// Finalize the mesh
				auto const result = finishMesh(ctx, config, tileX, tileY, tx, ty, out_chunk, *solid);
				if (!result)
				{
					ELOG("\t\t\tCould not finish mesh");
					return false;
				}
			}
		}

		return true;
	}

	/// Loads all WMOs of a given ADT file into the global wmo storage and builds their
	/// bvh trees.
	/// @param adt The parsed adt file infos.
	static bool loadADTWmos(const ADTFile &adt, MapWMOChunk &out_chunk)
	{
		// Reset chunk structure
		out_chunk.entries.clear();
		out_chunk.header.fourCC = MapWMOChunkCC;
		out_chunk.header.size = 0;

		// Now for every referenced WMO chunk in the parsed ADT file...
		for (const auto &chunk : adt.getMODFChunk().entries)
		{
			// Retrieve WMO file name
			const String filename = adt.getWMO(chunk.mwidEntry);
			if (filename.empty())
			{
				WLOG("Could not resolve WMO name of wmo entry " << chunk.uniqueId);
				continue;
			}

			// Build file path
			fs::path filePath = bvhOutputPath / ("WMO_" + fs::path(filename).stem().string() + ".bvh");

			// Load wmo if not happened already
			std::lock_guard<std::mutex> lock(wmoMutex);
			if (!serializedWMOs.contains(filename))
			{
				serializedWMOs.add(filename);

				auto wmoFile = std::make_shared<WMOFile>(filename);
				if (!wmoFile->load())
				{
					ELOG("Error loading wmo: " << filename);
					return false;
				}

				// Build AABBTree and serialize it
				ILOG("\tBuilding WMO " << wmoFile->getBaseName());
				std::vector<math::AABBTree::Vertex> vertices;
				std::vector<math::AABBTree::Index> indices;
				if (wmoFile->isRootWMO())
				{
					for (const auto &group : wmoFile->getGroups())
					{
						vertices.reserve(vertices.size() + group->getVertices().size());
						indices.reserve(indices.size() + group->getIndices().size());

						math::AABBTree::Index offset = vertices.size();
						for (const auto &v : group->getVertices())
						{
							vertices.push_back(v);
						}

						for (UInt32 index = 0; index < group->getIndices().size(); index += 3)
						{
							if (!group->isCollisionTriangle(index / 3))
							{
								continue;
							}

							const auto &groupInds = group->getIndices();
							indices.push_back(groupInds[index + 0] + offset);
							indices.push_back(groupInds[index + 1] + offset);
							indices.push_back(groupInds[index + 2] + offset);
						}
					}
				}

				auto wmoTree = std::make_shared<math::AABBTree>(vertices, indices);
				wmoTrees[chunk.uniqueId] = wmoTree;
				wmoIdsByFile[filename] = wmoTree;

				// Serialize it
				std::ofstream file(filePath.string().c_str(), std::ios::out | std::ios::binary);
				if (!file)
				{
					ELOG("Failed to create output file " << filePath);
					return false;
				}

				io::StreamSink fileSink(file);
				io::Writer fileWriter(fileSink);
				fileWriter << *wmoTree;
			}
			else
			{
				auto it = wmoIdsByFile.find(filename);
				if (it == wmoIdsByFile.end())
				{
					WLOG("Could not find wmo id for file " << filename);
				}

				wmoTrees[chunk.uniqueId] = it->second;
			}

			// Add a new WMO entry
			MapWMOChunk::WMOEntry entry;
			entry.uniqueId = chunk.uniqueId;
			entry.fileName = filePath.stem().string();
			entry.position = chunk.position;
			entry.rotation = chunk.rotation;
			out_chunk.entries.push_back(std::move(entry));
			// Position+Rotation+UniqueId+StrLen+Filename
			out_chunk.header.size += sizeof(math::Vector3) * 2 + sizeof(UInt32) + sizeof(UInt16) + entry.fileName.length();
		}

		return true;
	}

	/// Loads all Doodads of a given ADT file into the global doodad storage and builds
	/// their bvh trees.
	/// @param adt The parsed adt file infos.
	static bool loadADTDoodads(const ADTFile &adt, MapDoodadChunk &out_chunk)
	{
		// Reset chunk structure
		out_chunk.entries.clear();
		out_chunk.header.fourCC = MapDoodadChunkCC;
		out_chunk.header.size = 0;

		for (const auto &chunk : adt.getMDDFChunk().entries)
		{
			std::lock_guard<std::mutex> lock(doodadMutex);

			// Retrieve Doodad file name
			const String filename = fs::path(adt.getMDX(chunk.mmidEntry)).replace_extension(".m2").string();

			// Build file path
			const fs::path filePath = bvhOutputPath / ("Doodad_" + fs::path(filename).stem().string() + ".bvh");

			if (!serializedDoodads.contains(filename))
			{
				serializedDoodads.add(filename);

				auto doodadFile = std::make_shared<M2File>(filename);
				if (!doodadFile->load())
				{
					ELOG("Error loading M2: " << filename);
					return false;
				}

				// Build AABBTree and serialize it
				ILOG("\tBuilding doodad " << doodadFile->getBaseName());
				auto doodadTree = std::make_shared<math::AABBTree>(doodadFile->getVertices(), doodadFile->getIndices());
				doodadTrees[chunk.uniqueId] = doodadTree;
				doodadIdsByFile[filename] = doodadTree;

				// Serialize it
				std::ofstream file(filePath.string().c_str(), std::ios::out | std::ios::binary);
				if (!file)
				{
					ELOG("Failed to create output file " << filePath);
					return false;
				}

				io::StreamSink fileSink(file);
				io::Writer fileWriter(fileSink);
				fileWriter << *doodadTree;
			}
			else
			{
				auto it = doodadIdsByFile.find(filename);
				if (it == doodadIdsByFile.end())
				{
					WLOG("Could not find doodad id for file " << filename);
				}

				doodadTrees[chunk.uniqueId] = it->second;
			}

			// Add a new Doodad entry
			MapDoodadChunk::DoodadEntry entry;
			entry.uniqueId = chunk.uniqueId;
			entry.fileName = filePath.stem().string();
			entry.position = chunk.position;
			entry.rotation = chunk.rotation;
			entry.scale = static_cast<float>(chunk.scale) / 1024.0f;
			out_chunk.entries.push_back(std::move(entry));
			// Position+Rotation+Scale+UniqueId+StrLen+Filename
			out_chunk.header.size += sizeof(math::Vector3) * 2 + sizeof(float) + sizeof(UInt32) + sizeof(UInt16) + entry.fileName.length();
		}

		return true;
	}

	static void createHeaderChunk(MapHeaderChunk &out_chunk)
	{
		memset(&out_chunk, 0, sizeof(MapHeaderChunk));
		out_chunk.header.fourCC = MapHeaderChunkCC;
		out_chunk.header.size = sizeof(MapHeaderChunk) - 8;
		out_chunk.version = MapHeaderChunk::MapFormat;
	}

	static void createAreaChunk(const ADTFile &adt, MapAreaChunk &out_chunk)
	{
		out_chunk.header.fourCC = MapAreaChunkCC;
		out_chunk.header.size = sizeof(MapAreaChunk) - 8;
		for (size_t i = 0; i < 16 * 16; ++i)
		{
			UInt32 areaId = adt.getMCNKChunk(i).areaid;
			UInt32 flags = 0;
			auto it = areaFlags.find(areaId);
			if (it != areaFlags.end())
			{
				flags = it->second;
			}
			out_chunk.cellAreas[i].areaId = areaId;
			out_chunk.cellAreas[i].flags = flags;
		}
	}

	/// Generates all required map files of a given ADT cell.
	/// @param mapId The map id of the wdt file.
	/// @param mapName Name of the map used for file name generation.
	/// @param wdt Parsed WDT file information which is required to determine if the adt cell should exists.
	/// @param packedTileIndex The packed index of the tile.
	/// @param navMesh The nav mesh that will be updated.
	/// @return true on success, false on error.
	static bool convertADT(UInt32 mapId, const String &mapName, WDTFile &wdt, UInt32 packedTileIndex, dtNavMesh &navMesh)
	{
		// Check tile
		auto &adtTiles = wdt.getMAINChunk().adt;
		if (adtTiles[packedTileIndex].exist == 0)
		{
			// Nothing to do here since there is no ADT file for this map tile
			return true;
		}

		// Calcualte cell index
		const UInt32 cellX = packedTileIndex / 64;
		const UInt32 cellY = packedTileIndex % 64;

		// Only filter cells if we are on the specified map id (and if a map id has been specified)
		if (buildOnlyMap >= 0 && mapId == buildOnlyMap)
		{
			if (buildOnlyTileX >= 0 && buildOnlyTileY >= 0)
			{
				// Not our tile to build
				if (cellX != buildOnlyTileX || cellY != buildOnlyTileY)
					return false;
			}
		}

		// Build NavMesh tiles
		ILOG("\tBuilding adt cell [" << cellX << "," << cellY << "] ...");

		// File name formattings
		const String cellName = fmt::format("{0}_{1}_{2}", mapName, cellY, cellX);
		const String adtFileName = fmt::format("World\\Maps\\{0}\\{1}.adt", mapName, cellName);

		// Parse ADT file
		ADTFile adt(adtFileName);
		if (!adt.load())
		{
			ELOG("Could not load file " << adtFileName);
			return false;
		}

		// Create map file
		const String mapFileName =
			((outputPath / (fmt::format("{0}", mapId))) / (fmt::format("{0}_{1}.map", cellX, cellY))).string();
		std::ofstream fileStrm(mapFileName, std::ios::out | std::ios::binary);
		io::StreamSink sink(fileStrm);
		io::Writer writer(sink);
		
		// Mark header position
		const auto headerChunkPos = sink.position();

        // Create map header chunk
        MapHeaderChunk header;
		createHeaderChunk(header);
		writer.writePOD(header);

        // Create map adt area chunk
        MapAreaChunk areaHeader;
		createAreaChunk(adt, areaHeader);
		header.offsAreaTable = sink.position();
		writer.writePOD(areaHeader);
		header.areaTableSize = sink.position() - header.offsAreaTable;

		// Load WMOs and Doodads
		MapWMOChunk wmoChunk;
		if (!loadADTWmos(adt, wmoChunk))
			return false;
		MapDoodadChunk doodadChunk;
		if (!loadADTDoodads(adt, doodadChunk))
			return false;

		// Serialize WMO chunk
		header.offsWmos = sink.position();
		header.wmoSize = wmoChunk.header.size;
		{
			writer.writePOD(wmoChunk.header);
			writer
				<< io::write<NetUInt32>(wmoChunk.entries.size());
			for (const auto &entry : wmoChunk.entries)
			{
				writer
					<< io::write<NetUInt32>(entry.uniqueId)
					<< io::write_dynamic_range<UInt16>(entry.fileName);
				writer.writePOD(entry.position);
				writer.writePOD(entry.rotation);
			}
		}

		// Serialize Doodad chunk
		header.offsDoodads = sink.position();
		header.doodadSize = doodadChunk.header.size;
		{
			writer.writePOD(doodadChunk.header);
			writer
				<< io::write<NetUInt32>(doodadChunk.entries.size());
			for (const auto &entry : doodadChunk.entries)
			{
				writer
					<< io::write<NetUInt32>(entry.uniqueId)
					<< io::write_dynamic_range<UInt16>(entry.fileName);
				writer.writePOD(entry.position);
				writer.writePOD(entry.rotation);
				writer << io::write<float>(entry.scale);
			}
		}

		// Remember possible start of nav chunk offset
		const auto possibleNavChunkOffset = sink.position();

		// Prepare navigation chunk
		MapNavigationChunk navChunk;
		if (!createNavChunk(mapName, mapId, cellX, cellY, navMesh, adt, wmoChunk, doodadChunk, navChunk))
		{
			ELOG("Could not create nav chunk for cell " << cellX << "," << cellY);
		}
		else if (!navChunk.tiles.empty())
		{
			// Calculate real chunk size
			navChunk.header.size = sizeof(UInt32);
			for (const auto &tile : navChunk.tiles)
			{
				navChunk.header.size += tile.size + sizeof(UInt32);
			}

			// Fix header data
			header.offsNavigation = possibleNavChunkOffset;
			header.navigationSize += sizeof(MapChunkHeader) + navChunk.header.size;

			// Write chunk data
			writer
				<< io::write<UInt32>(navChunk.header.fourCC)
				<< io::write<UInt32>(navChunk.header.size)
				<< io::write<UInt32>(navChunk.tileCount);
			for (const auto &tile : navChunk.tiles)
			{
				writer
					<< io::write<UInt32>(tile.size)
					<< io::write_range(tile.data);
			}
		}

		// Overwrite header chunk with new values
		writer.writePOD(headerChunkPos, header);
		return true;
	}
	
	/// Generates all required data of a given map by it's index in the map dbc file.
	/// This method is called from multiple threads!
	/// @param dbcRow Row id in the map dbc file.
	/// @return false on error, true on success.
	static bool convertMap(UInt32 dbcRow)
	{
		// Get Map values
		UInt32 mapId = 0;
		if (!dbcMap->getValue(dbcRow, 0, mapId))
		{
			return false;
		}

		// Skip maps that aren't used ingame
		switch (mapId)
		{
			case 13:	// Testing
			case 25:	// Scott Test
			case 29:	// Test
			case 35:	// Old Stockades
			case 37:	// Azshara Crater
			case 42:	// Collin's Test
			case 44:	// <unused> Scarlet Monestary
			case 169:	// Emerald Dream
			case 451:	// Development World
			case 598:	// Sunwell Fix (Unused)
				return true;
		}

		String mapName;
		if (!dbcMap->getValue(dbcRow, 1, mapName))
		{
			return false;
		}

		// Skip this map eventually
		if (buildOnlyMap >= 0 && mapId != buildOnlyMap)
		{
			return true;
		}

		// Build map
		ILOG("Building map " << mapId << " - " << mapName << "...");

		// Create the map directory (if it doesn't exist)
		const fs::path mapPath = outputPath / fmt::format("{0}", mapId);
		if (!fs::is_directory(mapPath))
		{
			if (!fs::create_directory(mapPath))
			{
				// Skip this map
				return false;
			}
		}

		// Load the WDT file and get infos out of there
		const String wdtFileName = fmt::format("World\\Maps\\{0}\\{0}.wdt", mapName);
		WDTFile mapWDT(wdtFileName);
		if (!mapWDT.load())
		{
			// Do not warn about invalid map files, since some files are simply missing because 
			// Blizzard removed them. Players weren't able to visit these worlds anyway.
			return false;
		}

		// Get adt tile information
		ILOG("Discovering tiles for map " << mapId << "...");
		auto &adtTiles = mapWDT.getMAINChunk().adt;
		
		// Filter tiles we don't need
		for (UInt32 tile = 0; tile < adtTiles.size(); ++tile)
		{
			if (adtTiles[tile].exist > 0)
			{
				// Calcualte cell index
				const UInt32 cellX = tile / 64;
				const UInt32 cellY = tile % 64;
				tilesByMap[mapId].add(packTileID(cellX, cellY));
			}
		}

		ILOG("Found " << tilesByMap[mapId].size() << " adt tiles");

		// Create nav mesh
		auto freeNavMesh = [](dtNavMesh* ptr) { dtFreeNavMesh(ptr); };
		std::unique_ptr<dtNavMesh, decltype(freeNavMesh)> navMesh(dtAllocNavMesh(), freeNavMesh);
		if (!createNavMesh(mapId, *navMesh))
		{
			ELOG("Failed creating the navigation mesh!");
			return false;
		}

		// Write nav mesh parameters
		const String navParamFile =
			(outputPath / fmt::format("{0}.map", mapId)).string();
		std::ofstream outNavFile(navParamFile, std::ios::out | std::ios::binary);
		if (outNavFile)
		{
			// Write parameter settings
			outNavFile.write(reinterpret_cast<const char*>(navMesh->getParams()), sizeof(dtNavMeshParams));
		}
		else
		{
			ELOG("Could not create map file " << navParamFile);
			return false;
		}

		// Iterate through all tiles and create those which are available
		bool succeeded = true;
		for (UInt32 tile = 0; tile < adtTiles.size(); ++tile)
		{
			if (!convertADT(mapId, mapName, mapWDT, tile, *navMesh))
			{
				succeeded = false;
			}
		}

		return true;
	}
	
	/// Detects the client locale by checking MPQ files for existance.
	/// @param out_locale The locale string will be stored here in the form of 'xxXX'.
	/// @returns False if the client localization couldn't be detected.
	static bool detectLocale(String &out_locale)
	{
		// Enumerate all possible client locales for the Burning Crusade. Note that
		// the locales are placed in order, so enGB will always be perferred over deDE
		// for example.
		std::array<std::string, 16> locales = {
			"enGB",
			"enUS",
			"deDE",
			"esES",
			"frFR",
			"koKR",
			"zhCN",
			"zhTW",
			"enCN",
			"enTW",
			"esMX",
			"ruRU"
		};

		// Iterate through all these locales and check for file existance. If the file exists,
		// we found the local.
		for (auto &locale : locales)
		{
			if (fs::exists(inputPath / "Data" / locale / fmt::format("locale-{0}.MPQ", locale)))
			{
				out_locale = locale;
				return true;
			}
		}

		return false;
	}
	
	/// Loads all MPQ files which are available, including localized ones and patch archives.
	/// @param localeString Client locale string like 'enUS'.
	static void loadMPQFiles(const String &localeString)
	{
		// MPQ archives to load. These files need to stay in order, so that patch-2 is the last
		// loaded archive. Otherwise, wrong files might be extracted since there might be an older
		// version of a file in common or expansion.
		const String commonArchives[] = {
			"common.MPQ",
			"expansion.MPQ",
			"patch.MPQ",
			"patch-2.MPQ"
		};

		// Same as above, but locale dependant. However, order is still important!
		const String localeArchives[] = {
			"locale-{0}.MPQ",
			"patch-{0}.MPQ",
			"patch-{0}-2.MPQ",
		};

		// Try to load all common archives
		for (auto &file : commonArchives)
		{
			const String mpqFileName = (inputPath / "Data" / file).string();
			if (!mpq::loadMPQFile(mpqFileName))
			{
				WLOG("Could not load MPQ archive " << mpqFileName);
			}
		}

		// Now try to load all locale archives
		for (auto &file : localeArchives)
		{
			const String mpqFileName =
				(inputPath / "Data" / localeString / fmt::format(file, localeString)).string();
			if (!mpq::loadMPQFile(mpqFileName))
			{
				WLOG("Could not load MPQ archive " << mpqFileName);
			}
		}
	}
	
	/// Loads all required dbc files.
	/// @returns False if an error occurred.
	static bool loadDBCFiles()
	{
		// Needed to determine which maps do exist
		dbcMap = make_unique<DBCFile>("DBFilesClient\\Map.dbc");
		if (!dbcMap->load()) return false;

		// Needed to determine which areas exist and which ids represent an area,
		// and which areas are zones (an area is the parent of one or more zones)
		dbcAreaTable = make_unique<DBCFile>("DBFilesClient\\AreaTable.dbc");
		if (!dbcAreaTable->load()) return false;

		// Not used right now, but will be used later to determine different liquid
		// types, so that lava for example can do fire damage to players in the lava.
		dbcLiquidType = make_unique<DBCFile>("DBFilesClient\\LiquidType.dbc");
		if (!dbcLiquidType->load()) return false;
        
        // Cache area flags
        for (UInt32 i = 0; i < dbcAreaTable->getRecordCount(); ++i)
        {
            UInt32 areaId = 0, flags = 0;
            dbcAreaTable->getValue(i, 0, areaId);
            dbcAreaTable->getValue(i, 3, flags);
            areaFlags[areaId] = flags;
        }

		return true;
	}
}

/// Procedural entry point of the application.
int main(int argc, char* argv[])
{
	// Handle program options
	std::size_t concurrency = std::thread::hardware_concurrency();
	concurrency = std::max<size_t>(1, concurrency - 1);

	po::options_description desc("WoW++ data extractor, available options");
	desc.add_options()
		("help,h", "produce help message")
		("concurrency,j", po::value(&concurrency), fmt::format("the number of threads to use (defaults to {0} on this machine)", concurrency).c_str())
		("map,m", po::value(&buildOnlyMap), "build only this specific map id")
		("tileX,x", po::value(&buildOnlyTileX), "build only this specific x tile of the specified map")
		("tileY,y", po::value(&buildOnlyTileY), "build only this specific y tile of the specified map")
		("debug,d", po::value(&generateDebugFiles), "produce *.obj mesh files for debugging")
		;

	po::variables_map vm;
	po::positional_options_description p;
	try
	{
		po::store(
			po::command_line_parser(argc, argv).options(desc).positional(p).run(),
			vm);
		po::notify(vm);
	}
	catch (const po::error &e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}

	// Limit cpu count
	concurrency = std::max<size_t>(1, concurrency);

	// Display help message
	if (vm.count("help"))
	{
		std::cerr << desc << '\n';
		return 0;
	}



	// Initialize the log system with support for multithreading
	std::mutex logMutex;
	wowpp::g_DefaultLog.signal().connect([&logMutex](const wowpp::LogEntry &entry)
	{
		std::lock_guard<std::mutex> lock(logMutex);
		wowpp::printLogEntry(std::cout, entry, wowpp::g_DefaultConsoleLogOptions);
	});



	// Try to create output path
	if (!fs::is_directory(outputPath))
	{
		if (!fs::create_directory(outputPath))
		{
			ELOG("Could not create output path directory: " << outputPath);
			return 1;
		}
	}

	// Try to create BVH directory
	if (!fs::is_directory(bvhOutputPath))
	{
		if (!fs::create_directory(bvhOutputPath))
		{
			ELOG("Could not create bvh path: " << bvhOutputPath);
			return 1;
		}
	}

	// Detect client localization
	String currentLocale;
	if (!detectLocale(currentLocale))
	{
		ELOG("Could not detect client localization");
		return 1;
	}

	// Print current localization
	ILOG("Detected locale: " << currentLocale);

	// Load all mpq files
	loadMPQFiles(currentLocale);

	// Load all dbc files
	if (!loadDBCFiles())
	{
		return 1;
	}

	// Show some statistics
	ILOG("Found " << dbcMap->getRecordCount() << " maps");
	ILOG("Found " << dbcAreaTable->getRecordCount() << " areas");
	ILOG("Found " << dbcLiquidType->getRecordCount() << " liquid types");



	// Create work jobs for every map that exists
	boost::asio::io_service dispatcher;
	for (UInt32 i = 0; i < dbcMap->getRecordCount(); ++i)
	{
		dispatcher.post(
			std::bind(convertMap, i));
	}

	// Determine the amount of available cpu cores, and use just as many. However,
	// right now, this will only convert multiple maps at the same time, not multiple
	// tiles - but still A LOT faster than single threaded.
	ILOG("Using " << concurrency << " threads");

	// Do the work!
	std::vector<std::unique_ptr<std::thread>> workers;
	for (size_t i = 1, c = concurrency; i < c; ++i)
	{
		workers.push_back(make_unique<std::thread>(
			[&dispatcher]()
		{
			dispatcher.run();
		}));
	}

	dispatcher.run();

	// Wait for all running workers to finish
	for(auto &worker : workers)
	{
		worker->join();
	}

	// We are done!
	return 0;
}
