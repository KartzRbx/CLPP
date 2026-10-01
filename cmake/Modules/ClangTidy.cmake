if(NOT CLPP_ENABLE_CLANG_TIDY)
  return()
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(NOT CLANG_TIDY_EXE)
  message(WARNING "clang-tidy not found; set CLPP_ENABLE_CLANG_TIDY=OFF or install clang-tidy")
  return()
endif()

set(_clpp_clang_tidy_checks
  "-*,"
  "bugprone-*,"
  "cert-*,"
  "clang-analyzer-*,"
  "cppcoreguidelines-*,"
  "misc-*,"
  "modernize-*,"
  "performance-*,"
  "readability-*"
)

set(CMAKE_CXX_CLANG_TIDY
  "${CLANG_TIDY_EXE};"
  "-header-filter=${CMAKE_SOURCE_DIR}/(src|include)/.*;"
  "-checks=${_clpp_clang_tidy_checks}"
  CACHE STRING "clang-tidy command"
  FORCE
)

message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
