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

# We want at least CMake 2.8
cmake_minimum_required(VERSION 2.8)

# Collect source and header files
file(GLOB srcFiles "./*.cpp" "./*.h" "./*.hpp")
remove_pch_cpp(srcFiles "${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp")

# Add source groups
source_group(src FILES ${srcFiles})

# Add library project
add_library(updater STATIC ${srcFiles})
add_precompiled_header(updater "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")

target_link_libraries(updater 
	math 
	${OPENSSL_LIBRARIES} 
	http_client
	virtual_directory
	common)

if(MINGW OR UNIX)
	target_link_libraries(updater
		${Boost_IOSTREAMS_LIBRARY}
		${ZLIB_LIBRARY}
	)
endif()

# Solution folder
if(MSVC)
	set_property(TARGET updater PROPERTY FOLDER "shared")
endif(MSVC)
