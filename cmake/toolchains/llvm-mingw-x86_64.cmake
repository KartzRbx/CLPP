# Cross-compile clpp.exe for Windows x64 from Linux with llvm-mingw
# (https://github.com/mstorsjo/llvm-mingw). Produces a self-contained executable (no DLLs).
#   cmake -S . -B build/win64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/llvm-mingw-x86_64.cmake -DLLVM_MINGW=/opt/llvm-mingw
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
if(NOT LLVM_MINGW)
  set(LLVM_MINGW "$ENV{LLVM_MINGW}")
endif()
set(CLPP_LLVM_TOOL_SUFFIX "")
if(CMAKE_HOST_WIN32)
  set(CLPP_LLVM_TOOL_SUFFIX ".exe")
endif()
set(CMAKE_C_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang${CLPP_LLVM_TOOL_SUFFIX}")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang++${CLPP_LLVM_TOOL_SUFFIX}")
set(CMAKE_RC_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-windres${CLPP_LLVM_TOOL_SUFFIX}")
set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW}/x86_64-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
if(NOT CLPP_SANITIZE_ADDRESS AND NOT CLPP_SANITIZE_UNDEFINED)
  set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
endif()
