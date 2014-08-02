cmake_minimum_required(VERSION 2.6)

set (WinEditLine_VERSION_MAJOR 2)
set (WinEditLine_VERSION_MINOR 1)

set(INCLUDE_FILES              
  ../../inc/editline/readline.h
  el_globals.h
  histedit.h
  ${CMAKE_BINARY_DIR}/src/editline/config.h
)

set(SOURCE_FILES
  editline.c
  fn_complete.c
  history.c
)

source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Include Files" FILES ${INCLUDE_FILES})

include_directories(
  ../../inc
  ${CMAKE_BINARY_DIR}/src/editline
  .
)

add_library(editline ${SOURCE_FILES} ${INCLUDE_FILES})

configure_file (
  config.h.in
  ${CMAKE_BINARY_DIR}/src/editline/config.h
)
