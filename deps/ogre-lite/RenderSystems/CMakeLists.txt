#-------------------------------------------------------------------
# This file is part of the CMake build system for OGRE
#     (Object-oriented Graphics Rendering Engine)
# For the latest info, see http://www.ogre3d.org/
#
# The contents of this file are placed in the public domain. Feel
# free to make use of it in any way you like.
#-------------------------------------------------------------------

# Configure RenderSystem build

if (OGRE_BUILD_RENDERSYSTEM_GL)
  if (NOT OPENGL_FOUND)
    MESSAGE(STATUS "Could not find dependency: GL")
	  MESSAGE(STATUS "Skipping GL rendersystem build")
    set(OGRE_BUILD_RENDERSYSTEM_GL 0)
  else ()
    add_subdirectory(GL)
  endif ()
endif ()
