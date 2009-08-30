project(reprap-tools)

cmake_minimum_required(VERSION 2.4.6)

add_definitions( -Wall )

if(UNIX)
  add_definitions(-DUNIX)
  if(APPLE)
    add_definitions(-DAPPLE)
  else(APPLE)
    add_definitions(-DLINUX)
  endif(APPLE)
else(UNIX)
  if(WIN32)
    add_definitions(-DWINDOWS)
  endif(WIN32)
endif(UNIX)

add_subdirectory(common)

add_subdirectory(gcdump)
add_subdirectory(gcgen)