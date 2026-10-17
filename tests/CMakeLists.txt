include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Catch2)

# Catch2 and the tested code must agree on libc++ sanitizer container annotations.
# Mixed instrumented/uninstrumented Catch2 inline code can corrupt XML failure reports.
clpp_apply_sanitizers(Catch2)
clpp_apply_sanitizers(Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

include(CLPPTargets)
include(CTest)
include(Catch)
add_test(NAME "LLVM toolchain normalizes native path separators"
         COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/tests/distribution/check_toolchain_paths.cmake")

add_executable(clpp_unit_tests
  unit/test_lexer.cpp
  unit/test_parser.cpp
  unit/test_vm.cpp
  unit/test_ide.cpp
  unit/test_regressions.cpp
  unit/test_source_origins.cpp
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

add_executable(clpp_heap_lifetime_test runtime/shared_heap_lifetime.cpp)
target_link_libraries(clpp_heap_lifetime_test PRIVATE clpp_value)
clpp_configure_target(clpp_heap_lifetime_test)
add_test(NAME "shared heap native lifetime" COMMAND clpp_heap_lifetime_test)


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

file(GLOB CLPP_CAPABILITY_EXAMPLES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/examples/capabilities/*.clp")
foreach(example IN LISTS CLPP_CAPABILITY_EXAMPLES)
  get_filename_component(stem "${example}" NAME_WE)
  add_test(NAME "capability ${stem}"
           COMMAND ${CMAKE_COMMAND} -DCLPP=$<TARGET_FILE:clpp> -DSCRIPT=${example}
                   -DEXPECTED=${CMAKE_SOURCE_DIR}/tests/examples/capabilities/${stem}.out
                   -P "${CMAKE_SOURCE_DIR}/tests/run_example.cmake"
           WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  set_tests_properties("capability ${stem}" PROPERTIES LABELS capabilities TIMEOUT 30)
endforeach()

add_test(NAME "author Combat deterministic battle"
         COMMAND ${CMAKE_COMMAND} -DCLPP=$<TARGET_FILE:clpp> -DSCRIPT=examples/projects/combat/main.clp
                 -DEXPECTED=${CMAKE_SOURCE_DIR}/tests/examples/combat_author.out -P "${CMAKE_SOURCE_DIR}/tests/run_example.cmake"
         WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_test(NAME "author Pacman VM core"
         COMMAND ${CMAKE_COMMAND} -DCLPP=$<TARGET_FILE:clpp> -DSCRIPT=examples/projects/pacman/main.clp
                 -DEXPECTED=${CMAKE_SOURCE_DIR}/tests/examples/pacman_core.out -P "${CMAKE_SOURCE_DIR}/tests/run_example.cmake"
         WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# Stress harnesses (moved from build/stress, where git ignored them). Short ones run with
# every ctest under the "stress" label; the long campaigns run only with CLPP_STRESS_LONG=ON
# (ctest -L long). They write scratch files under the build directory.
option(CLPP_BUILD_STRESS "Build the stress harnesses in tests/stress" ON)
option(CLPP_STRESS_LONG "Register the long stress campaigns (minutes each)" OFF)
if(CLPP_BUILD_STRESS)
  add_executable(frontend_profile stress/frontend_profile.cpp)
  target_link_libraries(frontend_profile PRIVATE clpp_core)
  target_include_directories(frontend_profile PRIVATE "${CMAKE_SOURCE_DIR}/src")
  clpp_configure_target(frontend_profile)
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
  add_test(NAME "CLI LSP editor and installer release versions agree"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/distribution/test_versions.py" $<TARGET_FILE:clpp>)
  set_tests_properties("CLI LSP editor and installer release versions agree" PROPERTIES LABELS "distribution;contract" TIMEOUT 60)
  if(NOT CLPP_SANITIZE_ADDRESS AND NOT CLPP_SANITIZE_UNDEFINED)
    add_test(NAME "portable release archive runs outside the source tree"
             COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/distribution/test_release_archive.py" $<TARGET_FILE:clpp>)
    set_tests_properties("portable release archive runs outside the source tree" PROPERTIES LABELS "distribution" TIMEOUT 120)
    if(WIN32)
      add_test(NAME "Windows installer upgrade verification and uninstall"
               COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/distribution/test_windows_installer.py" $<TARGET_FILE:clpp>)
      set_tests_properties("Windows installer upgrade verification and uninstall" PROPERTIES LABELS "distribution" TIMEOUT 240)
    endif()
  endif()
  add_test(NAME "language semantics match independent generated oracles"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/stress/differential_validation.py" $<TARGET_FILE:clpp>)
  set_tests_properties("language semantics match independent generated oracles" PROPERTIES LABELS "core;contract" TIMEOUT 180)
  add_test(NAME "CLI imported runtime diagnostics preserve source paths"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/runtime/module_diagnostics.py" $<TARGET_FILE:clpp>)
  set_tests_properties("CLI imported runtime diagnostics preserve source paths" PROPERTIES LABELS "core;contract" TIMEOUT 60)
  add_test(NAME "CLI filesystem Unicode and NUL paths"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/runtime/filesystem_host.py" $<TARGET_FILE:clpp>)
  set_tests_properties("CLI filesystem Unicode and NUL paths" PROPERTIES LABELS "tooling;contract" TIMEOUT 60)
  add_test(NAME "CLI generic child process contract"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/runtime/tooling_host.py" $<TARGET_FILE:clpp>)
  set_tests_properties("CLI generic child process contract" PROPERTIES LABELS "tooling;contract" TIMEOUT 180)
  add_test(NAME "CLI bounded storage atomic writes and path identity"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/runtime/cli_files.py" $<TARGET_FILE:clpp>)
  set_tests_properties("CLI bounded storage atomic writes and path identity" PROPERTIES LABELS "tooling;contract" TIMEOUT 180)
  add_test(NAME "LSP formatter preserves the full example corpus"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/lsp/test_formatter.py" $<TARGET_FILE:clpp>)
  set_tests_properties("LSP formatter preserves the full example corpus" PROPERTIES LABELS "lsp;contract" TIMEOUT 240)
  add_test(NAME "LSP auto import accepts workspace and native modules"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/lsp/test_auto_import.py" $<TARGET_FILE:clpp>)
  set_tests_properties("LSP auto import accepts workspace and native modules" PROPERTIES LABELS "lsp;contract" TIMEOUT 120)
  if(CLPP_SANITIZE_ADDRESS)
    add_executable(clpp_reporter_probe runtime/catch_reporter.cpp)
    target_link_libraries(clpp_reporter_probe PRIVATE Catch2::Catch2WithMain)
    clpp_configure_target(clpp_reporter_probe)
    add_test(NAME "sanitizer JUnit failure reporting"
             COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/runtime/check_catch_reporter.py" $<TARGET_FILE:clpp_reporter_probe>)
    set_tests_properties("sanitizer JUnit failure reporting" PROPERTIES LABELS "qa;sanitizer" TIMEOUT 30)
  endif()
  add_test(NAME "lsp malformed transport remains bounded"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/lsp/test_transport.py" $<TARGET_FILE:clpp>)
  set_tests_properties("lsp malformed transport remains bounded" PROPERTIES LABELS "lsp;core" TIMEOUT 60)
  add_test(NAME "core adversarial validation"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/stress/core_validation.py" $<TARGET_FILE:clpp>)
  set_tests_properties("core adversarial validation" PROPERTIES LABELS "stress;core" TIMEOUT 180)
  add_test(NAME "all example sources use current syntax"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/examples/check_sources.py" $<TARGET_FILE:clpp>)
  set_tests_properties("all example sources use current syntax" PROPERTIES LABELS examples TIMEOUT 120)
  add_test(NAME "language contract examples and keyword coverage"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/examples/check_language_contract.py" $<TARGET_FILE:clpp>)
  set_tests_properties("language contract examples and keyword coverage" PROPERTIES LABELS "examples;docs;contract" TIMEOUT 120)
  add_test(NAME "CLI check does not execute native effects"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/runtime/check_only.py" $<TARGET_FILE:clpp>)
  add_test(NAME "lsp end to end"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/lsp/test_lsp.py" $<TARGET_FILE:clpp>)
  set_tests_properties("lsp end to end" PROPERTIES LABELS lsp TIMEOUT 120)
  add_test(NAME "LSP initializer fields preserve type and declaration ownership"
           COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/lsp/test_initializers.py" $<TARGET_FILE:clpp>)
  set_tests_properties("LSP initializer fields preserve type and declaration ownership" PROPERTIES LABELS "lsp;regression" TIMEOUT 120)
  if(CLPP_SANITIZE_ADDRESS OR CLPP_SANITIZE_UNDEFINED)
    # Instrumented correctness/responsiveness is a separate gate. Normal Debug
    # and Release builds retain the original 50 ms deployed-runtime threshold.
    set_tests_properties("lsp end to end" PROPERTIES ENVIRONMENT "CLPP_LSP_TIMING_BUDGET_MS=500")
  endif()
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
  add_test(NAME "editor discovers the installed native VM layout"
           COMMAND ${NODE_EXECUTABLE} --test "${CMAKE_SOURCE_DIR}/tests/vscode/test_discovery.cjs")
  set_tests_properties("editor discovers the installed native VM layout" PROPERTIES LABELS "lsp;distribution" TIMEOUT 30)
  add_test(NAME "vscode extension"
           COMMAND ${NODE_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/vscode/test_extension.js" $<TARGET_FILE:clpp>
           WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/vscode")
  set_tests_properties("vscode extension" PROPERTIES LABELS lsp TIMEOUT 120)
endif()
