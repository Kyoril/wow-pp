#-------------------------------------------------------------------
# This file is part of the CMake build system for OGRE
#     (Object-oriented Graphics Rendering Engine)
# For the latest info, see http://www.ogre3d.org/
#
# The contents of this file are placed in the public domain. Feel
# free to make use of it in any way you like.
#-------------------------------------------------------------------

# This file prints a summary of the selected build features.

set(_features "\n")
set(_features "${_features}----------------------------------------------------------------------------\n")
set(_features "${_features}  FEATURE SUMMARY\n")
set(_features "${_features}----------------------------------------------------------------------------\n\n")

# summarise plugins
if (OGRE_BUILD_PLUGIN_OCTREE)
	set(_plugins "${_plugins}  + Octree scene manager\n")
endif ()
if (OGRE_BUILD_PLUGIN_PFX)
	set(_plugins "${_plugins}  + Particle FX\n")
endif ()

if (DEFINED _plugins)
	set(_features "${_features}Building plugins:\n${_plugins}")
endif ()

# summarise rendersystems
if (OGRE_BUILD_RENDERSYSTEM_GL)
	set(_rendersystems "${_rendersystems}  + OpenGL\n")
endif ()

if (DEFINED _rendersystems)
	set(_features "${_features}Building rendersystems:\n${_rendersystems}")
endif ()

set(_features "${_features}\n")

# miscellaneous
macro(var_to_string VAR STR)
	if (${VAR})
		set(${STR} "enabled")
	else ()
		set(${STR} "disabled")
	endif ()
endmacro ()

# allocator settings
if (OGRE_CONFIG_ALLOCATOR EQUAL 1)
	set(_allocator "standard")
elseif (OGRE_CONFIG_ALLOCATOR EQUAL 2)
	set(_allocator "nedmalloc")
elseif (OGRE_CONFIG_ALLOCATOR EQUAL 3)
	set(_allocator "user")
else ()
    set(_allocator "nedmalloc (pooling)")
endif()
# various true/false settings
var_to_string(OGRE_CONFIG_CONTAINERS_USE_CUSTOM_ALLOCATOR _containers)
var_to_string(OGRE_CONFIG_DOUBLE _double)
var_to_string(OGRE_CONFIG_MEMTRACK_DEBUG _memtrack_debug)
var_to_string(OGRE_CONFIG_MEMTRACK_RELEASE _memtrack_release)
var_to_string(OGRE_CONFIG_NEW_COMPILERS _compilers)
var_to_string(OGRE_CONFIG_STRING_USE_CUSTOM_ALLOCATOR _string)
var_to_string(OGRE_USE_BOOST _boost)
# threading settings
if (OGRE_CONFIG_THREADS EQUAL 0)
	set(_threads "none")
elseif (OGRE_CONFIG_THREADS EQUAL 1)
	set(_threads "full (${OGRE_CONFIG_THREAD_PROVIDER})")
else ()
	set(_threads "background (${OGRE_CONFIG_THREAD_PROVIDER})")
endif ()
# build type
if (OGRE_STATIC)
	set(_buildtype "static")
else ()
	set(_buildtype "dynamic")
endif ()

set(_features "${_features}Build type:                      ${_buildtype}\n")
set(_features "${_features}Threading support:               ${_threads}\n")
set(_features "${_features}Use double precision:            ${_double}\n")
set(_features "${_features}Allocator type:                  ${_allocator}\n")
set(_features "${_features}STL containers use allocator:    ${_containers}\n")
set(_features "${_features}Strings use allocator:           ${_string}\n")
set(_features "${_features}Memory tracker (debug):          ${_memtrack_debug}\n")
set(_features "${_features}Memory tracker (release):        ${_memtrack_release}\n")
set(_features "${_features}Use new script compilers:        ${_compilers}\n")
set(_features "${_features}Use Boost:                       ${_boost}\n")


set(_features "${_features}\n----------------------------------------------------------------------------\n")
message(STATUS ${_features})
