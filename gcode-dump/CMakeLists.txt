add_executable(rru-gcode-dump
  gcode-dump.c
  )

target_link_libraries(rru-gcode-dump rru-common)

install(TARGETS rru-gcode-dump DESTINATION bin)