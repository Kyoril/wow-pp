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
#include "math/matrix4.h"
#include "math/vector3.h"
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

	enum NavTerrain
	{
		NAV_EMPTY = 0x00,
		NAV_GROUND = 0x01,
		NAV_MAGMA = 0x02,
		NAV_SLIME = 0x04,
		NAV_WATER = 0x08,
		NAV_UNUSED1 = 0x10,
		NAV_UNUSED2 = 0x20,
		NAV_UNUSED3 = 0x40,
		NAV_UNUSED4 = 0x80
		// we only have 8 bits
	};

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
			if (tileX > out_maxY) out_maxY = tileY;
		}
	}
	/// Calculates tile boundaries in world units, but converted to the recast coordinate
	/// system.
	static void calculateTileBounds(UInt32 tileX, UInt32 tileY, float* bmin, float* bmax)
	{
		bmax[0] = (32 - int(tileX)) * GridSize;
		bmax[1] = std::numeric_limits<float>::max();
		bmax[2] = (32 - int(tileY)) * GridSize;

		bmin[0] = bmax[0] - GridSize;
		bmin[1] = std::numeric_limits<float>::min();
		bmin[2] = bmax[2] - GridSize;
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

	/// Represents mesh data used for navigation mesh generation
	struct MeshData final
	{
		/// Three coordinates represent one vertex (x, y, z)
		std::vector<float> solidVerts;
		/// Three indices represent one triangle (v1, v2, v3)
		std::vector<int> solidTris;
	};
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
	// this class gathers all debug info holding and output
	struct IntermediateValues
	{
		rcHeightfield* heightfield;
		rcCompactHeightfield* compactHeightfield;
		rcContourSet* contours;
		rcPolyMesh* polyMesh;
		rcPolyMeshDetail* polyMeshDetail;

		IntermediateValues()
			: compactHeightfield(nullptr)
			, heightfield(nullptr)
			, contours(nullptr)
			, polyMesh(nullptr)
			, polyMeshDetail(nullptr)
		{
		}
		~IntermediateValues()
		{
		}
		void generateObjFile(UInt32 mapID, UInt32 tileX, UInt32 tileY, MeshData& meshData)
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
	};

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
		int maxTiles = tiles.size();
		int maxPolysPerTile = 1 << DT_POLY_BITS;

		// Setup navigation mesh creation parameters
		dtNavMeshParams navMeshParams;
		memset(&navMeshParams, 0, sizeof(dtNavMeshParams));
		navMeshParams.tileWidth = GridSize;
		navMeshParams.tileHeight = GridSize;
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
			mesh.solidVerts.push_back(coord[0]);
			mesh.solidVerts.push_back(coord[2]);
			mesh.solidVerts.push_back(coord[1]);
		}
		for (int i = 0; i < V8_SIZE_SQ; ++i)
		{
			getHeightCoord(i, GRID_V8, xoffset, yoffset, coord, V8.data());
			mesh.solidVerts.push_back(coord[0]);
			mesh.solidVerts.push_back(coord[2]);
			mesh.solidVerts.push_back(coord[1]);
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
					mesh.solidTris.push_back(indices[2] + count);
					mesh.solidTris.push_back(indices[1] + count);
					mesh.solidTris.push_back(indices[0] + count);
				}
			}
		}

		return true;
	}
	/// Generates a navigation tile.
	static bool creaveNavTile(const String &mapName, UInt32 mapId, UInt32 tileX, UInt32 tileY, dtNavMesh &navMesh, const ADTFile &adt, const MapCollisionChunk &collision, MapNavigationChunk &out_chunk)
	{
#if 1
		if (tileY != 30 || tileX != 26)
			return false;
#endif

		// Reset chunk data
		out_chunk.data.clear();
		out_chunk.size = 0;
		out_chunk.tileRef = 0;

		// Build mesh data
		MeshData mesh;
		{
			for (auto &vert : collision.vertices)
			{
				mesh.solidVerts.push_back(vert.x);
				mesh.solidVerts.push_back(vert.z);
				mesh.solidVerts.push_back(vert.y);
			}
			for (auto &tri : collision.triangles)
			{
				mesh.solidTris.push_back(tri.indexC);
				mesh.solidTris.push_back(tri.indexB);
				mesh.solidTris.push_back(tri.indexA);
			}

			if (addTerrainMesh(adt, tileX, tileY, ENTIRE, mesh))
			{
				String adtFile;
				std::unique_ptr<ADTFile> adtInst;

#define LOAD_TERRAIN(x, y, spot) \
				adtFile = (boost::format("World\\Maps\\%1%\\%1%_%2%_%3%.adt") % mapName % (y) % (x)).str(); \
					adtInst = make_unique<ADTFile>(adtFile); \
				if (adtInst->load()) \
				{ \
					addTerrainMesh(*adtInst, (x), (y), spot, mesh); \
				}

				LOAD_TERRAIN(tileX + 1, tileY, LEFT);
				LOAD_TERRAIN(tileX - 1, tileY, RIGHT);
				LOAD_TERRAIN(tileX, tileY + 1, TOP);
				LOAD_TERRAIN(tileX, tileY - 1, BOTTOM);
#undef LOAD_TERRAIN
			}
		}

		// Process Doodads
		{
			DLOG("\tTile has " << adt.getMDDFChunk().entries.size() << " doodads");

			// Now load all required WMO files
			std::vector<std::unique_ptr<M2File>> m2s;
			for (UInt32 i = 0; i < adt.getMDXCount(); ++i)
			{
				// Retrieve MDX file name
				String filename = adt.getMDX(i);
				if (filename.length() > 4)
				{
					filename = filename.substr(0, filename.length() - 3);
					filename.append("m2");
				}
				auto m2File = make_unique<M2File>(filename);
				if (!m2File->load())
				{
					ELOG("Error loading MDX: " << filename);
					return false;
				}

				// Push back to the list of MDX files
				m2s.push_back(std::move(m2File));
			}

			for (const auto &entry : adt.getMDDFChunk().entries)
			{
				// Entry placement
				auto &m2 = m2s[entry.mmidEntry];
				
				// TODO: Apply scale
#define WOWPP_DEG_TO_RAD(x) (3.14159265358979323846 * (x) / -180.0)
				math::Matrix4 rotMat = math::Matrix4::fromEulerAnglesXYZ(
					WOWPP_DEG_TO_RAD(entry.rotation.x), -WOWPP_DEG_TO_RAD(180-entry.rotation.y), WOWPP_DEG_TO_RAD(entry.rotation.z));

				math::Vector3 position(entry.position.z, entry.position.y, entry.position.x);
				position.x -= 32 * 533.3333f;
				position.z -= 32 * 533.3333f;
				position.x *= -1.0f;
				position.z *= -1.0f;
#undef WOWPP_DEG_TO_RAD
				
				// Transform vertices
				const auto &verts = m2->getVertices();
				const auto &inds = m2->getIndices();

				UInt32 count = mesh.solidVerts.size() / 3;
				for (auto &vert : verts)
				{
					// Transform vertex and push it to the list
					math::Vector3 transformed = (rotMat * (vert * (float(entry.scale) / 1024.0f))) + position;
					/*transformed.x *= -1.f;
					transformed.z *= -1.f;*/

					mesh.solidVerts.push_back(transformed.x);
					mesh.solidVerts.push_back(transformed.y);
					mesh.solidVerts.push_back(transformed.z);
				}
				for (UInt32 i = 0; i < inds.size(); i += 3)
				{
					Triangle tri;
					tri.indexA = inds[i] + count;
					tri.indexB = inds[i + 1] + count;
					tri.indexC = inds[i + 2] + count;
					mesh.solidTris.push_back(tri.indexA);
					mesh.solidTris.push_back(tri.indexB);
					mesh.solidTris.push_back(tri.indexC);
				}
			}
		}

		const static float BaseUnitDim = 0.266666f;

		// All are in UNIT metrics!
		const static int VerticesPerMap = int(GridSize / BaseUnitDim + 0.5f);
		const static int VerticesPerTile = 80;
		const static int TilesPerMap = VerticesPerMap / VerticesPerTile;
		const static int TilesPerMapSq = TilesPerMap * TilesPerMap;

		// Calculate tile bounds
		float bmin[3], bmax[3];
		calculateTileBounds(tileX, tileY, bmin, bmax);

		rcConfig config;
		std::memset(&config, 0, sizeof(rcConfig));
		rcVcopy(config.bmin, bmin);
		rcVcopy(config.bmax, bmax);
		config.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
		config.cs = BaseUnitDim;
		config.ch = BaseUnitDim;
		config.walkableSlopeAngle = 60.0f;
		config.tileSize = VerticesPerTile;
		config.walkableRadius = 2;
		config.borderSize = config.walkableRadius + 3;
		config.maxEdgeLen = VerticesPerTile + 1;        //anything bigger than tileSize
		config.walkableHeight = 6;
		config.walkableClimb = 4;   // keep less than walkableHeight
		config.minRegionArea = rcSqr(60);
		config.mergeRegionArea = rcSqr(50);
		config.maxSimplificationError = 2.0f;       // eliminates most jagged edges (tinny polygons)
		config.detailSampleDist = config.cs * 64;
		config.detailSampleMaxError = config.ch * 2;
		rcCalcGridSize(config.bmin, config.bmax, config.cs, &config.width, &config.height);

		// Initialize per tile config.
		rcConfig tileCfg;
		memcpy(&tileCfg, &config, sizeof(rcConfig));
		tileCfg.width = config.tileSize + config.borderSize * 2;
		tileCfg.height = config.tileSize + config.borderSize * 2;

		rcContext context(false);

		// Build all tiles
		std::vector<NavTile> navTiles(TilesPerMapSq);
		for (int y = 0; y < TilesPerMap; ++y)
		{
			for (int x = 0; x < TilesPerMap; ++x)
			{
				auto &tile = navTiles[x + y * TilesPerMap];

				// Calculate the per tile bounding box.
				tileCfg.bmin[0] = config.bmin[0] + (x * config.tileSize - config.borderSize) * config.cs;
				tileCfg.bmin[2] = config.bmin[2] + (y * config.tileSize - config.borderSize) * config.cs;
				tileCfg.bmax[0] = config.bmin[0] + ((x + 1) * config.tileSize + config.borderSize) * config.cs;
				tileCfg.bmax[2] = config.bmin[2] + ((y + 1) * config.tileSize + config.borderSize) * config.cs;

				float tbmin[2], tbmax[2];
				tbmin[0] = tileCfg.bmin[0];
				tbmin[1] = tileCfg.bmin[2];
				tbmax[0] = tileCfg.bmax[0];
				tbmax[1] = tileCfg.bmax[2];

				// build heightfield
				tile.solid = rcAllocHeightfield();
				if (!tile.solid || !rcCreateHeightfield(&context, *tile.solid, tileCfg.width, tileCfg.height, tileCfg.bmin, tileCfg.bmax, tileCfg.cs, tileCfg.ch))
				{
					ELOG("Failed building heightfield!");
					continue;
				}

				// Mark all walkable tiles, both liquids and solids
				unsigned char* triFlags = new unsigned char[mesh.solidTris.size() / 3];
				memset(triFlags, NAV_GROUND, mesh.solidTris.size() / 3 * sizeof(unsigned char));
				rcClearUnwalkableTriangles(&context, tileCfg.walkableSlopeAngle, &mesh.solidVerts[0], mesh.solidVerts.size() / 3, &mesh.solidTris[0], mesh.solidTris.size() / 3, triFlags);
				rcRasterizeTriangles(&context, &mesh.solidVerts[0], mesh.solidVerts.size() / 3, &mesh.solidTris[0], triFlags, mesh.solidTris.size() / 3, *tile.solid, config.walkableClimb);
				delete[] triFlags;

				rcFilterLowHangingWalkableObstacles(&context, config.walkableClimb, *tile.solid);
				rcFilterLedgeSpans(&context, tileCfg.walkableHeight, tileCfg.walkableClimb, *tile.solid);
				rcFilterWalkableLowHeightSpans(&context, tileCfg.walkableHeight, *tile.solid);

				// compact heightfield spans
				tile.chf = rcAllocCompactHeightfield();
				if (!tile.chf || !rcBuildCompactHeightfield(&context, tileCfg.walkableHeight, tileCfg.walkableClimb, *tile.solid, *tile.chf))
				{
					ELOG("Failed compacting heightfield!");
					continue;
				}

				// build polymesh intermediates
				if (!rcErodeWalkableArea(&context, config.walkableRadius, *tile.chf))
				{
					ELOG("Failed eroding area!");
					continue;
				}
				if (!rcBuildDistanceField(&context, *tile.chf))
				{
					ELOG("Failed building distance field!");
					continue;
				}
				if (!rcBuildRegions(&context, *tile.chf, tileCfg.borderSize, tileCfg.minRegionArea, tileCfg.mergeRegionArea))
				{
					ELOG("Failed building regions!");
					continue;
				}
				tile.cset = rcAllocContourSet();
				if (!tile.cset || !rcBuildContours(&context, *tile.chf, tileCfg.maxSimplificationError, tileCfg.maxEdgeLen, *tile.cset))
				{
					ELOG("Failed building contours!");
					continue;
				}

				// build polymesh
				tile.pmesh = rcAllocPolyMesh();
				if (!tile.pmesh || !rcBuildPolyMesh(&context, *tile.cset, tileCfg.maxVertsPerPoly, *tile.pmesh))
				{
					ELOG("Failed building polymesh!");
					continue;
				}
				tile.dmesh = rcAllocPolyMeshDetail();
				if (!tile.dmesh || !rcBuildPolyMeshDetail(&context, *tile.pmesh, *tile.chf, tileCfg.detailSampleDist, tileCfg.detailSampleMaxError, *tile.dmesh))
				{
					ELOG("Failed building polymesh detail!");
					continue;
				}

				// Memory free
				rcFreeHeightField(tile.solid);
				tile.solid = nullptr;
				rcFreeCompactHeightfield(tile.chf);
				tile.chf = nullptr;
				rcFreeContourSet(tile.cset);
				tile.cset = nullptr;
			}
		}

		IntermediateValues iv;

		// merge per tile poly and detail meshes
		rcPolyMesh** pmmerge = new rcPolyMesh*[TilesPerMapSq];
		if (!pmmerge)
		{
			ELOG("Allocating pmmerge failed");
			return false;
		}

		rcPolyMeshDetail** dmmerge = new rcPolyMeshDetail*[TilesPerMapSq];
		if (!dmmerge)
		{
			ELOG("Allocating dmmerge failed");
			return false;
		}

		int nmerge = 0;
		for (int y = 0; y < TilesPerMap; ++y)
		{
			for (int x = 0; x < TilesPerMap; ++x)
			{
				NavTile& tile = navTiles[x + y * TilesPerMap];
				if (tile.pmesh)
				{
					pmmerge[nmerge] = tile.pmesh;
					dmmerge[nmerge] = tile.dmesh;
					nmerge++;
				}
			}
		}

		iv.polyMesh = rcAllocPolyMesh();
		if (!iv.polyMesh)
		{
			ELOG("alloc iv.polyMesh failed");
			return false;
		}
		rcMergePolyMeshes(&context, pmmerge, nmerge, *iv.polyMesh);

		iv.polyMeshDetail = rcAllocPolyMeshDetail();
		if (!iv.polyMeshDetail)
		{
			ELOG("alloc m_dmesh failed");
			return false;
		}
		rcMergePolyMeshDetails(&context, dmmerge, nmerge, *iv.polyMeshDetail);

		// free things up
		delete[] pmmerge;
		delete[] dmmerge;

		for (int i = 0; i < iv.polyMesh->npolys; ++i)
			if (iv.polyMesh->areas[i] & RC_WALKABLE_AREA)
				iv.polyMesh->flags[i] = iv.polyMesh->areas[i];

		// setup mesh parameters
		dtNavMeshCreateParams params;
		memset(&params, 0, sizeof(params));
		params.verts = iv.polyMesh->verts;
		params.vertCount = iv.polyMesh->nverts;
		params.polys = iv.polyMesh->polys;
		params.polyAreas = iv.polyMesh->areas;
		params.polyFlags = iv.polyMesh->flags;
		params.polyCount = iv.polyMesh->npolys;
		params.nvp = iv.polyMesh->nvp;
		params.detailMeshes = iv.polyMeshDetail->meshes;
		params.detailVerts = iv.polyMeshDetail->verts;
		params.detailVertsCount = iv.polyMeshDetail->nverts;
		params.detailTris = iv.polyMeshDetail->tris;
		params.detailTriCount = iv.polyMeshDetail->ntris;
		params.walkableHeight = BaseUnitDim * config.walkableHeight;  // agent height
		params.walkableRadius = BaseUnitDim * config.walkableRadius;  // agent radius
		params.walkableClimb = BaseUnitDim * config.walkableClimb;    // keep less that walkableHeight (aka agent height)!
		params.tileX = (((bmin[0] + bmax[0]) / 2) - navMesh.getParams()->orig[0]) / GRID_SIZE;
		params.tileY = (((bmin[2] + bmax[2]) / 2) - navMesh.getParams()->orig[2]) / GRID_SIZE;
		rcVcopy(params.bmin, bmin);
		rcVcopy(params.bmax, bmax);
		params.cs = config.cs;
		params.ch = config.ch;
		params.tileLayer = 0;
		params.buildBvTree = true;

		// will hold final navmesh
		unsigned char* navData = NULL;
		int navDataSize = 0;
		do
		{
			// these values are checked within dtCreateNavMeshData - handle them here
			// so we have a clear error message
			if (params.nvp > DT_VERTS_PER_POLYGON)
			{
				ELOG("Invalid verts-per-polygon value!");
				continue;
			}
			if (params.vertCount >= 0xffff)
			{
				ELOG("Too many vertices!");
				continue;
			}
			if (!params.vertCount || !params.verts)
			{
				continue;
			}
			if (!params.polyCount || !params.polys ||
				TilesPerMapSq == params.polyCount)
			{
				// we have flat tiles with no actual geometry - don't build those, its useless
				// keep in mind that we do output those into debug info
				// drop tiles with only exact count - some tiles may have geometry while having less tiles
				ELOG("No polygons to build on tile!");
				continue;
			}
			if (!params.detailMeshes || !params.detailVerts || !params.detailTris)
			{
				ELOG("No detail mesh to build tile!");
				continue;
			}

			if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
			{
				ELOG("Failed building navmesh tile!");
				continue;
			}

			dtTileRef tileRef = 0;
			// DT_TILE_FREE_DATA tells detour to unallocate memory when the tile
			// is removed via removeTile()
			dtStatus dtResult = navMesh.addTile(navData, navDataSize, DT_TILE_FREE_DATA, 0, &tileRef);
			if (!tileRef || dtStatusFailed(dtResult))
			{
				ELOG("Failed adding tile to navmesh!");
				continue;
			}

			// We have nav data!
			out_chunk.data.resize(navDataSize);
			std::memcpy(&out_chunk.data[0], navData, navDataSize);
			out_chunk.tileRef = tileRef;

			const int nvp = iv.polyMesh->nvp;
			const float cs = iv.polyMesh->cs;
			const float ch = iv.polyMesh->ch;
			const float* orig = iv.polyMesh->bmin;
			int nIndex = 0;
			
#if 1
			if (iv.polyMesh->npolys > 0)
			{
				std::ostringstream strm;
				strm << "meshes/tile_" << tileX << "_" << tileY << ".obj";
				std::ofstream outFile(strm.str().c_str(), std::ios::out);
				for (int i = 0; i < iv.polyMesh->npolys; ++i) // go through all polygons
				{
					const unsigned short* p = &iv.polyMesh->polys[i*nvp * 2];

					unsigned short vi[3];
					for (int j = 2; j < nvp; ++j) // go through all verts in the polygon
					{
						if (p[j] == RC_MESH_NULL_IDX) break;
						vi[0] = p[0];
						vi[1] = p[j - 1];
						vi[2] = p[j];
						for (int k = 0; k < 3; ++k) // create a 3-vert triangle for each 3 verts in the polygon.
						{
							const unsigned short* v = &iv.polyMesh->verts[vi[k] * 3];
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

			// Remove (and unload) tile, since we've copied it's data already
			navMesh.removeTile(tileRef, &navData, &navDataSize);
		}
		while (0);

#if 1
		// restore padding so that the debug visualization is correct
		for (int i = 0; i < iv.polyMesh->nverts; ++i)
		{
			unsigned short* v = &iv.polyMesh->verts[i * 3];
			v[0] += (unsigned short)config.borderSize;
			v[2] += (unsigned short)config.borderSize;
		}

		iv.generateObjFile(mapId, tileX, tileY, mesh);
#endif

		return true;
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

		// Build file names
		const String adtFile =
			(boost::format("World\\Maps\\%1%\\%1%_%2%_%3%.adt") % mapName % cellY % cellX).str();
		const String mapFile =
			(outputPath / (boost::format("%1%") % mapId).str() / (boost::format("%1%_%2%.map") % cellX % cellY).str()).string();

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

			math::Matrix4 mat;
#define WOWPP_DEG_TO_RAD(x) (3.14159265358979323846 * (x) / -180.0)
			math::Matrix4 rotMat = math::Matrix4::fromEulerAnglesXYZ(
				WOWPP_DEG_TO_RAD(entry.rotation[2]), WOWPP_DEG_TO_RAD(entry.rotation[0]), WOWPP_DEG_TO_RAD(-entry.rotation[1]));

			math::Vector3 position(entry.position.z, entry.position.x, entry.position.y);
			position.x -= 32 * 533.3333f;
			position.y -= 32 * 533.3333f;
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
					transformed.x *= -1.f;
					transformed.y *= -1.f;
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
		if (mapId != 0)
		{
			return true;
		}
#endif

		// Build map
		ILOG("Building map " << mapId << " - " << mapName << "...");

		// Create the map directory (if it doesn't exist)
		const fs::path mapPath = outputPath / (boost::format("%1%") % mapId).str();
		if (!fs::is_directory(mapPath))
		{
			if (!fs::create_directory(mapPath))
			{
				// Skip this map
				return false;
			}
		}

		// Load the WDT file and get infos out of there
		const String wdtFileName = (boost::format("World\\Maps\\%1%\\%1%.wdt") % mapName).str();
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
			(outputPath / (boost::format("%1%.map") % mapId).str()).string();
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
			if (fs::exists(inputPath / "Data" / locale / (boost::format("locale-%1%.MPQ") % locale).str()))
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
			"locale-%1%.MPQ",
			"patch-%1%.MPQ",
			"patch-%1%-2.MPQ",
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
				(inputPath / "Data" / localeString / (boost::format(file) % localeString).str()).string();
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
	std::size_t concurrency = boost::thread::hardware_concurrency();
	concurrency = 1;	//std::max<size_t>(1, concurrency);
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
