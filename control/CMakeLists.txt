include(CheckIncludeFiles)
include(CheckLibraryExists)

check_include_files(popt.h HAVE_POPT_H)
check_library_exists(popt poptFreeContext "" HAVE_POPT_LIB)

if(HAVE_POPT_H AND HAVE_POPT_LIB)
  add_executable(rru-control
    control.c
    )

  target_link_libraries(rru-control rru-common)
  target_link_libraries(rru-control popt)

  install(TARGETS rru-control DESTINATION bin)
else(HAVE_POPT_H AND HAVE_POPT_LIB)
  MESSAGE("Missing dependency popt, skipping rru-control.")
endif(HAVE_POPT_H AND HAVE_POPT_LIB)