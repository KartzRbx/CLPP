cmake_minimum_required(VERSION 3.20)

project(clpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

option(CLPP_BUILD_TESTS "Build Catch2 unit tests" ON)
option(CLPP_SANITIZE_ADDRESS "Enable AddressSanitizer (Clang/GCC/MSVC)" OFF)
option(CLPP_ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)

include(CompilerWarnings)
include(Sanitizers)
include(ClangTidy)

add_subdirectory(src)

if(CLPP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
