# - Try to find MySQL.
# Once done this will define:
# MYSQL_FOUND			- If false, do not try to use MySQL.
# MYSQL_INCLUDE_DIRS	- Where to find mysql.h, etc.
# MYSQL_LIBRARIES		- The libraries to link against.
# MYSQL_VERSION_STRING	- Version in a string of MySQL.
#
# Created by RenatoUtsch based on eAthena implementation.
#
# Please note that this module only supports Windows and Linux officially, but
# should work on all UNIX-like operational systems too.
#

#=============================================================================
# Copyright 2012 RenatoUtsch
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

if( WIN32 )
    set(MYSQL_CONNECTOR_HINTS "$ENV{ProgramW6432}/MySQL/*Connector*/"
                              "$ENV{PROGRAMFILES}/MySQL/*Connector*/"
                              "$ENV{SYSTEMDRIVE}/MySQL/*Connector*/" )

    foreach (hint ${MYSQL_CONNECTOR_HINTS})
        if (NOT MYSQL_INCLUDE_DIR)
            file(GLOB_RECURSE MYSQL_INCLUDE_DIR "${hint}/include/mysql.h")
        endif()
        if (NOT MYSQL_LIBRARY)
            file(GLOB_RECURSE MYSQL_LIBRARY "${hint}/lib/libmysql.lib")
        endif()
        if (NOT MYSQL_LIBRARY)
            file(GLOB_RECURSE MYSQL_LIBRARY "${hint}/lib/mysqlclient.lib")
        endif()
        if (NOT MYSQL_LIBRARY)
            file(GLOB_RECURSE MYSQL_LIBRARY "${hint}/lib/mysqlclient_r.lib")
        endif()
    endforeach(hint)

    if (MYSQL_INCLUDE_DIR)
        get_filename_component(MYSQL_INCLUDE_DIR ${MYSQL_INCLUDE_DIR} DIRECTORY)
    endif()

    if(MYSQL_LIBRARY)
        string(REPLACE ".lib" ".dll" MYSQL_DLL ${MYSQL_LIBRARY})
        if (NOT EXISTS ${MYSQL_DLL})
            message(FATAL_ERROR "MySQL DLL matching ${MYSQL_LIBRARY} not found")
        endif()
    endif()
else()
	find_path( MYSQL_INCLUDE_DIR
		NAMES "mysql.h"
		PATHS "/usr/include/mysql"
			  "/usr/local/include/mysql"
			  "/usr/mysql/include/mysql" )
	
	find_library( MYSQL_LIBRARY
		NAMES "mysqlclient" "mysqlclient_r libmysql"
		PATHS "/lib/mysql"
			  "/lib64/mysql"
			  "/usr/lib/mysql"
			  "/usr/lib64/mysql"
			  "/usr/local/lib/mysql"
			  "/usr/local/lib64/mysql"
			  "/usr/mysql/lib/mysql"
			  "/usr/mysql/lib64/mysql" )
endif()

if( MYSQL_INCLUDE_DIR AND EXISTS "${MYSQL_INCLUDE_DIR}/mysql_version.h" )
	file( STRINGS "${MYSQL_INCLUDE_DIR}/mysql_version.h"
		MYSQL_VERSION_H REGEX "^#define[ \t]+MYSQL_SERVER_VERSION[ \t]+\"[^\"]+\".*$" )
	string( REGEX REPLACE
		"^.*MYSQL_SERVER_VERSION[ \t]+\"([^\"]+)\".*$" "\\1" MYSQL_VERSION_STRING
		"${MYSQL_VERSION_H}" )
	message(STATUS "MySQL Version: ${MYSQL_VERSION_STRING}")
else()
	message(STATUS "MySQL header doesn't exist")
endif()

set( MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR} )
set( MYSQL_LIBRARIES ${MYSQL_LIBRARY} )

mark_as_advanced( MYSQL_INCLUDE_DIR MYSQL_LIBRARY )