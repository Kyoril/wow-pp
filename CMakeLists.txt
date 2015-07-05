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

# This is the main project which contains all subprojects
project(WoW)

	# Enable solution folders in msvc
	if(MSVC)
		set_property(GLOBAL PROPERTY USE_FOLDERS ON)
	endif(MSVC)
	
	# Version header
	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
		find_package(Git)
		if(GIT_FOUND)
			execute_process(
				COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
				WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
				OUTPUT_VARIABLE "WOWPP_GIT_COMMIT"
				ERROR_QUIET
				OUTPUT_STRIP_TRAILING_WHITESPACE)
			execute_process(
				COMMAND ${GIT_EXECUTABLE} rev-list HEAD --count
				WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
				OUTPUT_VARIABLE "WOWPP_GIT_REVISION"
				ERROR_QUIET
				OUTPUT_STRIP_TRAILING_WHITESPACE)
			execute_process(
				COMMAND ${GIT_EXECUTABLE} show -s --format=%ci
				WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
				OUTPUT_VARIABLE "WOWPP_GIT_LASTCHANGE"
				ERROR_QUIET
				OUTPUT_STRIP_TRAILING_WHITESPACE)
			message(STATUS "Git commit: ${WOWPP_GIT_COMMIT}")
			message(STATUS "Last change: ${WOWPP_GIT_LASTCHANGE}")
		else(GIT_FOUND)
			set(WOWPP_GIT_COMMIT 0)
			set(WOWPP_GIT_REVISION 0)
		endif(GIT_FOUND)
	endif()

	include_directories(${PROJECT_BINARY_DIR})
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${PROJECT_BINARY_DIR}/version.h @ONLY)
	
	# Enable C++11 if needed
	if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
		list(APPEND CMAKE_CXX_FLAGS "-std=c++11")
		if(APPLE)
			set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11")
			set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
		endif()
	elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
		list(APPEND CMAKE_CXX_FLAGS "-std=c++0x -pthread")
	endif()

	# Include additional cmake modules
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

	# Find boost
	set(Boost_USE_STATIC_LIBS ON)
	find_package(Boost 1.54.0 REQUIRED COMPONENTS system date_time thread iostreams filesystem regex chrono program_options)
	
	# Add boost include directories
	include_directories(${Boost_INCLUDE_DIR})
	link_directories(${Boost_LIBRARY_DIRS})

	# Since boost 1.58 to make boost::variant::get<U>(...) happy
	add_definitions("-DBOOST_VARIANT_USE_RELAXED_GET_BY_DEFAULT")
	
	# Find OpenSSL
	find_package(OpenSSL REQUIRED)
	
	# Add boost include directories
	include_directories(${OPENSSL_INCLUDE_DIR})
	link_directories(${OPENSSL_LIBRARY_DIR})
	
	# Find MySQL
	find_package(MYSQL REQUIRED)
	
	# Add mysql include directories
	include_directories(${MYSQL_INCLUDE_DIR})
	
	# We want to use Vista or later API since we need this for
	# GetTickCount64 which is not available on XP and prior
	if(WIN32)
		add_definitions("-D_WIN32_WINNT=0x0600")
	endif(WIN32)
	
	if(MSVC)
		add_definitions("/D_CRT_SECURE_NO_WARNINGS /D_SCL_SECURE_NO_WARNINGS /wd4267 /wd4244 /MP /D_WINSOCK_DEPRECATED_NO_WARNINGS")
	endif(MSVC)
	
	# Editor option
	option(WOWPP_BUILD_EDITOR "If checked, will try to build the editor. You will need to have OgreSDK and QT 5 installed to build this." OFF)
	
	# Add the shared-directory to the list of local include directories
	include_directories(deps)
	include_directories(shared)
	
	# Include subdirectories which contain multiple subprojects
	add_subdirectory(deps)
	add_subdirectory(shared)
	add_subdirectory(login)
	add_subdirectory(realm)
	add_subdirectory(world)
	if (WOWPP_BUILD_EDITOR)
		add_subdirectory(editor)
	endif()