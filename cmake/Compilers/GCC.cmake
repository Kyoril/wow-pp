#
# This file is part of the WoW++ project.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software 
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# World of Warcraft, and all World of Warcraft or Warcraft art, images,
# and lore are copyrighted by Blizzard Entertainment, Inc.
# 

# This file, when included, does some config options for GCC.

# GCC 5.4 or newer is required
execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
if (NOT (GCC_VERSION VERSION_GREATER 5.4 OR GCC_VERSION VERSION_EQUAL 5.4))
	message(FATAL_ERROR "GCC 5.4 or newer is required in order to build WoW++. Please consider an update or switch over to clang.")
endif()

# Enable C++14 standard
set (CMAKE_CXX_STANDARD 14)
list(APPEND CMAKE_CXX_FLAGS "-std=c++14 -pthread")

# Zlib is required for GCC builds
find_package(ZLIB REQUIRED)
