//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
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
#include "stormlib/src/StormLib.h"
#include "boost/program_options.hpp"
#include "boost/format.hpp"
#include <limits>
#include <memory>
#include <deque>

using namespace std;

namespace mpq
{
	static std::deque<HANDLE> openedArchives;

	static bool loadMPQFile(const std::string &file)
	{
		HANDLE mpqHandle = nullptr;
		if (!SFileOpenArchive(file.c_str(), 0, 0, &mpqHandle))
		{
			// Could not open MPQ archive file
			return false;
		}

		// Push to open archives
		mpq::openedArchives.push_front(mpqHandle);
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

	// Create the required directory
	namespace fs = boost::filesystem;

	// Some variables
	const fs::path inputPath(".");
	const fs::path outputPath("maps");

	// Try to create output path
	if (!fs::is_directory(outputPath))
	{
		if (!fs::create_directory(outputPath))
		{
			ELOG("Could not create output path directory: " << outputPath);
			return 1;
		}
	}

	// Archives to load
	const std::string commonArchives[] = {
		"common.MPQ",
		"expansion.MPQ",
		"patch.MPQ",
		"patch-2.MPQ"
	};

	const std::string localeArchives[] = {
		"locale-%1%.MPQ",
		"patch-%1%.MPQ",
		"patch-%1%-2.MPQ",
	};

	// Try to load all MPQ files
	for (auto &file : commonArchives)
	{
		const std::string mpqFileName = (inputPath / "Data" / file).string();
		if (!mpq::loadMPQFile(mpqFileName))
		{
			WLOG("Could not load MPQ archive " << mpqFileName);
		}
	}

	// Locale files
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

	// Detect client locale
	std::string localeString;
	for (auto &locale : locales)
	{
		if (fs::exists(inputPath / "Data" / locale / (boost::format("locale-%1%.MPQ") % locale).str()))
		{
			localeString = locale;
			break;
		}
	}

	if (localeString.empty())
	{
		ELOG("Couldn't find any locale!");
		return 1;
	}
	else
	{
		ILOG("Detected locale: " << localeString);
	}

	// Open locale MPQ files
	for (auto &file : localeArchives)
	{
		std::string mpqFileName =
			(inputPath / "Data" / localeString / (boost::format(file) % localeString).str()).string();

		if (!mpq::loadMPQFile(mpqFileName))
		{
			WLOG("Could not load MPQ archive " << mpqFileName);
		}
	}

	// Load map dbc file


	// Close all opened MPQ archives
	for (auto &handle : mpq::openedArchives)
	{
		SFileCloseArchive(handle);
	}

	// Shutdown
	return 0;
}
