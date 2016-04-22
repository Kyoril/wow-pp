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
#include "clock.h"
#if defined(WIN32) || defined(_WIN32)
#	include <Windows.h>
#	include <mmsystem.h>
#	pragma comment(lib, "winmm.lib")
#else
#	include <sys/time.h>
#	include <unistd.h>
#	ifdef __MACH__
#		include <mach/clock.h>
#		include <mach/mach.h>
#	endif
#endif

namespace wowpp
{
	UInt32 TimeStamp()
	{
#ifdef _WIN32
		FILETIME ft;
		UInt64 t;
		GetSystemTimeAsFileTime(&ft);

		const UInt64 deltaEpichInUSec = 11644473600000000ULL;
		t = (UInt64)ft.dwHighDateTime << 32;
		t |= ft.dwLowDateTime;
		t /= 10;
		t -= deltaEpichInUSec;

		return UInt32(((t / 1000000L) * 1000) + ((t % 1000000L) / 1000));
#else
		struct timeval tp;
		gettimeofday(&tp, nullptr);
		return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
#endif
	}
	UInt32 mTimeStamp()
	{
#ifdef _WIN32
		return timeGetTime();
#else
		struct timeval tp;
		gettimeofday(&tp, nullptr);
		return UInt32(
            (tp.tv_sec * 1000) + (tp.tv_usec / 1000) % UInt64(0x00000000FFFFFFFF));
#endif
	}

	GameTime getCurrentTime()
	{
#if defined(WIN32) || defined(_WIN32)
		//::GetTickCount64() is not available on Windows XP and prior systems
		return ::GetTickCount64();
#else
        struct timeval tp;
        gettimeofday(&tp, nullptr);
        return UInt32(
            (tp.tv_sec * 1000) + (tp.tv_usec / 1000) % UInt64(0x00000000FFFFFFFF));
#endif
    }
}
