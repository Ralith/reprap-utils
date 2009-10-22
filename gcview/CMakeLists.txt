find_package(SDL)

if(SDL_FOUND)
  add_executable(gcview
    gcview.c)

  include_directories(${SDL_INCLUDE_DIRS})
  target_link_libraries(gcview ${SDL_LIBRARIES})

  install(TARGETS gcview DESTINATION bin)
else(SDL_FOUND)
  message("WARNING: SDL not found; gcview will not be built.")
endif(SDL_FOUND)
