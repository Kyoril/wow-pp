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

cmake_minimum_required(VERSION 2.8.11)

# Collect source and header files
file(GLOB srcFiles "./*.cpp" "./*.h" "./*.hpp")
remove_pch_cpp(srcFiles "${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp")
file(GLOB sffFiles "../../shared/simple_file_format/*.h")
file(GLOB parserFiles "./parser/*.cpp" "./parser/*.h" "./parser/*.hpp")

include_directories("parser")

# Add source groups
source_group(src FILES ${srcFiles})
source_group(src\\parser FILES ${parserFiles})
source_group(src\\sff FILES ${sffFiles})

# Add library project
add_executable(extractor ${srcFiles} ${sffFiles} ${parserFiles})
add_precompiled_header(extractor "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")

# Link required shared libs
target_link_libraries(extractor common log game auth_protocol wowpp_protocol sql_wrapper mysql_wrapper stormlib math recast detour debug_utils)

# Link dependency libraries
target_link_libraries(extractor ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${MYSQL_LIBRARY} cppformat)

# Install target
install(TARGETS extractor
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib/static)
		
# Solution folder
if(MSVC)
	set_property(TARGET extractor PROPERTY FOLDER "tools")
endif(MSVC)