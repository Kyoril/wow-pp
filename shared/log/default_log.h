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

#pragma once

#include "log.h"
#include "log_entry.h"
#include <boost/date_time/posix_time/posix_time.hpp>

namespace wowpp
{
	extern Log g_DefaultLog;

#if defined(WOWPP_LOG) || defined(WOWPP_LOG_FORMATTER_NAME)
#error Something went wrong with the log macros
#endif

#define WOWPP_LOG_FORMATTER_NAME _wowpp_log_formatter_
#define WOWPP_LOG(level, message) \
	{ \
	::std::basic_ostringstream<char> WOWPP_LOG_FORMATTER_NAME; \
	WOWPP_LOG_FORMATTER_NAME << message; \
			::wowpp::g_DefaultLog.signal()( \
			::wowpp::LogEntry(level, \
							   WOWPP_LOG_FORMATTER_NAME.str(), \
							   ::boost::posix_time::second_clock::local_time() \
							   ) \
			); \
	}
}
