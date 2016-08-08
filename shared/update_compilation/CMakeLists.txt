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

# Collect source and header files
file(GLOB srcFiles "./*.cpp" "./*.h" "./*.hpp")
remove_pch_cpp(srcFiles "${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp")

# Add source groups
source_group(src FILES ${srcFiles})
	
# Add library project
add_library(update_compilation ${srcFiles})
add_precompiled_header(update_compilation "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")

target_link_libraries(update_compilation virtual_directory common)

# Solution folder
if(MSVC)
	set_property(TARGET update_compilation PROPERTY FOLDER "shared")
endif(MSVC)

if(MINGW OR UNIX)
	target_link_libraries(update_compilation
		${Boost_IOSTREAMS_LIBRARY}
		${Boost_FILESYSTEM_LIBRARY}
		${Boost_SYSTEM_LIBRARY}
		${ZLIB_LIBRARY}
	)
endif()
