cmake_minimum_required(VERSION 2.8)

file(GLOB SOURCE_FILES *.cpp)
file(GLOB INCLUDE_FILES ../../inc/xmlrpc/*.[Hh])

source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Header Files" FILES ${INCLUDE_FILES})

include_directories(
  ../../inc
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_definitions(-D__LINUX__)
endif(CMAKE_SYSTEM_NAME STREQUAL "Linux")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_definitions(-D__WIN32__ -D__WINDOWS__ -D_WINDOWS)
endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")

if(CMAKE_SYSTEM_NAME MATCHES ".*BSD")
  add_definitions(-D__BSD__)
endif(CMAKE_SYSTEM_NAME MATCHES ".*BSD")

add_library(xmlrpc ${SOURCE_FILES} ${INCLUDE_FILES})

#Windows requires wsock32
if(WIN32)
  target_link_libraries(xmlrpc wsock32)
endif(WIN32)
