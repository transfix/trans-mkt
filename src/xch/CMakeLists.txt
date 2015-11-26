cmake_minimum_required(VERSION 2.6)

if(WIN32)
  add_definitions(-D__WINDOWS__)
endif(WIN32)

set(INCLUDE_FILES
  ../../inc/xch/accounts.h
  ../../inc/xch/assets.h
)

set(SOURCE_FILES
  accounts.cpp
  assets.cpp
)

source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Include Files" FILES ${INCLUDE_FILES})

include_directories(
  ../../inc
  ${CMAKE_BINARY_DIR}/inc
  .
)

set(XCH_LINK_LIBS "mkt")

add_library(xch-s STATIC ${SOURCE_FILES} ${INCLUDE_FILES})
SetupBoost(xch-s)
target_link_libraries(xch-s ${XCH_LINK_LIBS})

# TODO: fix module linking
add_library(xch MODULE 
  ../../inc/xch/xch.h 
  xch.cpp 
  ${SOURCE_FILES} 
  ${INCLUDE_FILES}
)
SetupBoost(xch)

add_executable(trans-xch orderbook.cpp)
target_link_libraries(trans-xch xch-s)
