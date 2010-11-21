find_package(SDL)

if(SDL_FOUND)
  find_package(OpenGL)
  if(OPENGL_FOUND)
    if(COMMAND cmake_policy)
      cmake_policy(SET CMP0003 NEW)
    endif(COMMAND cmake_policy)
    add_executable(gcview
      render.c
      gcview.c)

    include_directories(${SDL_INCLUDE_DIR})
    target_link_libraries(gcview ${SDL_LIBRARY})
    
    include_directories(${OPENGL_INCLUDE_DIR})
    target_link_libraries(gcview ${OPENGL_LIBRARIES})

    target_link_libraries(gcview common)

    install(TARGETS gcview DESTINATION bin)
  else(OPENGL_FOUND)
    message("WARNING: OpenGL not found; gcview will not be built.")
  endif(OPENGL_FOUND)
else(SDL_FOUND)
  message("WARNING: SDL not found; gcview will not be built.")
endif(SDL_FOUND)
