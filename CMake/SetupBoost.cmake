#
# This macro is for setting up Boost for a target
#

macro(SetupBoost TargetName)
 set(Boost_USE_STATIC_LIBS OFF)
 set(Boost_USE_MULTITHREAD ON)
 set(Boost_DEBUG ON)
 find_package(Boost COMPONENTS thread date_time regex filesystem system)
 if(Boost_FOUND)
   add_definitions(-DBOOST_ALL_DYN_LINK)
   if(MSVC11)
     add_definitions(-D_VARIADIC_MAX=10)
     add_definitions(-DBOOST_ALL_NO_LIB)
   endif(MSVC11)
   include_directories(${Boost_INCLUDE_DIRS})
   link_directories(${Boost_LIBRARY_DIRS})
   set(LINK_LIBS ${Boost_LIBRARIES}) 
   message("Boost includes: ${Boost_INCLUDE_DIRS}")
   message("Boost libraries: ${Boost_LIBRARIES}")
   target_link_libraries(${TargetName}
     ${LINK_LIBS}
   )
 else(Boost_FOUND)
   message(SEND_ERROR "If you're having trouble finding boost, set environment variables "
           "BOOST_INCLUDEDIR and BOOST_LIBRARYDIR to the appropriate paths")
 endif(Boost_FOUND)
endmacro(SetupBoost)
