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

	/// This is the length of an edge of a map file in ingame units where one unit 
	/// represents one meter.
	const float GridSize = 533.3333f;
	const float GridPart = GridSize / 128;

	/// Represents mesh data used for navigation mesh generation
	struct MeshData final
	{
		// Three coordinates represent one vertex (x, y, z)
		std::vector<float> solidVerts;
		/// Three indices represent one triangle (v1, v2, v3)
		std::vector<int> solidTris;
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
		void writeIV(UInt32 mapID, UInt32 tileX, UInt32 tileY)
		{
		}
		void debugWrite(FILE* file, const rcHeightfield* mesh)
		{
		}
		void debugWrite(FILE* file, const rcCompactHeightfield* chf)
		{
		}
		void debugWrite(FILE* file, const rcContourSet* cs)
		{
		}
		void debugWrite(FILE* file, const rcPolyMesh* mesh)
		{
		}
		void debugWrite(FILE* file, const rcPolyMeshDetail* mesh)
		{
		}
		void generateObjFile(UInt32 mapID, UInt32 tileX, UInt32 tileY, MeshData& meshData)
		{
		}
	};

	/// Generates navigation mesh for one map.
	static bool createNavMesh(UInt32 mapId, dtNavMesh &navMesh)
	{
		float bmin[3], bmax[3];
		bmin[1] = FLT_MIN;
		bmax[1] = FLT_MAX;

		// this is for width and depth
		bmax[0] = (32 - int(64)) * GridSize;
		bmax[2] = (32 - int(64)) * GridSize;
		bmin[0] = bmax[0] - GridSize;
		bmin[2] = bmax[2] - GridSize;

		int tileBits = 28;
		int polyBits = 20;
		int maxTiles = 64*64;
		int maxPolysPerTile = 1 << polyBits;

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
			wmos.emplace_back(std::move(wmoFile));
		}

		// Create files (TODO)
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
#define WOWPP_DEG_TO_RAD(x) (3.14159265358979323846 * x / -180.0)
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

#if 0
		// Map height header
		MapHeightChunk heightChunk;
		heightChunk.fourCC = 0x54484D57;		// WMHT		- WoW Map Height
		heightChunk.size = sizeof(MapHeightChunk) - 8;
		for (size_t i = 0; i < 16 * 16; ++i)
		{
			const auto &mcnk = adt.getMCNKChunk(i);
			const auto &mcvt = adt.getMCVTChunk(i);

			size_t index = 0;
			for (auto &height : mcvt.heights)
			{
				heightChunk.heights[i][index++] = mcnk.zpos + height;
			}
		}
#endif

#if 0
		// Build nav mesh
		MapNavigationChunk navigationChunk;
		navigationChunk.fourCC = 0x564E4D57;		// WMNV		- WoW Map Navigation
		navigationChunk.size = 0;
		header.offsNavigation = header.offsAreaTable + header.areaTableSize + header.collisionSize;
		header.navigationSize = sizeof(UInt32) * 4 + navigationChunk.size;
		writer
			<< io::write<UInt32>(navigationChunk.fourCC)
			<< io::write<UInt32>(navigationChunk.size)
			<< io::write<UInt32>(navigationChunk.tileRef)
			<< io::write_dynamic_range<NetUInt32>(navigationChunk.data);
#endif

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
		/*
		if (mapId != 36)
		{
			ILOG("Skipping map " << mapId);
			return true;
		}
		*/

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
		auto &adtTiles = mapWDT.getMAINChunk().adt;
		
		// TODO: Filter tiles that aren't needed here

		// Create nav mesh
		auto freeNavMesh = [](dtNavMesh* ptr) { dtFreeNavMesh(ptr); };
		std::unique_ptr<dtNavMesh, decltype(freeNavMesh)> navMesh(dtAllocNavMesh(), freeNavMesh);
		if (!createNavMesh(mapId, *navMesh))
		{
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
	concurrency = std::max<size_t>(1, concurrency);
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
