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

# This file tries to find external dependencies and sets them up.


# ===============================================================================
# BOOST
# ===============================================================================

# Find boost static libraries
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.58.0 REQUIRED COMPONENTS system date_time iostreams filesystem regex chrono program_options unit_test_framework)
	
# Add boost include directories
include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

# Since boost 1.58 to make boost::variant::get<U>(...) happy
add_definitions("-DBOOST_VARIANT_USE_RELAXED_GET_BY_DEFAULT")



# ===============================================================================
# GOOGLE PROTOBUFFER
# ===============================================================================

# Find google protobuffer
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})



# ===============================================================================
# OPENSSL
# ===============================================================================

# Find OpenSSL
find_package(OpenSSL)
if (NOT OPENSSL_FOUND)
	message(FATAL_ERROR "Couldn't find OpenSSL libraries on your system. If you're on windows, please download and install Win64 OpenSSL v1.0.2o from this website: https://slproweb.com/products/Win32OpenSSL.html!")
endif()

# Check openssl version infos (right now, we want to make sure we don't use 1.1.X, but 1.0.2)
if (NOT OPENSSL_VERSION MATCHES "1.0.2[a-z]")
	message(FATAL_ERROR "OpenSSL 1.0.2 is required, but ${OPENSSL_VERSION} was detected! This would lead to compile errors.")
endif()

# Mark OpenSSL options as advanced
mark_as_advanced(LIB_EAY_DEBUG LIB_EAY_RELEASE SSL_EAY_DEBUG SSL_EAY_RELEASE OPENSSL_INCLUDE_DIR)

# Include OpenSSL include directories
include_directories(${OPENSSL_INCLUDE_DIR})
link_directories(${OPENSSL_LIBRARY_DIR})



# ===============================================================================
# MYSQL
# ===============================================================================

# Find MySQL
find_package(MYSQL REQUIRED)
include_directories(${MYSQL_INCLUDE_DIR})
