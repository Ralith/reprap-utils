add_executable(gcgen
  gcgen.c
  )

target_link_libraries(gcgen common)

install(TARGETS gcgen DESTINATION bin)
