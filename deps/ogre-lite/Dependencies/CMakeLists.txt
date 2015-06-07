if (APPLE AND NOT ANDROID)
  cmake_minimum_required(VERSION 2.8.8)
else()
  cmake_minimum_required(VERSION 2.6)
endif ()

project(OGREDEPS)
set(CMAKE_MODULE_PATH "${OGREDEPS_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})

if (APPLE)
  # Set 10.7 as the base SDK by default
  set(CMAKE_OSX_DEPLOYMENT_TARGET 10.7)

  if (NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES ${ARCHS_STANDARD_32_64_BIT})
  endif()
endif ()

add_subdirectory(src)
