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

# This file detects the current compiler and warns if an unsupported compiler has been 
# detected. It will then include the respective compiler script from the CMake/Compilers
# directory to apply compiler-specific settings.

if (MSVC)
	include("Compilers/MSVC")
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
	include("Compilers/Clang")
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
	include("Compilers/GCC")
else()
	message(FATAL_ERROR "Unsupported compiler! Currently, WoW++ supports Microsoft Visual Studio 2015 or newer, GCC 5.4 or newer and Clang.")
endif()

