add_executable(gcdump
  gcdump.c
  )

target_link_libraries(gcdump common)

install(TARGETS gcdump DESTINATION bin)