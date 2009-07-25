add_executable(rru-gcode-gen
  gcode-gen.c
  )

target_link_libraries(rru-gcode-gen rru-common)
target_link_libraries(rru-gcode-gen)

install(TARGETS rru-gcode-gen DESTINATION bin)
