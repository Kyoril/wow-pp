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

#include <iostream>
#include <set>
#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>
#include "update_compilation/compile_directory.h"
#include "virtual_directory/file_system_reader.h"
#include "virtual_directory/file_system_writer.h"

int main(int argc, char **argv)
{
	using namespace ::wowpp;
	namespace po = ::boost::program_options;

	static const std::string VersionStr = "WoW++ Update Compiler 1.0";

	std::string sourceDir, outputDir;
	std::string compression;

	po::options_description desc(VersionStr + ", available options");
	desc.add_options()
	("help", "produce help message")
	("version", "display the application's name and version")
	("source,s", po::value<std::string>(&sourceDir), "a directory containing source.txt")
	("output,o", po::value<std::string>(&outputDir), "where to put the updater-compatible files")
	("compression,c", po::value<std::string>(&compression), "provide 'zlib' for compression")
	;

	po::positional_options_description p;
	p.add("source", 1);
	p.add("output", 1);

	po::variables_map vm;
	try
	{
		po::store(
		    po::command_line_parser(argc, argv).options(desc).positional(p).run(),
		    vm);
		po::notify(vm);
	}
	catch (const po::error &e)
	{
		std::cerr << e.what() << "\n";
		return 1;
	}

	if (vm.count("version"))
	{
		std::cerr << VersionStr << '\n';
	}

	if ((argc == 1) ||
	    vm.count("help"))
	{
		std::cerr << desc << "\n";
		return 0;
	}

	bool isZLibCompressed = false;
	if (!compression.empty())
	{
		if (compression == "zlib")
		{
			isZLibCompressed = true;
		}
		else
		{
			std::cerr
			        << "Unknown compression type\n"
			        << "Use 'zlib' for compression\n";
			return 1;
		}
	}

	try
	{
		virtual_dir::FileSystemReader sourceReader(sourceDir);
		virtual_dir::FileSystemWriter outputWriter(outputDir);

		wowpp::updating::compileDirectory(
		    sourceReader,
		    outputWriter,
		    isZLibCompressed
		);
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}

	return 0;
}
