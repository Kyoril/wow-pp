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

#pragma once

#define WOWPP_HELPER_STRINGIFY(x) #x
#define WOWPP_STRINGIFY(x) WOWPP_HELPER_STRINGIFY(x)

#ifdef _MSC_VER
#	define WOWPP_DEBUG_BREAK() __debugbreak()
#else
//assuming GCC
#	define WOWPP_DEBUG_BREAK() __builtin_trap()
#endif

#if defined(DEBUG) || !defined(NDEBUG) || defined(_DEBUG)
#	ifndef WOWPP_DEBUG
#		define WOWPP_DEBUG
#	endif
#endif

#ifdef _MSC_VER
#	define WOWPP_ASSUME(booleanExpr) __assume((booleanExpr))
#else
#	define WOWPP_ASSUME(booleanExpr) do { if (!(booleanExpr)) { __builtin_unreachable(); } } while(false)
#endif

namespace wowpp
{
	//from crash_handler.h for ASSERT
	void printCallStack(std::ostream &out);
}

#if defined(WOWPP_DEBUG) || defined(WOWPP_ALWAYS_ASSERT)
#	include <iostream>
#	include "log/default_log_levels.h"
#	define WOWPP_HELPER_MAKE_ASSERT_FAIL_MESSAGE(expression) "ASSERT(" #expression ") failed in " __FILE__ ":" WOWPP_STRINGIFY(__LINE__)
#	define ASSERT(booleanExpr) \
	{ \
		bool const isWrong = !(booleanExpr); \
		if (isWrong) { \
			ELOG(WOWPP_HELPER_MAKE_ASSERT_FAIL_MESSAGE(booleanExpr)); \
			::wowpp::printCallStack(::std::cerr); \
			WOWPP_DEBUG_BREAK(); \
		} \
	}
#else
#	define ASSERT(booleanExpr) WOWPP_ASSUME(booleanExpr)
#endif

// Use as follows: WOWPP_DEPRECATED void MyFunc(...);
// Will generate a compiler warning when this method is being used.
#ifndef WOWPP_DEPRECATED
#	ifdef __GNUC__
#		define WOWPP_DEPRECATED __attribute__((deprecated))
#	elif defined(_MSC_VER)
#		define WOWPP_DEPRECATED __declspec(deprecated)
#	else
#		pragma message("WARNING: You need to implement WOWPP_DEPRECATED for this compiler")
#		define WOWPP_DEPRECATED
#	endif
#endif


