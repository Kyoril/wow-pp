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

# We want at least CMake 2.8.11 for QT automoc
cmake_minimum_required(VERSION 2.8.11)

project(editor)

	if(POLICY CMP0043)
		cmake_policy(SET CMP0043 OLD)
	endif()
		
	# Enable CMake automatic moc file generation
	set(CMAKE_AUTOMOC ON)
	set(CMAKE_INCLUDE_CURRENT_DIR ON)
	
	# Find required Qt5 packages
	find_package(Qt5Widgets REQUIRED)
	find_package(Qt5Core REQUIRED)
	find_package(Qt5Gui REQUIRED)
	find_package(Qt5OpenGL REQUIRED)
	find_package(OpenGL REQUIRED)
	if(UNIX AND NOT APPLE)
		find_package(Qt5X11Extras REQUIRED)
	elseif(APPLE)
		find_package(Qt5MacExtras REQUIRED)
	endif()
	
	include_directories(${OPENGL_INCLUDE_DIR})

	file(GLOB srcFiles "*.cpp" "*.h")
	
	# Include ui files
	file(GLOB uiFiles "ui/*.ui")
	
	# Include qrc files
	file(GLOB resources "qrc/*.qrc")
	
	# Create cpp files out of qrc files
    	qt5_add_resources(resourceSources ${resources})
	
	# Create cpp header files out of ui files
	QT5_WRAP_UI(UIS_HDRS
		${uiFiles}
	)
	
	# Group files to filter automatic generated source and header files
	source_group(moc FILES ${UIS_HDRS} ${resourceSources})	# Auto-generated files (based on *.ui and *.qrc files)
	source_group(ui FILES ${uiFiles})						# *.ui files (xml)
	source_group(qrc FILES ${resources})					# *.qrc files (xml)
	
	# Group editor source files
	source_group(src FILES ${srcFiles})
	
	if(APPLE)
		set(apple_bundle_sources "ico101.icns")
		set_source_files_properties(
			"ico101.icns"
			PROPERTIES
			MACOSX_PACKAGE_LOCATION Resources
		)
		set(MACOSX_BUNDLE_ICON_FILE ico101.icns)
	endif()

	include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../deps/ogre-lite/OgreMain/include")
	include_directories("${CMAKE_BINARY_DIR}/deps/ogre-lite/include")
	if (OGRE_BUILD_RENDERSYSTEM_GL)
		include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../deps/ogre-lite/RenderSystems/GL/include")
		include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../deps/ogre-lite/RenderSystems/GL/src/GLSL/include")
	endif()
	if (OGRE_BUILD_RENDERSYSTEM_D3D9)
		include_directories("${DirectX_INCLUDE_DIR}")
		include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../deps/ogre-lite/RenderSystems/Direct3D9/include")
	endif()
	if (OGRE_BUILD_PLUGIN_OCTREE)
		include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../deps/ogre-lite/PlugIns/OctreeSceneManager/include")
	endif()
	if (OGRE_BUILD_PLUGIN_PFX)
		include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../deps/ogre-lite/PlugIns/ParticleFX/include")
	endif()
	
	# Create the application
	if(WIN32)
		add_executable(editor WIN32 editor.rc ${srcFiles} ${UIS_HDRS} ${resourceSources})
	elseif(APPLE)
		add_executable(editor MACOSX_BUNDLE ${srcFiles} ${UIS_HDRS} ${resourceSources} ${apple_bundle_sources})
	else()
		add_executable(editor ${srcFiles} ${UIS_HDRS} ${resourceSources})
	endif()
	
	# Link standard libraries
	target_link_libraries(editor common templates virtual_directory log data stormlib recast detour detour_crowd detour_tile_cache debug_utils)
	
	# Link OGRE and dependant libraries
	target_link_libraries(editor OgreMain)
	if(OGRE_STATIC)
		target_link_libraries(editor freetype zlib)
		if (OGRE_BUILD_RENDERSYSTEM_GL)
			target_link_libraries(editor RenderSystem_GL)
		endif()
		if (OGRE_BUILD_RENDERSYSTEM_D3D9)
			target_link_libraries(editor RenderSystem_Direct3D9)
			target_link_libraries(editor "${DirectX_LIBRARY}")
		endif()
		if (OGRE_BUILD_PLUGIN_OCTREE)
			target_link_libraries(editor Plugin_OctreeSceneManager)
		endif ()
		if (OGRE_BUILD_PLUGIN_PFX)
			target_link_libraries(editor Plugin_ParticleFX)
		endif ()
	endif()
	
	# Link dependency libraries
	target_link_libraries(editor ${Boost_LIBRARIES} ${OPENGL_LIBRARIES} ${OPENSSL_LIBRARIES})
	
	# Link Qt5 Libraries
	qt5_use_modules(editor Widgets OpenGL)
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
	