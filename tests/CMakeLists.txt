include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

include(CLPPTargets)
include(CTest)
include(Catch)

add_executable(clpp_unit_tests
  unit/test_lexer.cpp
  unit/test_parser.cpp
  unit/test_vm.cpp
  unit/test_ide.cpp
  unit/test_regressions.cpp
)

target_link_libraries(clpp_unit_tests PRIVATE clpp_core Catch2::Catch2WithMain)
target_include_directories(clpp_unit_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/src"
)

clpp_configure_target(clpp_unit_tests)

string(REPLACE "\\" "/" CLPP_SOURCE_POSIX "${CMAKE_SOURCE_DIR}")
target_compile_definitions(clpp_unit_tests PRIVATE "CLPP_SOURCE_DIR=\"${CLPP_SOURCE_POSIX}\"")

catch_discover_tests(clpp_unit_tests)

# Every example is a golden test: it must run from the repository root and print exactly
# the matching tests/examples/<file>.out. Examples without a .out file only need a clean exit.
file(GLOB CLPP_EXAMPLES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/examples/*.clp" "${CMAKE_SOURCE_DIR}/examples/*.clp")
foreach(example IN LISTS CLPP_EXAMPLES)
  get_filename_component(example_name "${example}" NAME)
  set(expected "${CMAKE_SOURCE_DIR}/tests/examples/${example_name}.out")
  if(NOT EXISTS "${expected}")
    set(expected "")
  endif()
  add_test(NAME "example ${example_name}"
           COMMAND ${CMAKE_COMMAND} -DCLPP=$<TARGET_FILE:clpp> -DSCRIPT=examples/${example_name}
                   -DEXPECTED=${expected} -P "${CMAKE_SOURCE_DIR}/tests/run_example.cmake"
           WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endforeach()

# Stress harnesses (moved from build/stress, where git ignored them). Short ones run with
# every ctest under the "stress" label; the long campaigns run only with CLPP_STRESS_LONG=ON
# (ctest -L long). They write scratch files under the build directory.
option(CLPP_BUILD_STRESS "Build the stress harnesses in tests/stress" ON)
option(CLPP_STRESS_LONG "Register the long stress campaigns (minutes each)" OFF)
if(CLPP_BUILD_STRESS)
  set(CLPP_STRESS_SHORT async_stress fs_stress func_stress link_stress match_stress
      oop_stress parallel_stress repl_stress self_stress spawn_stress struct_stress
      template_stress type_stress vector_stress)
  set(CLPP_STRESS_LONG_LIST all_stress campaign features_stress million_stress stress)
  foreach(harness IN LISTS CLPP_STRESS_SHORT CLPP_STRESS_LONG_LIST)
    add_executable(${harness} stress/${harness}.cpp)
    target_link_libraries(${harness} PRIVATE clpp_core)
    target_compile_options(${harness} PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-w>)
    clpp_apply_sanitizers(${harness})
  endforeach()
  foreach(harness IN LISTS CLPP_STRESS_SHORT)
    add_test(NAME "stress ${harness}" COMMAND ${harness} WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    set_tests_properties("stress ${harness}" PROPERTIES LABELS stress TIMEOUT 300)
  endforeach()
  if(CLPP_STRESS_LONG)
    foreach(harness IN LISTS CLPP_STRESS_LONG_LIST)
      add_test(NAME "long ${harness}" COMMAND ${harness} WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
      set_tests_properties("long ${harness}" PROPERTIES LABELS long TIMEOUT 3600)
    endforeach()
  endif()

  # Scripts kept next to the harnesses must keep running cleanly.
  file(GLOB CLPP_STRESS_SCRIPTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/stress/*.clp")
  foreach(script IN LISTS CLPP_STRESS_SCRIPTS)
    get_filename_component(script_name "${script}" NAME)
    set(expected "${CMAKE_SOURCE_DIR}/tests/stress/${script_name}.out")
    if(NOT EXISTS "${expected}")
      set(expected "")
    endif()
    add_test(NAME "script ${script_name}"
             COMMAND ${CMAKE_COMMAND} -DCLPP=$<TARGET_FILE:clpp> -DSCRIPT=tests/stress/${script_name}
                     -DEXPECTED=${expected} -P "${CMAKE_SOURCE_DIR}/tests/run_example.cmake"
             WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  endforeach()
endif()

# End-to-end language server test: the same JSON-RPC traffic VS Code sends.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME "lsp end to end"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/lsp/test_lsp.py" $<TARGET_FILE:clpp>)
  set_tests_properties("lsp end to end" PROPERTIES LABELS lsp TIMEOUT 120)
  # Every ```clp example in docs/ compiles, runs and prints what its ```saida block says.
  add_test(NAME "documentation examples"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/docs/check_docs.py" $<TARGET_FILE:clpp>
                   "${CMAKE_SOURCE_DIR}/docs")
  set_tests_properties("documentation examples" PROPERTIES LABELS docs TIMEOUT 300)
  add_test(NAME "legacy documentation examples"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/docs/check_docs.py" $<TARGET_FILE:clpp>
                   "${CMAKE_SOURCE_DIR}/tests/docs/legacy")
  set_tests_properties("legacy documentation examples" PROPERTIES LABELS docs TIMEOUT 300)
endif()

# The VS Code extension, activated against a mocked `vscode` API and the real server.
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
  add_test(NAME "vscode extension"
           COMMAND ${NODE_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/vscode/test_extension.js" $<TARGET_FILE:clpp>
           WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/vscode")
  set_tests_properties("vscode extension" PROPERTIES LABELS lsp TIMEOUT 120)
endif()
