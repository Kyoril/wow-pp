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
#include "common/create_process.h"
#include "log/log_std_stream.h"
#include "log/default_log_levels.h"
#ifdef _WIN32
#	include <tlhelp32.h>
#endif
using namespace wowpp;
namespace fs = boost::filesystem;

static void waitForProcess(int processId)
{
#ifndef _WIN32
	WLOG("Waiting for process id not supported on this platform!");
	return;
#else
	// Create toolhelp snapshot.
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 process;
	ZeroMemory(&process, sizeof(process));
	process.dwSize = sizeof(process);

	// Walkthrough all processes
	bool foundProcess = false;
	if (Process32First(snapshot, &process))
	{
		do
		{
			if (process.th32ProcessID == processId)
			{
				foundProcess = true;
				break;
			}
		} while (Process32Next(snapshot, &process));
	}
	CloseHandle(snapshot);

	if (foundProcess)
	{
		HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
		ILOG("Waiting for editor.exe to quit...");
		WaitForSingleObject(processHandle, INFINITE);
		CloseHandle(processHandle);
	}
	else
	{
		ILOG("Editor process no longer running - applying patch");
	}
#endif
}

bool copyDir(
	fs::path const & source,
	fs::path const & destination
)
{
	try
	{
		// Check whether the function call is valid
		if (
			!fs::exists(source) ||
			!fs::is_directory(source)
			)
		{
			ELOG("Source directory " << source.string()
				<< " does not exist or is not a directory.");
			return false;
		}
		if (!fs::exists(destination))
		{
			// Create the destination directory
			if (!fs::create_directory(destination))
			{
				ELOG("Unable to create destination directory"
					<< destination.string());
				return false;
			}
		}
	}
	catch (fs::filesystem_error const & e)
	{
		std::cerr << e.what() << '\n';
		return false;
	}
	// Iterate through the source directory
	for (
		fs::directory_iterator file(source);
		file != fs::directory_iterator(); ++file
		)
	{
		try
		{
			fs::path current(file->path());
			if (fs::is_directory(current))
			{
				// Found directory: Recursion
				if (
					!copyDir(
						current,
						destination / current.filename()
					)
					)
				{
					return false;
				}
			}
			else
			{
				// Found file: Copy
				fs::copy_file(
					current,
					destination / current.filename(),
					fs::copy_option::overwrite_if_exists
				);
			}
		}
		catch (fs::filesystem_error const & e)
		{
			ELOG(e.what());
		}
	}
	return true;
}

static bool applyPatch(const fs::path &source)
{
	if (!copyDir(source, "."))
		return false;

	// Remove directory
	try
	{
		fs::remove_all(source);
	}
	catch (const std::exception &e)
	{
		ELOG(e.what());
		return false;
	}

	return true;
}

/// Procedural entry point of the application.
int main(int argc, char* argv[])
{
	int processId = 0;
	std::string tempDir;

	// Add cout to the list of log output streams
	wowpp::g_DefaultLog.signal().connect(std::bind(
		wowpp::printLogEntry,
		std::ref(std::cout), std::placeholders::_1, wowpp::g_DefaultConsoleLogOptions));
	
	// The log files are written to in a special background thread
	std::ofstream logFile;
	LogStreamOptions logFileOptions = g_DefaultFileLogOptions;
	logFileOptions.alwaysFlush = true;

	namespace po = boost::program_options;

	po::options_description desc("WoW++ Update Helper options");
	desc.add_options()
		("help,h", "produce help message")
		("pid", po::value<int>(&processId) , "process id of the editor application")
		("dir", po::value<std::string>(&tempDir), "directory which contains the patch files")
		;

	po::positional_options_description p;
	p.add("pid", 1);
	p.add("dir", 1);

	po::variables_map vm;
	try
	{
		po::store(
			po::command_line_parser(argc, argv).options(desc).run(),
			vm
		);
		po::notify(vm);
	}
	catch (const po::error &e)
	{
		ELOG(e.what());
		return 1;
	}

	// Wait for editor process to terminate
	waitForProcess(processId);

	// Apply patch files
	if (!applyPatch(tempDir))
	{
		return 1;
	}

	try
	{
		std::string processName = "editor";
#ifdef _WIN32
		processName += ".exe";
#elif defined(__APPLE__)
		processName += ".app/Contents/MacOS/editor";
#endif

		// Start the editor
		std::vector<std::string> args;
		createProcess(processName, args);
	}
	catch (const std::exception &e)
	{
		ELOG(e.what());
		return 1;
	}

	return 0;
}
