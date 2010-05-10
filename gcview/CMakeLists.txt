find_package(GLUT)

if(GLUT_FOUND)
  find_package(OpenGL)
  if(OPENGL_FOUND)
    add_executable(gcview
      gcview.c)

    include_directories(${GLUT_INCLUDE_DIR})
    target_link_libraries(gcview ${GLUT_LIBRARY})
    
    include_directories(${OPENGL_INCLUDE_DIR})
    target_link_libraries(gcview ${OPENGL_LIBRARIES})  

    install(TARGETS gcview DESTINATION bin)
  else(OPENGL_FOUND)
    message("WARNING: OpenGL not found; gcview will not be built.")
  endif(OPENGL_FOUND)
else(GLUT_FOUND)
  message("WARNING: GLUT not found; gcview will not be built.")
endif(GLUT_FOUND)
