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

#include "common/typedefs.h"
#include "log/default_log_levels.h"
#include "log/log_std_stream.h"
#include "boost/filesystem.hpp"
#include "boost/program_options.hpp"
#include "boost/format.hpp"
#include "mpq_file.h"
#include "dbc_file.h"
#include "wdt_file.h"
#include "adt_file.h"
#include "binary_io/writer.h"
#include "binary_io/stream_sink.h"
#include "common/make_unique.h"
#include <fstream>
#include <limits>
#include <memory>
#include <map>
#include "game/map.h"
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

//////////////////////////////////////////////////////////////////////////
// Helper functions
namespace
{
	/// Converts an ADT tile of a WDT file.
	static bool convertADT(UInt32 mapId, const String &mapName, WDTFile &wdt, UInt32 tileIndex)
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

		// Create files (TODO)
		std::ofstream fileStrm(mapFile, std::ios::out | std::ios::binary);
		io::StreamSink sink(fileStrm);
		io::Writer writer(sink);
		
        // Create map header
        MapHeaderChunk header;
        header.fourCC = 0x50414D57;				// WMAP		- WoW Map
        header.size = sizeof(MapHeaderChunk) - 8;
        header.version = 0x100;
        header.offsAreaTable = sizeof(MapHeaderChunk);
        header.areaTableSize = sizeof(MapAreaChunk);
        header.offsHeight = header.offsAreaTable + header.areaTableSize;
		header.heightSize = sizeof(MapHeightChunk);
        
        // Area header
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
		
		// Map size header
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
        
		// Write map header
        writer.writePOD(header);
        writer.writePOD(areaHeader);
		writer.writePOD(heightChunk);
        
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

		// Iterate through all tiles and create those which are available
		bool succeeded = true;
		for (UInt32 tile = 0; tile < adtTiles.size(); ++tile)
		{
			if (!convertADT(mapId, mapName, mapWDT, tile))
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
		const String commonArchives[] = {
			"common.MPQ",
			"expansion.MPQ",
			"patch.MPQ",
			"patch-2.MPQ"
		};

		const String localeArchives[] = {
			"locale-%1%.MPQ",
			"patch-%1%.MPQ",
			"patch-%1%-2.MPQ",
		};

		for (auto &file : commonArchives)
		{
			const String mpqFileName = (inputPath / "Data" / file).string();
			if (!mpq::loadMPQFile(mpqFileName))
			{
				WLOG("Could not load MPQ archive " << mpqFileName);
			}
		}

		for (auto &file : localeArchives)
		{
			String mpqFileName =
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
		dbcMap = make_unique<DBCFile>("DBFilesClient\\Map.dbc");
		if (!dbcMap->load()) return false;

		dbcAreaTable = make_unique<DBCFile>("DBFilesClient\\AreaTable.dbc");
		if (!dbcAreaTable->load()) return false;

		dbcLiquidType = make_unique<DBCFile>("DBFilesClient\\LiquidType.dbc");
		if (!dbcLiquidType->load()) return false;
        
        // Parse area flags
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
	// Add cout to the list of log output streams
	wowpp::g_DefaultLog.signal().connect(std::bind(
		wowpp::printLogEntry,
		std::ref(std::cout), std::placeholders::_1, wowpp::g_DefaultConsoleLogOptions));

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

	// Iterate through all maps and build all tiles
	for (UInt32 i = 0; i < dbcMap->getRecordCount(); ++i)
	{
		convertMap(i);
	}

	return 0;
}
