cmake_minimum_required(VERSION 2.8)
project(trans-mkt)

# CMake Modules
set(CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}/CMake
  ${CMAKE_MODULE_PATH})

# Build output 
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
mark_as_advanced(LIBRARY_OUTPUT_PATH EXECUTABLE_OUTPUT_PATH)

# The version number.
set(transmkt_VERSION_MAJOR 0)
set(transmkt_VERSION_MINOR 1)

# The full version string
set(transmkt_VERSION ${transmkt_VERSION_MAJOR}.${transmkt_VERSION_MINOR})
message(STATUS "trans-mkt version: ${transmkt_VERSION}")

# include macros for setting up some library dependencies
include(SetupBoost)

# 05/11/2012 - transfix
# Possibly prevent crashing on OSX Lion
# http://groups.google.com/group/boost-list/browse_thread/thread/1c5a614a9e8c9808
if(APPLE)
  add_definitions(-D_GLIBCXX_FULLY_DYNAMIC_STRING)
endif(APPLE)

# newer versions of msvc enable c++11 by default. GCC & Clang need it specified.
if(NOT MSVC)
  set(CPP11_ARG "-std=c++11")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.7")
      set(CPP11_ARG "-std=c++0x") 
    endif()
  endif()

  # Initialize CXXFLAGS.
  set(CMAKE_CXX_FLAGS                "-Wall ${CPP11_ARG}")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O4 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
endif(NOT MSVC)

add_subdirectory (src)
