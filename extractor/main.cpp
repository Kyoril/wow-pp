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
#include "binary_io/writer.h"
#include "binary_io/stream_sink.h"
#include "common/make_unique.h"
#include "game/map.h"
#include "math/matrix3.h"
#include "math/matrix4.h"
#include "math/vector3.h"
#include "math/quaternion.h"
#include "detour/DetourCommon.h"
#include "detour/DetourNavMesh.h"
#include "detour/DetourNavMeshBuilder.h"
#include "recast/Recast.h"
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

//////////////////////////////////////////////////////////////////////////
// Path variables
static const fs::path inputPath(".");
static const fs::path outputPath("maps");

//////////////////////////////////////////////////////////////////////////
// DBC files
static std::unique_ptr<DBCFile> dbcMap;
static std::unique_ptr<DBCFile> dbcAreaTable;
static std::unique_ptr<DBCFile> dbcLiquidType;

//////////////////////////////////////////////////////////////////////////
// Caches
std::map<UInt32, UInt32> areaFlags;
std::map<UInt32, LinearSet<UInt32>> tilesByMap;

//////////////////////////////////////////////////////////////////////////
// Helper functions
namespace
{
	class MeshSettings
	{
	public:
		static constexpr int ChunksPerTile = 4;
		static constexpr int TileVoxelSize = 450;

		static constexpr float CellHeight = 0.5f;
		static constexpr float WalkableHeight = 1.6f;           // agent height in world units (yards)
		static constexpr float WalkableRadius = 0.3f;           // narrowest allowable hallway in world units (yards)
		static constexpr float WalkableSlope = 50.f;            // maximum walkable slope, in degrees
		static constexpr float WalkableClimb = 1.f;             // maximum 'step' height for which slope is ignored (yards)
		static constexpr float DetailSampleDistance = 3.f;      // heightfield detail mesh sample distance (yards)
		static constexpr float DetailSampleMaxError = 0.75f;    // maximum distance detail mesh surface should deviate from heightfield (yards)

																// NOTE: If Recast warns "Walk towards polygon center failed to reach center", try lowering this value
		static constexpr float MaxSimplificationError = 0.5f;

		static constexpr int MinRegionSize = 1600;
		static constexpr int MergeRegionSize = 400;
		static constexpr int VerticesPerPolygon = 6;

		// Nothing below here should ever have to change

		static constexpr int Adts = 64;
		static constexpr int ChunksPerAdt = 16;
		static constexpr int TilesPerADT = ChunksPerAdt / ChunksPerTile;
		static constexpr int TileCount = Adts * TilesPerADT;
		static constexpr int ChunkCount = Adts * ChunksPerAdt;

		static constexpr float AdtSize = 533.f + (1.f / 3.f);
		static constexpr float AdtChunkSize = AdtSize / ChunksPerAdt;

		static constexpr float TileSize = AdtChunkSize * ChunksPerTile;
		static constexpr float CellSize = TileSize / TileVoxelSize;
		static constexpr int VoxelWalkableRadius = static_cast<int>(WalkableRadius / CellSize);
		static constexpr int VoxelWalkableHeight = static_cast<int>(WalkableHeight / CellHeight);
		static constexpr int VoxelWalkableClimb = static_cast<int>(WalkableClimb / CellHeight);

		static_assert(WalkableRadius > CellSize, "CellSize must be able to approximate walkable radius");
		static_assert(WalkableHeight > CellSize, "CellSize must be able to approximate walkable height");
		static_assert(ChunksPerAdt % ChunksPerTile == 0, "Chunks per tile must divide chunks per ADT (16)");
		static_assert(VoxelWalkableRadius > 0, "VoxelWalkableRadius must be a positive integer");
		static_assert(VoxelWalkableHeight > 0, "VoxelWalkableHeight must be a positive integer");
		static_assert(VoxelWalkableClimb >= 0, "VoxelWalkableClimb must be non-negative integer");
		static_assert(CellSize > 0.f, "CellSize must be positive");
	};

	/// Represents mesh data used for navigation mesh generation
	struct MeshData final
	{
		/// Three coordinates represent one vertex (x, y, z)
		std::vector<float> solidVerts;
		/// Three indices represent one triangle (v1, v2, v3)
		std::vector<int> solidTris;
		/// Triangle flags
		std::vector<unsigned char> triangleFlags;
	};

	using SmartHeightFieldPtr = std::unique_ptr<rcHeightfield, decltype(&rcFreeHeightField)>;
	using SmartCompactHeightFieldPtr = std::unique_ptr<rcCompactHeightfield, decltype(&rcFreeCompactHeightfield)>;
	using SmartContourSetPtr = std::unique_ptr<rcContourSet, decltype(&rcFreeContourSet)>;
	using SmartPolyMeshPtr = std::unique_ptr<rcPolyMesh, decltype(&rcFreePolyMesh)>;
	using SmartPolyMeshDetailPtr = std::unique_ptr<rcPolyMeshDetail, decltype(&rcFreePolyMeshDetail)>;

	/// This is the length of an edge of a map file in ingame units where one unit 
	/// represents one meter.
	const float GridSize = 533.3333f;
	const float GridPart = GridSize / 128;

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

	enum Spot
	{
		TOP = 1,
		RIGHT = 2,
		LEFT = 3,
		BOTTOM = 4,
		ENTIRE = 5
	};

	enum Grid
	{
		GRID_V8,
		GRID_V9
	};

	enum AreaFlags : unsigned char
	{
		Walkable	= 1 << 0,
		ADT			= 1 << 1,
		Liquid		= 1 << 2,
		WMO			= 1 << 3,
		Doodad		= 1 << 4,
	};

	using PolyFlags = AreaFlags;

	static const int V9_SIZE = 129;
	static const int V9_SIZE_SQ = V9_SIZE * V9_SIZE;
	static const int V8_SIZE = 128;
	static const int V8_SIZE_SQ = V8_SIZE * V8_SIZE;
	static const float GRID_SIZE = 533.33333f;
	static const float GRID_PART_SIZE = GRID_SIZE / V8_SIZE;

	static void getHeightCoord(UInt32 index, Grid grid, float xOffset, float yOffset, float* coord, const float* v)
	{
		// wow coords: x, y, height
		// coord is mirroed about the horizontal axes
		switch (grid)
		{
			case GRID_V9:
				coord[0] = (xOffset + index % (V9_SIZE)* GRID_PART_SIZE) * -1.f;
				coord[1] = (yOffset + (int)(index / (V9_SIZE)) * GRID_PART_SIZE) * -1.f;
				coord[2] = v[index];
				break;
			case GRID_V8:
				coord[0] = (xOffset + index % (V8_SIZE)* GRID_PART_SIZE + GRID_PART_SIZE / 2.f) * -1.f;
				coord[1] = (yOffset + (int)(index / (V8_SIZE)) * GRID_PART_SIZE + GRID_PART_SIZE / 2.f) * -1.f;
				coord[2] = v[index];
				break;
		}
	}
	
	static void getHeightTriangle(UInt32 square, Spot triangle, int* indices)
	{
		int rowOffset = square / V8_SIZE;
		switch (triangle)
		{
			case TOP:
				indices[0] = square + rowOffset;                //           0-----1 .... 128
				indices[1] = square + 1 + rowOffset;            //           |\ T /|
				indices[2] = (V9_SIZE_SQ)+square;				//           | \ / |
				break;                                          //           |L 0 R| .. 127
			case LEFT:                                          //           | / \ |
				indices[0] = square + rowOffset;                //           |/ B \|
				indices[1] = (V9_SIZE_SQ)+square;				//          129---130 ... 386
				indices[2] = square + V9_SIZE + rowOffset;      //           |\   /|
				break;                                          //           | \ / |
			case RIGHT:                                         //           | 128 | .. 255
				indices[0] = square + 1 + rowOffset;            //           | / \ |
				indices[1] = square + V9_SIZE + 1 + rowOffset;  //           |/   \|
				indices[2] = (V9_SIZE_SQ)+square;				//          258---259 ... 515
				break;
			case BOTTOM:
				indices[0] = (V9_SIZE_SQ)+square;
				indices[1] = square + V9_SIZE + 1 + rowOffset;
				indices[2] = square + V9_SIZE + rowOffset;
				break;
			default: break;
		}
	}
	
	static void getLoopVars(Spot portion, int& loopStart, int& loopEnd, int& loopInc)
	{
		switch (portion)
		{
			case ENTIRE:
				loopStart = 0;
				loopEnd = V8_SIZE_SQ;
				loopInc = 1;
				break;
			case TOP:
				loopStart = 0;
				loopEnd = V8_SIZE;
				loopInc = 1;
				break;
			case LEFT:
				loopStart = 0;
				loopEnd = V8_SIZE_SQ - V8_SIZE + 1;
				loopInc = V8_SIZE;
				break;
			case RIGHT:
				loopStart = V8_SIZE - 1;
				loopEnd = V8_SIZE_SQ;
				loopInc = V8_SIZE;
				break;
			case BOTTOM:
				loopStart = V8_SIZE_SQ - V8_SIZE;
				loopEnd = V8_SIZE_SQ;
				loopInc = 1;
				break;
		}
	}

	/// This method calculates the boundaries of a given map.
	static void calculateMapBounds(UInt32 mapId, UInt32 &out_minX, UInt32 &out_minY, UInt32 &out_maxX, UInt32 &out_maxY)
	{
		auto it = tilesByMap.find(mapId);
		if (it == tilesByMap.end())
			return;

		out_minX = std::numeric_limits<UInt32>::max();
		out_maxX = std::numeric_limits<UInt32>::min();
		out_minY = std::numeric_limits<UInt32>::max();
		out_maxY = std::numeric_limits<UInt32>::min();

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
	/// Calculates tile boundaries in world units, but converted to the recast coordinate
	/// system.
	static void calculateTileBounds(UInt32 tileX, UInt32 tileY, float* bmin, float* bmax)
	{
		bmin[0] = (32 - int(tileX)) * -MeshSettings::AdtSize;
		bmin[1] = std::numeric_limits<float>::max();
		bmin[2] = (32 - int(tileY)) * -MeshSettings::AdtSize;

		bmax[0] = bmin[0] + MeshSettings::AdtSize / MeshSettings::ChunksPerTile;
		bmax[1] = -1000.0f;
		bmax[2] = bmin[2] + MeshSettings::AdtSize / MeshSettings::ChunksPerTile;
	}

	static UInt16 holetab_h[4] = { 0x1111, 0x2222, 0x4444, 0x8888 };
	static UInt16 holetab_v[4] = { 0x000F, 0x00F0, 0x0F00, 0xF000 };

	static bool isHole(int square, const ADTFile& adt)
	{
		int row = square / 128;
		int col = square % 128;
		int cellRow = row / 8;     // 8 squares per cell
		int cellCol = col / 8;
		int holeRow = row % 8 / 2;
		int holeCol = (square - (row * 128 + cellCol * 8)) / 2;

		const UInt16 &hole = adt.getMCNKChunk(cellRow + cellCol * 16).holes;
		return (hole & holetab_v[holeCol] & holetab_h[holeRow]) != 0;
	}

	/// 
	struct NavTile
	{
		NavTile() : chf(NULL), solid(NULL), cset(NULL), pmesh(NULL), dmesh(NULL) {}
		~NavTile()
		{
			rcFreeCompactHeightfield(chf);
			rcFreeContourSet(cset);
			rcFreeHeightField(solid);
			rcFreePolyMesh(pmesh);
			rcFreePolyMeshDetail(dmesh);
		}
		rcCompactHeightfield* chf;
		rcHeightfield* solid;
		rcContourSet* cset;
		rcPolyMesh* pmesh;
		rcPolyMeshDetail* dmesh;
	};
	
	/// Writes data of a certain map mesh to an obj file.
	static void serializeMeshData(UInt32 mapID, UInt32 tileX, UInt32 tileY, MeshData& meshData)
	{
		char objFileName[255];
		sprintf(objFileName, "meshes/map%03u%02u%02u.obj", mapID, tileY, tileX);

		FILE* objFile = fopen(objFileName, "wb");
		if (!objFile)
		{
			char message[1024];
			sprintf(message, "Failed to open %s for writing!\n", objFileName);
			perror(message);
			return;
		}

		float* verts = &meshData.solidVerts[0];
		int vertCount = meshData.solidVerts.size() / 3;
		int* tris = &meshData.solidTris[0];
		int triCount = meshData.solidTris.size() / 3;

		for (int i = 0; i < meshData.solidVerts.size() / 3; i++)
			fprintf(objFile, "v %f %f %f\n", verts[i * 3], verts[i * 3 + 1], verts[i * 3 + 2]);

		for (int i = 0; i < meshData.solidTris.size() / 3; i++)
			fprintf(objFile, "f %i %i %i\n", tris[i * 3] + 1, tris[i * 3 + 1] + 1, tris[i * 3 + 2] + 1);

		fclose(objFile);
	}

	// Code taken from tripleslash
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
						assert(nx >= 0 && ny >= 0 && nx < chf.width && ny < chf.height);

						auto const &neighborCell = chf.cells[ny*chf.width + nx];
						auto const &neighborSpan = chf.spans[k + neighborCell.index];

						// if the span height difference is <= the walkable climb, nothing else matters.  skip it
						if (rcAbs(static_cast<int>(neighborSpan.y) - static_cast<int>(span.y)) <= walkableClimb)
							continue;

						const NavTerrain neighborSpanArea = static_cast<NavTerrain>(chf.areas[k + neighborCell.index]);

						// if both the current span and the neighbor span are ADTs, do nothing
						if ((spanArea & PolyFlags::ADT) != 0 && (neighborSpanArea & PolyFlags::ADT) != 0)
							continue;

						//std::cout << "Removing connection for span " << i << " dir " << dir << " to " << k << std::endl;
						rcSetCon(span, dir, RC_NOT_CONNECTED);
					}
				}
			}
		}
	}

	/// Finishes the nav mesh generation.
	bool finishMesh(rcContext &ctx, const rcConfig &config, int tileX, int tileY, MapNavigationChunk &out_chunk, rcHeightfield &solid)
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
		assert(!!cset->nconts);

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
		params.tileX = tileX;
		params.tileY = tileY;
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

		// Write the serialized data into the navigation chunk of the map
		out_chunk.data.resize(outDataSize);
		std::memcpy(&out_chunk.data[0], outData, outDataSize);
		out_chunk.tileRef = 0;

#if 1
		const int nvp = polyMesh->nvp;
		const float cs = polyMesh->cs;
		const float ch = polyMesh->ch;
		const float* orig = polyMesh->bmin;
		int nIndex = 0;

		if (polyMesh->npolys > 0)
		{
			std::ostringstream strm;
			strm << "meshes/tile_" << tileX << "_" << tileY << ".obj";
			std::ofstream outFile(strm.str().c_str(), std::ios::out);
			for (int i = 0; i < polyMesh->npolys; ++i) // go through all polygons
			{
				const unsigned short* p = &polyMesh->polys[i*nvp * 2];

				unsigned short vi[3];
				for (int j = 2; j < nvp; ++j) // go through all verts in the polygon
				{
					if (p[j] == RC_MESH_NULL_IDX) break;

					vi[0] = p[0];
					vi[1] = p[j - 1];
					vi[2] = p[j];

					for (int k = 0; k < 3; ++k) // create a 3-vert triangle for each 3 verts in the polygon.
					{
						const unsigned short* v = &polyMesh->verts[vi[k] * 3];
						const float x = orig[0] + v[0] * cs;
						const float y = orig[1] + (v[1] + 1)*ch;
						const float z = orig[2] + v[2] * cs;

						outFile << "v " << x << " " << y << " " << z << std::endl;
					}

					outFile << "f " << nIndex + 1 << " " << nIndex + 2 << " " << nIndex + 3 << std::endl;
					nIndex += 3;
				}
			}
		}
#endif

		dtFree(outData);
		return true;
	}

	/// Restores the area flags of all listed ADT spans in case there flags have been changed.
	void restoreAdtSpans(const std::vector<rcSpan *> &spans)
	{
		for (auto s : spans)
			s->area |= AreaFlags::ADT;
	}

	/// Initializes a rcConfig struct without settings it's boundaries.
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

	/// 
	bool rasterize(rcContext &ctx, rcHeightfield &heightField, bool filterWalkable, float slope, const MeshData &mesh, unsigned char areaFlags)
	{
		if (!mesh.solidVerts.size() || !mesh.solidTris.size())
			return true;

		std::vector<unsigned char> areas(mesh.solidTris.size() / 3, areaFlags);

		if (filterWalkable)
			rcClearUnwalkableTriangles(&ctx, slope, &mesh.solidVerts[0], static_cast<int>(mesh.solidVerts.size() / 3), &mesh.solidTris[0], static_cast<int>(mesh.solidTris.size() / 3), &areas[0]);

		rcRasterizeTriangles(&ctx, &mesh.solidVerts[0], static_cast<int>(mesh.solidVerts.size() / 3), &mesh.solidTris[0], &areas[0], static_cast<int>(mesh.solidTris.size() / 3), heightField);
		return true;
	}

	/// Generates navigation mesh for one map.
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
		calculateTileBounds(maxX, maxY, bmin, bmax);

		//int polyBits = 20;
		int maxTiles = tiles.size() * (MeshSettings::TilesPerADT * MeshSettings::TilesPerADT);
		int maxPolysPerTile = 1 << DT_POLY_BITS;

		// Setup navigation mesh creation parameters
		dtNavMeshParams navMeshParams;
		memset(&navMeshParams, 0, sizeof(dtNavMeshParams));
		navMeshParams.tileWidth = MeshSettings::TileSize;// GridSize;
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

		return true;
	}

	/// Serializes the terrain mesh of a given ADT file and adds it's vertices to the provided mesh object.
	static bool addTerrainMesh(const ADTFile &adt, UInt32 tileX, UInt32 tileY, Spot spot, MeshData &mesh)
	{
		static_assert(V8_SIZE == 128, "V8_SIZE has to equal 128");
		std::array<float, 128 * 128> V8;
		V8.fill(0.0f);
		static_assert(V9_SIZE == 129, "V9_SIZE has to equal 129");
		std::array<float, 129 * 129> V9;
		V9.fill(0.0f);

		UInt32 chunkIndex = 0;
		for (UInt32 i = 0; i < 16; ++i)
		{
			for (UInt32 j = 0; j < 16; ++j)
			{
				auto &MCNK = adt.getMCNKChunk(j + i * 16);
				auto &MCVT = adt.getMCVTChunk(j + i * 16);

				// get V9 height map
				for (int y = 0; y <= 8; y++)
				{
					int cy = i * 8 + y;
					for (int x = 0; x <= 8; x++)
					{
						int cx = j * 8 + x;
						V9[cy + cx * V9_SIZE] = MCVT.heights[y * (8 * 2 + 1) + x] + MCNK.ypos;
					}
				}
				// get V8 height map
				for (int y = 0; y < 8; y++)
				{
					int cy = i * 8 + y;
					for (int x = 0; x < 8; x++)
					{
						int cx = j * 8 + x;
						V8[cy + cx * V8_SIZE] = MCVT.heights[y * (8 * 2 + 1) + 8 + 1 + x] + MCNK.ypos;
					}
				}
			}
		}

		int count = mesh.solidVerts.size() / 3;
		float xoffset = (float(tileX) - 32) * GridSize;
		float yoffset = (float(tileY) - 32) * GridSize;

		float coord[3];
		for (int i = 0; i < V9_SIZE_SQ; ++i)
		{
			getHeightCoord(i, GRID_V9, xoffset, yoffset, coord, V9.data());
			mesh.solidVerts.push_back(-coord[1]);
			mesh.solidVerts.push_back(coord[2]);
			mesh.solidVerts.push_back(-coord[0]);
		}
		for (int i = 0; i < V8_SIZE_SQ; ++i)
		{
			getHeightCoord(i, GRID_V8, xoffset, yoffset, coord, V8.data());
			mesh.solidVerts.push_back(-coord[1]);
			mesh.solidVerts.push_back(coord[2]);
			mesh.solidVerts.push_back(-coord[0]);
		}

		int loopStart, loopEnd, loopInc;
		int indices[3];

		getLoopVars(spot, loopStart, loopEnd, loopInc);
		for (int i = loopStart; i < loopEnd; i += loopInc)
		{
			for (int j = TOP; j <= BOTTOM; j += 1)
			{
				if (!isHole(i, adt))
				{
					getHeightTriangle(i, Spot(j), indices);
					mesh.solidTris.push_back(indices[0] + count);
					mesh.solidTris.push_back(indices[1] + count);
					mesh.solidTris.push_back(indices[2] + count);
					mesh.triangleFlags.push_back(AreaFlags::ADT);
				}
			}
		}

		return true;
	}

	/// Generates a navigation tile.
	static bool creaveNavTile(const String &mapName, UInt32 mapId, UInt32 tileX, UInt32 tileY, dtNavMesh &navMesh, const ADTFile &adt, const MapCollisionChunk &collision, MapNavigationChunk &out_chunk)
	{
		// Min and max height values used for recast
		float minZ = std::numeric_limits<float>::max(), maxZ = std::numeric_limits<float>::lowest();

		// Reset chunk data
		out_chunk.data.clear();
		out_chunk.size = 0;
		out_chunk.tileRef = 0;

		// Process WMOs
		MeshData wmoMesh;
		{
			// Add vertices
			for (auto &vert : collision.vertices)
			{
				wmoMesh.solidVerts.push_back(vert.x);
				wmoMesh.solidVerts.push_back(vert.z);
				wmoMesh.solidVerts.push_back(vert.y);
			}
			// Add triangles
			for (auto &tri : collision.triangles)
			{
				wmoMesh.solidTris.push_back(tri.indexC);
				wmoMesh.solidTris.push_back(tri.indexB);
				wmoMesh.solidTris.push_back(tri.indexA);
				wmoMesh.triangleFlags.push_back(AreaFlags::WMO);
			}
		}

		// Process ADTs
		MeshData adtMesh;
		{
			// Now we add adts
			if (addTerrainMesh(adt, tileX, tileY, ENTIRE, adtMesh))
			{
				String adtFile;
				std::unique_ptr<ADTFile> adtInst;

#define LOAD_TERRAIN(x, y, spot) \
				adtFile = fmt::format("World\\Maps\\{0}\\{0}_{1}_{2}.adt", mapName, (y), (x)); \
					adtInst = make_unique<ADTFile>(adtFile); \
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
#if 0
		MeshData doodadMesh;
		{
			DLOG("\tTile has " << adt.getMDDFChunk().entries.size() << " doodads");

			// Now load all required M2 files
			std::vector<std::unique_ptr<M2File>> m2s;
			for (UInt32 i = 0; i < adt.getMDXCount(); ++i)
			{
				// Retrieve MDX file name and replace *.mdx with *.m2
				String filename = adt.getMDX(i);
				if (filename.length() > 4)
				{
					filename = filename.substr(0, filename.length() - 3);
					filename.append("m2");
				}

				// Try to load the respective m2 file
				auto m2File = make_unique<M2File>(filename);
				if (!m2File->load())
				{
					ELOG("Error loading MDX: " << filename);
					return false;
				}

				// Push back to the list of MDX files
				m2s.push_back(std::move(m2File));
			}

			// Load MDDF entries now that we loaded all mesh files
			for (const auto &entry : adt.getMDDFChunk().entries)
			{
				// Entry placement
				auto &m2 = m2s[entry.mmidEntry];
				
#define WOWPP_DEG_TO_RAD(x) (3.14159265358979323846 * (x) / -180.0)
				math::Matrix3 rotMat = math::Matrix3::fromEulerAnglesXYZ(
					WOWPP_DEG_TO_RAD(-entry.rotation[2]), WOWPP_DEG_TO_RAD(-entry.rotation[0]), WOWPP_DEG_TO_RAD(-entry.rotation[1] - 180));

				math::Vector3 position(entry.position.z, entry.position.x, entry.position.y);
				position.x = (32 * 533.3333f) - position.x;
				position.y = (32 * 533.3333f) - position.y;
#undef WOWPP_DEG_TO_RAD
				
				// Transform vertices
				const auto &verts = m2->getVertices();
				const auto &inds = m2->getIndices();

				UInt32 count = mesh.solidVerts.size() / 3;
				for (auto &vert : verts)
				{
					// Transform vertex and push it to the list
					math::Vector3 transformed = rotMat * (vert * (float(entry.scale) / 1024.0f));
					doodadMesh.solidVerts.push_back(transformed.x + position.x);
					doodadMesh.solidVerts.push_back(transformed.y + position.y);
					doodadMesh.solidVerts.push_back(transformed.z + position.z);
				}
				for (UInt32 i = 0; i < inds.size(); i += 3)
				{
					Triangle tri;
					tri.indexA = inds[i] + count;
					tri.indexB = inds[i + 1] + count;
					tri.indexC = inds[i + 2] + count;
					doodadMesh.solidTris.push_back(tri.indexA);
					doodadMesh.solidTris.push_back(tri.indexB);
					doodadMesh.solidTris.push_back(tri.indexC);
					doodadMesh.triangleFlags.push_back(AreaFlags::Doodad);
				}
			}
		}
#endif

		// Serialize adt data
		serializeMeshData(mapId, tileX, tileY, adtMesh);

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

		// Initialize config
		rcConfig config;
		initializeRecastConfig(config);

		// Setup boundaries
		float bmin[3], bmax[3];
		calculateTileBounds(tileX, tileY, bmin, bmax);
		rcVcopy(config.bmin, bmin);
		rcVcopy(config.bmax, bmax);

		UInt32 convertedTileX = tileX * MeshSettings::ChunksPerTile;
		UInt32 convertedTileY = tileY * MeshSettings::ChunksPerTile;

		config.bmin[0] = (convertedTileX * MeshSettings::ChunksPerTile) * MeshSettings::AdtChunkSize - 32.f * MeshSettings::AdtSize;
		config.bmin[1] = minZ;
		config.bmin[2] = (convertedTileY * MeshSettings::ChunksPerTile) * MeshSettings::AdtChunkSize - 32.f * MeshSettings::AdtSize;
		config.bmax[0] = ((convertedTileX + 1) * MeshSettings::ChunksPerTile) * MeshSettings::AdtChunkSize - 32.f * MeshSettings::AdtSize;
		config.bmax[1] = maxZ;
		config.bmax[2] = ((convertedTileY + 1) * MeshSettings::ChunksPerTile) * MeshSettings::AdtChunkSize - 32.f * MeshSettings::AdtSize;

		config.bmin[0] -= config.borderSize * config.cs;
		config.bmin[2] -= config.borderSize * config.cs;
		config.bmax[0] += config.borderSize * config.cs;
		config.bmax[2] += config.borderSize * config.cs;

		// Create the context object
		rcContext ctx(false);

		// Allocate and build the recast heightfield
		SmartHeightFieldPtr solid(rcAllocHeightfield(), rcFreeHeightField);
		if (!rcCreateHeightfield(&ctx, *solid, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch))
		{
			ELOG("Could not create heightfield (rcCreateHeightfield failed)");
			return false;
		}

		// Rasterize adt terrain mesh
		if (!rasterize(ctx, *solid, false, config.walkableSlopeAngle, adtMesh, AreaFlags::ADT))
		{
			ELOG("Could not rasterize ADT data");
			return false;
		}

		// Rasterize adt wmo object meshes
		if (!rasterize(ctx, *solid, true, config.walkableSlopeAngle, wmoMesh, AreaFlags::WMO))
		{
			ELOG("Could not rasterize WMO data");
			return false;
		}

		// Remember all ADT flagged spans as the information may get lost after the next step
		std::vector<rcSpan *> adtSpans;
		adtSpans.reserve(solid->width*solid->height);
		for (int i = 0; i < solid->width * solid->height; ++i)
			for (rcSpan *s = solid->spans[i]; s; s = s->next)
				if (!!(s->area & AreaFlags::ADT))
					adtSpans.push_back(s);

		// Filter ledge spans
		rcFilterLedgeSpans(&ctx, config.walkableHeight, config.walkableClimb, *solid);

		// Restore ADT flags for previously remembered spans
		restoreAdtSpans(adtSpans);

		// Apply more geometry filtering
		rcFilterWalkableLowHeightSpans(&ctx, config.walkableHeight, *solid);
		rcFilterLowHangingWalkableObstacles(&ctx, config.walkableClimb, *solid);

		// Finalize the mesh
		auto const result = finishMesh(ctx, config, tileX, tileY, out_chunk, *solid);
		if (!result)
		{
			ELOG("Could not finish mesh");
		}

		return result;
	}
	
	/// Converts an ADT tile of a WDT file.
	static bool convertADT(UInt32 mapId, const String &mapName, WDTFile &wdt, UInt32 tileIndex, dtNavMesh &navMesh)
	{
		// Check tile
		auto &adtTiles = wdt.getMAINChunk().adt;
		if (adtTiles[tileIndex].exist == 0)
		{
			// Nothing to do here since there is no ADT file for this map tile
			return true;
		}

		// Calcualte cell index
		const UInt32 cellX = tileIndex / 64;
		const UInt32 cellY = tileIndex % 64;

#if 1
		switch (mapId)
		{
			case 0:
				if (cellY != 36 || cellX != 26)
					return false;
				break;
			case 1:
				if (cellY != 31 || cellX != 19)
					return false;
				break;
			case 489:
				if (cellY != 29 || cellX != 29)
					return false;
				break;
			default:
				break;
		}
#endif

		// Build file names
		const String cellName =
			fmt::format("{0}_{1}_{2}", mapName, cellY, cellX);
		const String adtFile =
			fmt::format("World\\Maps\\{0}\\{1}.adt", mapName, cellName);
		const String mapFile =
			((outputPath / (fmt::format("{0}", mapId))) / (fmt::format("{0}_{1}.map", cellX, cellY))).string();

		// Build NavMesh tiles
		ILOG("\tBuilding tile [" << cellX << "," << cellY << "] ...");

		// Load ADT file
		ADTFile adt(adtFile);
		if (!adt.load())
		{
			ELOG("Could not load file " << adtFile);
			return false;
		}

		// Now load all required WMO files
		std::vector<std::unique_ptr<WMOFile>> wmos;
		for (UInt32 i = 0; i < adt.getWMOCount(); ++i)
		{
			// Retrieve WMO file name
			const String filename = adt.getWMO(i);
			auto wmoFile = make_unique<WMOFile>(filename);
			if (!wmoFile->load())
			{
				ELOG("Error loading WMO: " << filename);
				return false;
			}

			// Push back to the list of WMO files
			wmos.push_back(std::move(wmoFile));
		}

		// Create files
		std::ofstream fileStrm(mapFile, std::ios::out | std::ios::binary);
		io::StreamSink sink(fileStrm);
		io::Writer writer(sink);
		
		// Mark header position
		const size_t headerPos = sink.position();

        // Create map header chunk
        MapHeaderChunk header;
        header.fourCC = 0x50414D57;				// WMAP		- WoW Map
        header.size = sizeof(MapHeaderChunk) - 8;
        header.version = 0x120;
        header.offsAreaTable = 0;
        header.areaTableSize = 0;
		header.offsCollision = 0;
		header.collisionSize = 0;
		header.offsNavigation = 0;
		header.navigationSize = 0;
		writer.writePOD(header);

		// Mark area header chunk
		header.offsAreaTable = sink.position();

        // Area header chunk
        MapAreaChunk areaHeader;
        areaHeader.fourCC = 0x52414D57;			// WMAR		- WoW Map Areas
        areaHeader.size = sizeof(MapAreaChunk) - 8;
        for (size_t i = 0; i < 16 * 16; ++i)
        {
            UInt32 areaId = adt.getMCNKChunk(i).areaid;
            UInt32 flags = 0;
            auto it = areaFlags.find(areaId);
            if (it != areaFlags.end())
            {
                flags = it->second;
            }
            areaHeader.cellAreas[i].areaId = areaId;
            areaHeader.cellAreas[i].flags = flags;
        }
		writer.writePOD(areaHeader);
		header.areaTableSize = sink.position() - header.offsAreaTable;

		// Collision chunk
		MapCollisionChunk collisionChunk;
		collisionChunk.fourCC = 0x4C434D57;			// WMCL		- WoW Map Collision
		collisionChunk.size = sizeof(UInt32) * 4;
		collisionChunk.vertexCount = 0;
		collisionChunk.triangleCount = 0;
		for (const auto &entry : adt.getMODFChunk().entries)
		{
			// Entry placement
			auto &wmo = wmos[entry.mwidEntry];
			if (!wmo->isRootWMO())
			{
				WLOG("Group wmo placed, but root wmo expected");
				continue;
			}

#define WOWPP_DEG_TO_RAD(x) (3.14159265358979323846 * (x) / -180.0)
			math::Matrix3 rotMat = math::Matrix3::fromEulerAnglesXYZ(
				WOWPP_DEG_TO_RAD(-entry.rotation[2]), WOWPP_DEG_TO_RAD(-entry.rotation[0]), WOWPP_DEG_TO_RAD(-entry.rotation[1] - 180));
			
			math::Vector3 position(entry.position.z, entry.position.x, entry.position.y);
			position.x = (32 * 533.3333f) - position.x;
			position.y = (32 * 533.3333f) - position.y;
#undef WOWPP_DEG_TO_RAD

			// Transform vertices
			for (auto &group : wmo->getGroups())
			{
				UInt32 groupStartIndex = collisionChunk.vertexCount;

				const auto &verts = group->getVertices();
				const auto &inds = group->getIndices();

				collisionChunk.vertexCount += verts.size();
				UInt32 groupTris = inds.size() / 3;
				for (auto &vert : verts)
				{
					// Transform vertex and push it to the list
					math::Vector3 transformed = (rotMat * vert) + position;
					collisionChunk.vertices.push_back(transformed);
				}
				for (UInt32 i = 0; i < inds.size(); i += 3)
				{
					if (!group->isCollisionTriangle(i / 3))
					{
						// Skip this triangle
						groupTris--;
						continue;
					}

					Triangle tri;
					tri.indexA = inds[i] + groupStartIndex;
					tri.indexB = inds[i+1] + groupStartIndex;
					tri.indexC = inds[i+2] + groupStartIndex;
					collisionChunk.triangles.emplace_back(std::move(tri));
				}

				collisionChunk.triangleCount += groupTris;
			}
		}
		collisionChunk.size += sizeof(math::Vector3) * collisionChunk.vertexCount;
		collisionChunk.size += sizeof(UInt32) * 3 * collisionChunk.triangleCount;
		header.collisionSize = collisionChunk.size;
		if (collisionChunk.vertexCount > 0)
		{
			header.offsCollision = sink.position();
			if (header.offsCollision)
			{
				writer
					<< io::write<UInt32>(collisionChunk.fourCC)
					<< io::write<UInt32>(collisionChunk.size)
					<< io::write<UInt32>(collisionChunk.vertexCount)
					<< io::write<UInt32>(collisionChunk.triangleCount);
				for (auto &vert : collisionChunk.vertices)
				{
					writer
						<< io::write<float>(vert.x)
						<< io::write<float>(vert.y)
						<< io::write<float>(vert.z);
				}
				for (auto &triangle : collisionChunk.triangles)
				{
					writer
						<< io::write<UInt32>(triangle.indexA)
						<< io::write<UInt32>(triangle.indexB)
						<< io::write<UInt32>(triangle.indexC);
				}
			}
			header.collisionSize = sink.position() - header.offsCollision;
		}

		// Build nav mesh
		MapNavigationChunk navigationChunk;
		navigationChunk.fourCC = 0x564E4D57;		// WMNV		- WoW Map Navigation
		navigationChunk.size = 0;
		if (creaveNavTile(mapName, mapId, cellX, cellY, navMesh, adt, collisionChunk, navigationChunk))
		{
			if (!navigationChunk.data.empty())
			{
				// Calculate real chunk size
				navigationChunk.size = sizeof(dtTileRef) + navigationChunk.data.size();

				// Fix header
				header.offsNavigation = sink.position();
				header.navigationSize = sizeof(UInt32) * 2 + navigationChunk.size;
				writer
					<< io::write<UInt32>(navigationChunk.fourCC)
					<< io::write<UInt32>(navigationChunk.size)
					<< io::write<UInt32>(navigationChunk.tileRef)
					<< io::write_range(navigationChunk.data);
			}
		}
		else
		{
			ELOG("Could not create navigation data for tile " << cellX << "," << cellY);
		}
		
		// Overwrite header settings
		writer.writePOD(headerPos, header);

		return true;
	}
	
	/// Converts a map.
	static bool convertMap(UInt32 dbcRow)
	{
		// Get Map values
		UInt32 mapId = 0;
		if (!dbcMap->getValue(dbcRow, 0, mapId))
		{
			return false;;
		}

		String mapName;
		if (!dbcMap->getValue(dbcRow, 1, mapName))
		{
			return false;
		}

#if 1
		if (mapId != 489)
		{
			return true;
		}
#endif

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

		ILOG("Found " << tilesByMap[mapId].size() << " tiles");
		
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
	
	/// Detects the client locale by checking files for existance.
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
	// Multithreaded log support
	boost::mutex logMutex;
	wowpp::g_DefaultLog.signal().connect([&logMutex](const wowpp::LogEntry &entry)
	{
		boost::mutex::scoped_lock lock(logMutex);
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
	std::size_t concurrency = 1;//boost::thread::hardware_concurrency();
	concurrency = std::max<size_t>(1, concurrency - 1);
	ILOG("Using " << concurrency << " threads");

	// Do the work!
	std::vector<std::unique_ptr<boost::thread>> workers;
	for (size_t i = 1, c = concurrency; i < c; ++i)
	{
		workers.push_back(make_unique<boost::thread>(
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
