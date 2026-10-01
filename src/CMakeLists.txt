include(CLPPTargets)

find_package(Threads REQUIRED)

# Libraries in dependency order. Each one links only what it really uses, so a missing
# boundary shows up as a link error instead of hiding inside one big object library.
#
#   clpp_std       stdlib natives (leaf: depends only on the public Value type)
#   clpp_front     lexer, parser, binder, type checker, analyze_program
#   clpp_compiler  stack bytecode (CLIR v1), verifiers, register lowering, Luau text backend
#   clpp_vm        register VM and GC
#   clpp_tools     IDE queries, LSP server, REPL
#
# Known debt (fatia F10): clpp_front and clpp_vm still call clpp_std directly
# (module_source, axiom_native, std_apply). They should receive natives through a
# registration table so the core can be embedded without the stdlib.

set(CLPP_PUBLIC_INCLUDES "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/src")

add_library(clpp_std STATIC
  stdlib/std_io.cpp
  stdlib/std_math.cpp
  stdlib/std_axiom.cpp
  stdlib/std_fs.cpp
  stdlib/std_os.cpp
  stdlib/std_http.cpp
)
target_include_directories(clpp_std PUBLIC ${CLPP_PUBLIC_INCLUDES})
clpp_configure_target(clpp_std)

add_library(clpp_front STATIC
  core/lexer/lexer.cpp
  core/parser/parser.cpp
  core/binder/binder.cpp
  core/typechecker/solver.cpp
  core/compiler/analyze.cpp
)
target_include_directories(clpp_front PUBLIC ${CLPP_PUBLIC_INCLUDES})
target_link_libraries(clpp_front PUBLIC clpp_std)
clpp_configure_target(clpp_front)

add_library(clpp_compiler STATIC
  core/compiler/bytecode_format.cpp
  core/compiler/bytecode_verifier.cpp
  core/compiler/stack_verifier.cpp
  core/compiler/clir_io.cpp
  core/compiler/luau_emit.cpp
  core/compiler/backend.cpp
  core/compiler/codegen.cpp
  core/compiler/register_lower.cpp
)
target_link_libraries(clpp_compiler PUBLIC clpp_front)
clpp_configure_target(clpp_compiler)

add_library(clpp_vm STATIC
  core/vm/gc.cpp
  core/vm/vm.cpp
)
target_link_libraries(clpp_vm PUBLIC clpp_compiler clpp_std Threads::Threads)
clpp_configure_target(clpp_vm)

# JSON for the language server: a real parser/serializer instead of hand-built strings
# (the old hand-written initialize response was invalid JSON).
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

add_library(clpp_tools STATIC
  core/ide/semantic.cpp
  core/ide/ide.cpp
  core/ide/lsp.cpp
  core/repl/repl.cpp
)
target_link_libraries(clpp_tools PUBLIC clpp_vm clpp_front PRIVATE nlohmann_json::nlohmann_json)
clpp_configure_target(clpp_tools)

# Everything at once, for the CLI, the tests and existing embedders.
add_library(clpp_core INTERFACE)
target_link_libraries(clpp_core INTERFACE clpp_tools clpp_vm clpp_compiler clpp_front clpp_std)

add_executable(clpp cli/main.cpp)
if(WIN32)
  enable_language(RC)
  target_sources(clpp PRIVATE cli/clpp.rc)  # icon + version info in Explorer
endif()
target_link_libraries(clpp PRIVATE clpp_core)
clpp_configure_target(clpp)
