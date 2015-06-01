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

#include "clock.h"
#if defined(WIN32) || defined(_WIN32)
#	include <Windows.h>
#else
#	include <sys/time.h>
#	include <unistd.h>
#	ifdef __MACH__
#		include <mach/clock.h>
#		include <mach/mach.h>
#	endif
#endif
#include <cassert>

namespace wowpp
{
	GameTime getCurrentTime()
	{
#if defined(WIN32) || defined(_WIN32)
		//::GetTickCount64() is not available on Windows XP and prior systems
		return ::GetTickCount64();
#elif !defined(__APPLE__)
		struct timespec ts = {0, 0};
		const int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
		assert(rc == 0);

		return
		    static_cast<GameTime>(ts.tv_sec) * 1000 +
		    static_cast<GameTime>(ts.tv_nsec) / 1000 / 1000;
#else
		struct timespec ts = {0, 0};
		
		clock_serv_t cclock;
		mach_timespec_t mts;
		host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
		clock_get_time(cclock, &mts);
		mach_port_deallocate(mach_task_self(), cclock);
		ts.tv_sec = mts.tv_sec;
		ts.tv_nsec = mts.tv_nsec;
		
		return
			static_cast<GameTime>(ts.tv_sec) * 1000 +
			static_cast<GameTime>(ts.tv_nsec) / 1000 / 1000;
#endif
	}
}
