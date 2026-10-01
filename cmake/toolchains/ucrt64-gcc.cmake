# MSYS2 UCRT64 — use: cmake --preset debug -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ucrt64-gcc.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER "C:/msys64/ucrt64/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "C:/msys64/ucrt64/bin/g++.exe")
set(CMAKE_RC_COMPILER "C:/msys64/ucrt64/bin/windres.exe")

set(CMAKE_CXX_STANDARD 20 CACHE STRING "" FORCE)
