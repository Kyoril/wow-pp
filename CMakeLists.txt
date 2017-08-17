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

# We want at least CMake 3.1
cmake_minimum_required(VERSION 3.1)

# This is the main project which contains all subprojects
project(WoW)

	option(WOWPP_WITH_RELEASE_ASSERTS "Enables assertions in release builds." OFF)
	if (WOWPP_WITH_RELEASE_ASSERTS)
		add_definitions("-DWOWPP_ALWAYS_ASSERT")
	endif()

	# Enable solution folders in msvc
	if(MSVC)
		set_property(GLOBAL PROPERTY USE_FOLDERS ON)
	endif(MSVC)

	if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++14")
		set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
		set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++14 -stdlib=libc++")
	endif()
	
	set(WOWPP_SUPPORTED_CLIENT "8606" CACHE STRING "This sets the supported client build. Currently, WoW++ supports build (2.0.12.)6546 and (2.4.3.)8606")
	if (NOT WOWPP_SUPPORTED_CLIENT MATCHES "6546" AND NOT WOWPP_SUPPORTED_CLIENT MATCHES "8606")
		message(FATAL_ERROR "Unsupported client build! WoW++ only supports build 6546 and 8606!")
	endif()
	add_definitions("-DSUPPORTED_CLIENT_BUILD=${WOWPP_SUPPORTED_CLIENT}")
	add_definitions("-DDT_POLYREF64")
	
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
			execute_process(
				COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
				WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
				OUTPUT_VARIABLE "WOWPP_GIT_BRANCH"
				ERROR_QUIET
				OUTPUT_STRIP_TRAILING_WHITESPACE)
			message(STATUS "Git branch: ${WOWPP_GIT_BRANCH}")
			message(STATUS "Git commit: ${WOWPP_GIT_COMMIT}")
			message(STATUS "Last change: ${WOWPP_GIT_LASTCHANGE}")
		else(GIT_FOUND)
			set(WOWPP_GIT_COMMIT "UNKNOWN")
			set(WOWPP_GIT_REVISION 0)
			set(WOWPP_GIT_LASTCHANGE "UNKNOWN")
			set(WOWPP_GIT_BRANCH "UNKNOWN")
		endif(GIT_FOUND)
	endif()
	
	# Editor option
	option(WOWPP_EDITOR_WITH_PATCH_SUPPORT "If checked, the editor will be built with patch support (checks for patches on startup)" OFF)

	include_directories(${PROJECT_BINARY_DIR})
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${PROJECT_BINARY_DIR}/version.h @ONLY)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/editor_config.h.in ${PROJECT_BINARY_DIR}/editor_config.h @ONLY)
	
	include_directories (${CMAKE_BINARY_DIR})

	# Enable C++14 if needed
	if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
		list(APPEND CMAKE_CXX_FLAGS "-std=c++1y -stdlib=libc++")
		#list(APPEND CMAKE_C_FLAGS "-std=c++14 -stdlib=libc++")
		if(APPLE)
			set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++14")
			set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
		endif()
	elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
		execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                OUTPUT_VARIABLE GCC_VERSION)
		if (NOT (GCC_VERSION VERSION_GREATER 4.9 OR GCC_VERSION VERSION_EQUAL 4.9))
			message(FATAL_ERROR "GCC 4.9 or newer is required in order to build WoW++. Please consider an update or switch over to clang.")
		endif()
	
		set (CMAKE_CXX_STANDARD 14)
		list(APPEND CMAKE_CXX_FLAGS "-std=c++14 -pthread")
		
		find_package(ZLIB REQUIRED)
	endif()

	# Include additional cmake modules
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
	
	option(WOWPP_UNITY_BUILD "combine cpp files to accelerate compilation" OFF)
	set(WOWPP_UNITY_FILES_PER_UNIT "50" CACHE STRING "Number of files per compilation unit in Unity build.")
	include(Macros)
	
	# Find boost
    # currently there is a bug in the cmake included with visual studio where the wrong
    # version number is resolved for the compiler.  this should work around it.
    # FIXME: this should be removed once visual studio begins using cmake 3.8
    if (MSVC)
	    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 19.10)
            set(Boost_COMPILER -vc141)
        endif()
    endif()

	set(Boost_USE_STATIC_LIBS ON)
	find_package(Boost 1.54.0 REQUIRED COMPONENTS system date_time thread iostreams filesystem regex chrono program_options unit_test_framework)
	
	# Find protobuf
	find_package(Protobuf REQUIRED)
	include_directories(${PROTOBUF_INCLUDE_DIRS})
	
	include_directories(${CMAKE_CURRENT_SOURCE_DIR})
	
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
	
	# Build with developer commands
	option(WOWPP_WITH_DEV_COMMANDS "If checked, developer commands will be available." OFF)
	if(WOWPP_WITH_DEV_COMMANDS)
		add_definitions("-DWOWPP_WITH_DEV_COMMANDS")
	endif(WOWPP_WITH_DEV_COMMANDS)
	
	# We want to use Vista or later API since we need this for
	# GetTickCount64 which is not available on XP and prior
	if(WIN32)
		add_definitions("-D_WIN32_WINNT=0x0600")
	endif(WIN32)
	
	if(MSVC)
		add_definitions("/D_CRT_SECURE_NO_WARNINGS /D_SCL_SECURE_NO_WARNINGS /wd4267 /wd4244 /wd4800 /MP /D_WINSOCK_DEPRECATED_NO_WARNINGS /bigobj /Gy")
	endif(MSVC)
	
	# Editor option
	option(WOWPP_BUILD_EDITOR "If checked, will try to build the editor. You will need to have QT 5 installed to build this." OFF)
	
	# Tools option
	option(WOWPP_BUILD_TOOLS "If checked, will try to build tools." OFF)
	
	# Tests option
	option(WOWPP_BUILD_TESTS "If checked, will try to test programs." OFF)
	
	# Add the shared-directory to the list of local include directories
	include_directories(deps)
	include_directories(shared)
	
	# Include subdirectories which contain multiple subprojects
	add_subdirectory(deps)
	add_subdirectory(shared)
	add_subdirectory(servers)
	add_subdirectory(tools)