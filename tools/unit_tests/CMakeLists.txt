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
	add_executable(unit_tests ${srcFiles})
	add_precompiled_header(unit_tests "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
	
	# Link required shared libs
	target_link_libraries(unit_tests common log game_protocol wowpp_protocol sql_wrapper mysql_wrapper virtual_directory game base64 http web_services proto_data math detour)
	
	# Link dependency libraries
	target_link_libraries(unit_tests ${Boost_LIBRARIES} ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} ${OPENSSL_LIBRARIES} ${MYSQL_LIBRARY} ${PROTOBUF_LIBRARIES} cppformat)
	if(UNIX AND NOT APPLE)
		target_link_libraries(unit_tests z)
	endif(UNIX AND NOT APPLE)

	# Install target
	install(TARGETS unit_tests
			RUNTIME DESTINATION bin
			LIBRARY DESTINATION lib
			ARCHIVE DESTINATION lib/static)

	# Solution folder
	if(MSVC)
		set_property(TARGET unit_tests PROPERTY FOLDER "tools")
	endif(MSVC)