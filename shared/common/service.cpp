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

#include "service.h"
#include <exception>
#ifdef __linux__
#	include <unistd.h>
#	include <sys/stat.h>
#endif

namespace wowpp
{
	create_service_result::Enum createService(/*
	    const std::function<void ()> &openLogFiles*/)
	{
#ifdef __linux__
		{
			const pid_t pid = fork();
			if (pid < 0)
			{
				throw std::runtime_error("fork failed");
			}
			else if (pid != 0)
			{
				//this is the parent process
				return create_service_result::IsObsoleteProcess;
			}
		}
		umask(0);
#endif

		//openLogFiles();

#ifdef __linux__
		{
			const pid_t sid = setsid();
			if (sid < 0)
			{
				throw std::runtime_error("setsid failed");
			}
		}

		/*if (chdir("/") < 0)
		{
			throw std::runtime_error("chdir failed");
		}*/

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
#endif

		return create_service_result::IsServiceProcess;
	}
}
