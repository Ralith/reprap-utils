add_executable(gcdump
  gcdump.c
  )

add_definitions(-Wno-unused-parameter)

target_link_libraries(gcdump reprap)

install(TARGETS gcdump DESTINATION bin)