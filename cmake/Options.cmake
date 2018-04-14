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

# This file contains possible config options for the cmake project


# ===============================================================================
# OPTION DEFINITIONS
# ===============================================================================

# If enabled, assertions are enabled even in release builds
option(WOWPP_WITH_RELEASE_ASSERTS "Enables assertions in release builds." OFF)

# Change this to build WoW++ for a different client build. Currently, this option is not really used / supported.
set(WOWPP_SUPPORTED_CLIENT "8606" CACHE STRING "This sets the supported client build. Currently, WoW++ supports build (2.0.12.)6546 and (2.4.3.)8606")
if (NOT WOWPP_SUPPORTED_CLIENT MATCHES "6546" AND NOT WOWPP_SUPPORTED_CLIENT MATCHES "8606")
	message(FATAL_ERROR "Unsupported client build! WoW++ only supports build 6546 and 8606!")
endif()

# If enabled, the editor is built with patch support. By doing so, it will send an HTTP GET request on startup to the patch server
# and check for editor updates there. This is turned OFF by default as it makes developing impossible and should only be turned ON
# for distribution builds.
option(WOWPP_EDITOR_WITH_PATCH_SUPPORT "If checked, the editor will be built with patch support (checks for patches on startup)" OFF)

# If enabled, multiple c++ files are batched together for building. This might reduce build times but should not be used for developing
# but rather for distribution builds.
option(WOWPP_UNITY_BUILD "combine cpp files to accelerate compilation" OFF)

# The number of c++ files to be batched together when unity build is enabled.
set(WOWPP_UNITY_FILES_PER_UNIT "50" CACHE STRING "Number of files per compilation unit in Unity build.")

# Whether developer commands will be available for the servers (things like create item for players, which you don't want
# on a public server to avoid GM power abuse, but which you might need on development servers to test certain things quickly).
option(WOWPP_WITH_DEV_COMMANDS "If checked, developer commands will be available." OFF)

# If enabled, the editor will be built. The editor can be used to modify static game data like creature spawns etc.
# However, it's a heavy tool with heavy dependencies and thus you might not want to build this yourself, so this option is 
# turned OFF by default.
option(WOWPP_BUILD_EDITOR "If checked, will try to build the editor. You will need to have QT 5 installed to build this." OFF)

# If enabled, additional tools (most likely console tools) are build for things like navmesh generation. This does not require
# additional dependencies, but is turned OFF so that the standard cmake options will simply build the servers to make things
# easier for linux builds.
option(WOWPP_BUILD_TOOLS "If checked, will try to build tools." OFF)

# If enabled, unit tests will be built.
option(WOWPP_BUILD_TESTS "If checked, will try to test programs." ON)

# TODO: Add more options here as you need


# ===============================================================================
# OPTION APPLICATION
# ===============================================================================

if(WOWPP_WITH_DEV_COMMANDS)
	add_definitions("-DWOWPP_WITH_DEV_COMMANDS")
endif(WOWPP_WITH_DEV_COMMANDS)

if (WOWPP_WITH_RELEASE_ASSERTS)
	add_definitions("-DWOWPP_ALWAYS_ASSERT")
endif()

add_definitions("-DSUPPORTED_CLIENT_BUILD=${WOWPP_SUPPORTED_CLIENT}")

# TODO: Add more option-specific settings here as you need
