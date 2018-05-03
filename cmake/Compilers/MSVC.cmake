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

# This file, when included, does some config options for Microsoft Visual Studio.

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Make sure Visual Studio 2015 or newer is used
if (MSVC_VERSION LESS 1900)
	message(FATAL_ERROR "Visual Studio 2015 or newer is required in order to build WoW++! Please upgrade.")
endif()

# Make sure 64 bit build is used
if(NOT "${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
    message(FATAL_ERROR "WoW++ needs to be built using a 64 bit compiler! Please use Win64 Visual Studio build options.")
endif()

# Enable solution folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)


# We want to use Vista or later API since we need this for
# GetTickCount64 which is not available on XP and prior
add_definitions("-D_WIN32_WINNT=0x0600")

# Disable certain warnings, enable multi-cpu builds and allow big precompiled headers
add_definitions("/D_CRT_SECURE_NO_WARNINGS /D_SCL_SECURE_NO_WARNINGS /wd4267 /wd4244 /wd4800 /MP /D_WINSOCK_DEPRECATED_NO_WARNINGS /bigobj /Gy")
