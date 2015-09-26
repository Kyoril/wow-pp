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

# Include subdirectories
add_subdirectory(common)
add_subdirectory(log)
add_subdirectory(templates)
add_subdirectory(virtual_directory)
add_subdirectory(data)
add_subdirectory(base64)
add_subdirectory(http)
add_subdirectory(web_services)
add_subdirectory(nav_mesh)
add_subdirectory(game)
add_subdirectory(auth_protocol)
add_subdirectory(game_protocol)
add_subdirectory(wowpp_protocol)
add_subdirectory(sql_wrapper)
add_subdirectory(mysql_wrapper)
	