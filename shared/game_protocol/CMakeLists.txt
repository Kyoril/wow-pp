#
# This file is part of the WoW++ project.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Genral Public License as published by
# the Free Software Foudnation; either version 2 of the Licanse, or
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

# We want at least CMake 2.8
cmake_minimum_required(VERSION 2.8)

# This library contains commonly used things...
project(game_protocol)

	# Collect source and header files
	file(GLOB srcFiles "./*.cpp" "./*.h" "./*.hpp")
	file(GLOB networkFiles "./../network/*.h")
	file(GLOB binaryIOFiles "./../binary_io/*.h")
	
	# Add source groups
	source_group(src FILES ${srcFiles})
	source_group(src\\network FILES ${networkFiles})
	source_group(src\\binary_io FILES ${binaryIOFiles})
	
	# Add library project
	add_library(game_protocol ${srcFiles} ${networkFiles} ${binaryIOFiles})
	
	# Link shared libraries
	target_link_libraries(game_protocol common log game)

	# Solution folder
	if(MSVC)
		set_property(TARGET game_protocol PROPERTY FOLDER "shared")
	endif(MSVC)