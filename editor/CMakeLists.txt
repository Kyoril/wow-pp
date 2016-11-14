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

# We want at least CMake 2.8.11 for QT automoc
cmake_minimum_required(VERSION 2.8.11)

	if(POLICY CMP0043)
		cmake_policy(SET CMP0043 OLD)
	endif()
	
	find_package(OGRE REQUIRED)
	include_directories(${OGRE_INCLUDE_DIRS})
	
	# Needed for debug usage
	include_directories("${CMAKE_SOURCE_DIR}/deps/debug_utils")
	include_directories("${CMAKE_SOURCE_DIR}/deps/detour")
	include_directories("${CMAKE_SOURCE_DIR}/deps/detour_tile_cache")
		
	# Enable CMake automatic moc file generation
	set(CMAKE_AUTOMOC OFF)
	set(CMAKE_AUTOMOC_MOC_OPTIONS "-bpch.h")
	set(CMAKE_INCLUDE_CURRENT_DIR ON)
	
	# Find required Qt5 packages
	find_package(Qt5Widgets REQUIRED)
	find_package(Qt5Core REQUIRED)
	find_package(Qt5Gui REQUIRED)
	find_package(Qt5OpenGL REQUIRED)
	find_package(Qt5Concurrent REQUIRED)
	find_package(Qt5Multimedia REQUIRED)
	find_package(OpenGL REQUIRED)
	if(UNIX AND NOT APPLE)
		find_package(Qt5X11Extras REQUIRED)
	elseif(APPLE)
		find_package(Qt5MacExtras REQUIRED)
	endif()
	
	mark_as_advanced(Qt5Concurrent_DIR Qt5Core_DIR Qt5Gui_DIR Qt5Multimedia_DIR Qt5Network_DIR Qt5OpenGL_DIR Qt5Widgets_DIR OGRE_FRAMEWORK_PATH)
	
	include_directories(${OPENGL_INCLUDE_DIR})

	file(GLOB srcFiles "*.cpp" "*.h")
	remove_pch_cpp(srcFiles "${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp")
	file(GLOB mocFiles "windows/*.h" "editor_application.h" "ogre_wrappers/qt_ogre_window.h" "property_view_model.h")
	file(GLOB wndSrcFiles "windows/*.cpp")
	file(GLOB worldSrcFiles "world/*.cpp" "world/*.h")
	file(GLOB ogreSrcFiles "ogre_wrappers/*.cpp" "ogre_wrappers/*.h")
	QT5_WRAP_CPP(MOC_SOURCES "${mocFiles}" OPTIONS "-bpch.h")
	
	# Include ui files
	file(GLOB uiFiles "ui/*.ui")
	
	# Include qrc files
	file(GLOB resources "qrc/*.qrc")
	
	# Create cpp files out of qrc files
    qt5_add_resources(resourceSources ${resources})
	foreach(qrc_file ${resourceSources})
		if(WIN32)
			set_source_files_properties("${qrc_file}" PROPERTIES COMPILE_FLAGS "/Y-")
		else()
			if(APPLE)
			else()
				
			endif()
		endif()
	endforeach()
	
	# Create cpp header files out of ui files
	QT5_WRAP_UI(UIS_HDRS
		${uiFiles}
	)
	
	# Group files to filter automatic generated source and header files
	source_group(moc FILES ${UIS_HDRS} ${resourceSources} ${MOC_SOURCES})	# Auto-generated files (based on *.ui and *.qrc files)
	source_group(ui FILES ${uiFiles})						# *.ui files (xml)
	source_group(qrc FILES ${resources})					# *.qrc files (xml)
	
	file(GLOB windowSources "*_dialog.cpp" "*_dialog.h" "*_window.cpp" "*_window.h")
	
	# Group editor source files
	source_group(src FILES ${srcFiles})
	source_group(src\\windows FILES ${wndSrcFiles} ${mocFiles})
	source_group(src\\world FILES ${worldSrcFiles})
	source_group(src\\ogre_wrappers FILES ${ogreSrcFiles})
	
	if(APPLE)
		set(apple_bundle_sources "ico101.icns")
		set_source_files_properties(
			"ico101.icns"
			PROPERTIES
			MACOSX_PACKAGE_LOCATION Resources
		)
		set(MACOSX_BUNDLE_ICON_FILE ico101.icns)
	endif()
	
	# Create the application
	if(WIN32)
		add_executable(editor WIN32 editor.rc ${srcFiles} ${UIS_HDRS} ${mocFiles} ${resourceSources} ${wndSrcFiles} ${worldSrcFiles} ${ogreSrcFiles} ${MOC_SOURCES})
	elseif(APPLE)
		add_executable(editor MACOSX_BUNDLE ${srcFiles} ${UIS_HDRS} ${mocFiles} ${resourceSources} ${apple_bundle_sources} ${wndSrcFiles} ${worldSrcFiles} ${ogreSrcFiles} ${MOC_SOURCES})
	else()
		add_executable(editor ${srcFiles} ${UIS_HDRS} ${resourceSources} ${mocFiles} ${wndSrcFiles} ${worldSrcFiles} ${ogreSrcFiles} ${MOC_SOURCES})
	endif()
	add_precompiled_header(editor "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
	
	# Link standard libraries
	target_link_libraries(editor wowpp_protocol common virtual_directory log proto_data stormlib recast detour detour_crowd detour_tile_cache debug_utils game sql_wrapper mysql_wrapper math updater)
	
	# Link dependency libraries
	target_link_libraries(editor ${Boost_LIBRARIES} ${OPENGL_LIBRARIES} ${OPENSSL_LIBRARIES} ${OGRE_LIBRARIES} ${PROTOBUF_LIBRARIES} ${MYSQL_LIBRARY})
	
	# Link Qt5 Libraries
	qt5_use_modules(editor Widgets OpenGL Concurrent Multimedia)
	if(UNIX AND NOT APPLE)
		qt5_use_modules(editor X11Extras)
	elseif(APPLE)
		qt5_use_modules(editor MacExtras)
	endif()
	
	# Solution folder
	if(MSVC)
		set_property(TARGET editor PROPERTY FOLDER "programs")
	endif(MSVC)
	
	# Install target
	install(TARGETS editor
			RUNTIME DESTINATION bin
			LIBRARY DESTINATION lib
			BUNDLE DESTINATION bin
			ARCHIVE DESTINATION lib/static)
	