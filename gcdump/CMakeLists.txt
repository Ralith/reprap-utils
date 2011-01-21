add_executable(gcdump
  gcdump.c
  )

target_link_libraries(gcdump reprap)

install(TARGETS gcdump DESTINATION bin)