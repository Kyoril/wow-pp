#-------------------------------------------------------------------
# This file is part of the CMake build system for OGRE
#     (Object-oriented Graphics Rendering Engine)
# For the latest info, see http://www.ogre3d.org/
#
# The contents of this file are placed in the public domain. Feel
# free to make use of it in any way you like.
#-------------------------------------------------------------------

#####################################################
# Install dependencies 
#####################################################
if (NOT APPLE AND NOT WIN32)
  return()
endif()

# TODO - most of this file assumes a common dependencies root folder
# This is not robust, we should instead source dependencies from their individual locations
get_filename_component(OGRE_DEP_DIR ${OIS_INCLUDE_DIR}/../../ ABSOLUTE)

option(OGRE_INSTALL_DEPENDENCIES "Install dependency libs needed for samples" TRUE)
option(OGRE_COPY_DEPENDENCIES "Copy dependency libs to the build directory" TRUE)

macro(install_debug INPUT)
  if (EXISTS ${OGRE_DEP_DIR}/bin/debug/${INPUT})
    #install(FILES ${OGRE_DEP_DIR}/bin/debug/${INPUT} DESTINATION bin/debug CONFIGURATIONS Debug)
  else()
    message(send_error "${OGRE_DEP_DIR}/bin/debug/${INPUT} did not exist, can't install!")
  endif ()
endmacro()

macro(install_release INPUT)
  if (EXISTS ${OGRE_DEP_DIR}/bin/release/${INPUT})
    #install(FILES ${OGRE_DEP_DIR}/bin/release/${INPUT} DESTINATION bin/release CONFIGURATIONS Release None "")
    #install(FILES ${OGRE_DEP_DIR}/bin/release/${INPUT} DESTINATION bin/relwithdebinfo CONFIGURATIONS RelWithDebInfo)
	#install(FILES ${OGRE_DEP_DIR}/bin/release/${INPUT} DESTINATION bin/minsizerel CONFIGURATIONS MinSizeRel)
  else()
    message(send_error "${OGRE_DEP_DIR}/bin/release/${INPUT} did not exist, can't install!")
  endif ()
endmacro()

macro(copy_debug INPUT)
  if (EXISTS ${OGRE_DEP_DIR}/bin/debug/${INPUT})
    if (MINGW OR NMAKE)
      configure_file(${OGRE_DEP_DIR}/bin/debug/${INPUT} ${OGRE_BINARY_DIR}/bin/${INPUT} COPYONLY)
	else ()
      configure_file(${OGRE_DEP_DIR}/bin/debug/${INPUT} ${OGRE_BINARY_DIR}/bin/debug/${INPUT} COPYONLY)
	endif ()
  endif ()
endmacro()

macro(copy_release INPUT)
  if (EXISTS ${OGRE_DEP_DIR}/bin/release/${INPUT})
    if (MINGW OR NMAKE)
      configure_file(${OGRE_DEP_DIR}/bin/release/${INPUT} ${OGRE_BINARY_DIR}/bin/${INPUT} COPYONLY)
	else ()
      configure_file(${OGRE_DEP_DIR}/bin/release/${INPUT} ${OGRE_BINARY_DIR}/bin/release/${INPUT} COPYONLY)
      configure_file(${OGRE_DEP_DIR}/bin/release/${INPUT} ${OGRE_BINARY_DIR}/bin/relwithdebinfo/${INPUT} COPYONLY)
      configure_file(${OGRE_DEP_DIR}/bin/release/${INPUT} ${OGRE_BINARY_DIR}/bin/minsizerel/${INPUT} COPYONLY)
	endif ()
  endif ()
endmacro ()

if (OGRE_INSTALL_DEPENDENCIES)
  if (OGRE_STATIC)
    # for static builds, projects must link against all Ogre dependencies themselves, so copy full include and lib dir
    if (EXISTS ${OGRE_DEP_DIR}/include/)
	  #install(DIRECTORY ${OGRE_DEP_DIR}/include/ DESTINATION include)
	endif ()
	if (EXISTS ${OGRE_DEP_DIR}/lib/)
      #install(DIRECTORY ${OGRE_DEP_DIR}/lib/ DESTINATION lib)
	endif ()
  endif ()  
endif ()
