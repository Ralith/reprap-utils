add_executable(rru-control
  control.c
  )

target_link_libraries(rru-control rru-common)

install(TARGETS rru-control DESTINATION bin)
