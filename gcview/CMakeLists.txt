find_package(SDL)

if(SDL_FOUND)
  find_package(OpenGL REQUIRED)
  if(OPENGL_FOUND)
    add_executable(gcview
      gcview.c)

    include_directories(${SDL_INCLUDE_DIR})
    target_link_libraries(gcview ${SDL_LIBRARY})
    
    include_directories(${OPENGL_INCLUDE_DIR})
    target_link_libraries(gcview ${OPENGL_LIBRARIES})  

    install(TARGETS gcview DESTINATION bin)
  else(OPENGL_FOUND)
    message("WARNING: OpenGL not found; gcview will not be built.")
  endif(OPENGL_FOUND)
else(SDL_FOUND)
  message("WARNING: SDL not found; gcview will not be built.")
endif(SDL_FOUND)
