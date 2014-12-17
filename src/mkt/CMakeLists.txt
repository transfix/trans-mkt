cmake_minimum_required(VERSION 2.6)

if(WIN32)
  add_definitions(-D__WINDOWS__)
endif(WIN32)

set(INCLUDE_FILES
  ../../inc/mkt/commands.h
  ../../inc/mkt/exceptions.h
  ../../inc/mkt/types.h
  ../../inc/mkt/app.h
  ../../inc/mkt/threads.h
  ../../inc/mkt/vars.h
  ${CMAKE_BINARY_DIR}/inc/mkt/config.h
)

set(SOURCE_FILES
  app.cpp
  commands.cpp
  threads.cpp
  vars.cpp
)

source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Include Files" FILES ${INCLUDE_FILES})

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

option(MKT_USING_XMLRPC "If this is enabled, mkt will use xmlrpc" ON)
mark_as_advanced(MKT_USING_XMLRPC)
if(MKT_USING_XMLRPC)
  set(MKT_LINK_LIBS ${MKT_LINK_LIBS} xmlrpc)
  set(SOURCE_FILES
      ${SOURCE_FILES}
      xmlrpc.cpp)
endif(MKT_USING_XMLRPC)

#link against the readline implementations
if(MKT_INTERACTIVE)
  if(WIN32)
    set(MKT_LINK_LIBS ${MKT_LINK_LIBS} editline)
  else(WIN32)
    #message("Interactive mode not supported outside of Windows yet.")
    #set(MKT_INTERACTIVE OFF)
    
    #use external libedit if available
    find_package(libedit)
    if(LIBEDIT_FOUND)
      set(MKT_LINK_LIBS ${MKT_LINK_LIBS} ${LIBEDIT_LIBRARY})
    else(LIBEDIT_FOUND)
      message("Missing libedit, not supporting interactive mode")
      set(MKT_INTERACTIVE OFF)
    endif(LIBEDIT_FOUND)
  endif(WIN32)
endif(MKT_INTERACTIVE)

add_library(mkt ${SOURCE_FILES} ${INCLUDE_FILES})

SetupBoost(mkt)

target_link_libraries(mkt ${MKT_LINK_LIBS})

add_executable(trans-mkt trans-mkt.cpp)
target_link_libraries(trans-mkt mkt)

add_executable(trans-orderbook orderbook.cpp)
target_link_libraries(trans-orderbook mkt)

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/mkt/config.h.cmake ${CMAKE_BINARY_DIR}/inc/mkt/config.h )

if(WIN32)
  file(TO_NATIVE_PATH ${Boost_LIBRARY_DIRS} BOOST_LOCATION)
  file(TO_NATIVE_PATH ${CMAKE_BINARY_DIR} BINARIES_LOCATION)
  configure_file( trans-mkt.bat.cmake ${CMAKE_BINARY_DIR}/trans-mkt.bat )
  if(MSVC)
    configure_file( vs2012.bat.cmake ${CMAKE_BINARY_DIR}/vs2012.bat )
  endif(MSVC)
endif(WIN32)
