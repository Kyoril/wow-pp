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

# We want at least CMake 3.10
cmake_minimum_required(VERSION 3.10)

# This is the main project which contains all subprojects
project(WoW)
	
	# Add additional include directories as they are needed to make life easier
	include_directories(${PROJECT_BINARY_DIR})
	include_directories(${PROJECT_BINARY_DIR}/src)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR})
	
	# Include additional cmake modules
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

	# Add Project options
	include(Options)
	
	# Apply compiler settings
	include(CompilerSettings)
	
	# Find external dependencies
	include(ExternalDependencies)
	
	# Enable custom macros (mainly for project generation)
	include(Macros)
	
	# Gather commit infos and make them available via header files
	include(Version)
	
	# Detour should use 64 bit poly references
	add_definitions("-DDT_POLYREF64")

	# Add the shared-directory to the list of local include directories
	include_directories(deps)
	include_directories(src)
	
	# Include subdirectories which contain multiple subprojects
	add_subdirectory(deps)
	add_subdirectory(src)
	