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

# This file extracts the git commit count and other infos and fills them 
# in templated header files which are used to show version informations in the 
# compiled applications.

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

# Setup config files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${PROJECT_BINARY_DIR}/version.h @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/editor_config.h.in ${PROJECT_BINARY_DIR}/editor_config.h @ONLY)
