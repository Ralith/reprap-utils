add_executable(rru-control
  control.c
  )

target_link_libraries(rru-control rru-common)
target_link_libraries(rru-control popt)

install(TARGETS rru-control DESTINATION bin)
