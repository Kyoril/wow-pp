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
#include "program.h"
#include "log/default_log_levels.h"
#include "log/log_std_stream.h"
#include "common/crash_handler.h"
#include "common/service.h"

using namespace std;

/// Procedural entry point of the application.
int main(int argc, char* argv[])
{
	namespace po = boost::program_options;

	po::options_description desc("WoW++ Realm available options");
	desc.add_options()
		("help,h", "produce help message")
#ifdef __linux__
		("service,s", "run the realm as a background process")
#endif
		;

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
		std::cerr << e.what() << '\n';
		return 1;
	}

	if (vm.count("help"))
	{
		std::cerr << desc << '\n';
		return 0;
	}

	// Running as a service only works on linux
	const bool startAsService =
#ifdef __linux__
		(vm.count("service") > 0);
#else
		false;
#endif

	auto openStdLog = []() {
		// Add cout to the list of log output streams
		wowpp::g_DefaultLog.signal().connect(std::bind(
			wowpp::printLogEntry,
			std::ref(std::cout), std::placeholders::_1, wowpp::g_DefaultConsoleLogOptions));
	};

	if (startAsService)
	{
		switch (wowpp::createService())
		{
			case wowpp::create_service_result::IsObsoleteProcess:
				std::cout << "Realm service is now running." << '\n';
				return 0;

			case wowpp::create_service_result::IsServiceProcess:
			{
				openStdLog();
				ILOG("Successfully running as a service");
			} break;
		}
	}
	else
	{
		openStdLog();
	}

	//constructor enables error handling
	wowpp::CrashHandler::get().enableDumpFile("RealmCrash.dmp");

	//when the application terminates unexpectedly
	const auto crashFlushConnection =
		wowpp::CrashHandler::get().onCrash.connect(
		[]()
	{
		ELOG("Application crashed...");
	});

	// Triggers if the program should be restarted
	bool shouldRestartProgram = false;

	do
	{
		// Run the main program
		wowpp::Program program(startAsService);
		shouldRestartProgram = program.run();
	} while (shouldRestartProgram);

	// Shutdown
	return 0;
}
