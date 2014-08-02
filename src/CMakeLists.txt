cmake_minimum_required(VERSION 2.8)

option(MKT_INTERACTIVE "If this is enabled, mkt will enable an interactive prompt." ON)
mark_as_advanced(MKT_INTERACTIVE)
if(MKT_INTERACTIVE)
  if(WIN32)
    add_subdirectory(editline)
  endif(WIN32)
endif(MKT_INTERACTIVE)

add_subdirectory(xmlrpc)
add_subdirectory(mkt)
