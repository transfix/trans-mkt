cmake_minimum_required(VERSION 2.6)

if(WIN32)
  add_definitions(-D__WINDOWS__)
endif(WIN32)

set(INCLUDE_FILES
  ../../inc/mkt/exceptions.h
  ../../inc/mkt/app.h
)

set(SOURCE_FILES
  app.cpp
)

source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Header Files" FILES ${INCLUDE_FILES})

include_directories(
  ../../inc
  ${CMAKE_BINARY_DIR}/inc
  .
)

# The libmkt version number.
set(MKT_VERSION_MAJOR 0)
set(MKT_VERSION_MINOR 1)

# The full version string
set(MKT_VERSION ${MKT_VERSION_MAJOR}.${MKT_VERSION_MINOR})
message(STATUS "libmkt version: ${MKT_VERSION}")

set(MKT_LINK_LIBS "")

#option(MKT_USING_XMLRPC "If this is enabled, mkt will use xmlrpc" ON)
#mark_as_advanced(MKT_USING_XMLRPC)
#if(MKT_USING_XMLRPC)
#  set(MKT_LINK_LIBS ${MKT_LINK_LIBS} xmlrpc)
#  set(SOURCE_FILES
#      ${SOURCE_FILES}
#      xmlrpc_client.cpp
#      xmlrpc_server.cpp)
#  add_definitions(-DUSING_XMLRPC)
#endif(MKT_USING_XMLRPC)

#if poco is found, use it for the http server.
#find_package(POCO)
#if(POCO_FOUND)
#  include_directories(${POCO_INCLUDE_DIRS})
#  set(CVC_LINK_LIBS ${CVC_LINK_LIBS} ${POCO_LIBRARIES})
#  add_definitions(-DCVC_USING_POCO)
#  message("Poco libs: " ${POCO_LIBRARIES})
#endif(POCO_FOUND)

add_library(mkt ${SOURCE_FILES} ${INCLUDE_FILES})

SetupBoost(mkt)

target_link_libraries(mkt ${MKT_LINK_LIBS})

add_executable(trans-mkt trans-mkt.cpp)
target_link_libraries(trans-mkt mkt)

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/mkt/config.h.cmake ${CMAKE_BINARY_DIR}/inc/mkt/config.h )

if(WIN32)
  file(TO_NATIVE_PATH ${Boost_LIBRARY_DIRS} BOOST_LOCATION)
  file(TO_NATIVE_PATH ${CMAKE_BINARY_DIR} BINARIES_LOCATION)
  configure_file( trans-mkt.bat.cmake ${CMAKE_BINARY_DIR}/trans-mkt.bat )
endif(WIN32)
