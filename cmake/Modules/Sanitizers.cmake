function(clpp_apply_sanitizers target)
  if(CLPP_SANITIZE_ADDRESS)
    if(MSVC)
      target_compile_options(${target} PRIVATE /fsanitize=address)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
      target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address)
    else()
      message(FATAL_ERROR "AddressSanitizer not supported for ${CMAKE_CXX_COMPILER_ID}")
    endif()
  endif()
  if(CLPP_SANITIZE_UNDEFINED)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT MSVC)
      target_compile_options(${target} PRIVATE -fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=undefined)
    else()
      message(FATAL_ERROR "UndefinedBehaviorSanitizer requires a Clang/GCC toolchain with its runtime")
    endif()
  endif()
  # LLVM MinGW links ASan through a DLL. Stage that runtime beside each executable
  # so tests launched in a temporary directory do not depend on the caller's PATH.
  # Keep instrumented runtimes out of ordinary Release/distribution builds.
  get_target_property(clpp_sanitized_target_type ${target} TYPE)
  if(CLPP_SANITIZE_ADDRESS AND WIN32 AND NOT MSVC AND
     CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
     clpp_sanitized_target_type STREQUAL "EXECUTABLE")
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -dumpmachine
                    OUTPUT_VARIABLE clpp_sanitizer_triple
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE clpp_sanitizer_query_status)
    if(NOT clpp_sanitizer_query_status EQUAL 0)
      message(FATAL_ERROR "Cannot identify the Windows ASan runtime architecture")
    endif()
    string(REGEX REPLACE "-.*$" "" clpp_sanitizer_arch "${clpp_sanitizer_triple}")
    get_filename_component(clpp_sanitizer_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_file(CLPP_ASAN_RUNTIME
              NAMES "libclang_rt.asan_dynamic-${clpp_sanitizer_arch}.dll"
              HINTS "${clpp_sanitizer_compiler_bin}"
              NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH
              DOC "Matching LLVM MinGW AddressSanitizer DLL")
    if(NOT CLPP_ASAN_RUNTIME OR NOT EXISTS "${CLPP_ASAN_RUNTIME}")
      message(FATAL_ERROR "Missing Windows ASan DLL; set CLPP_ASAN_RUNTIME to the matching compiler runtime")
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different
                               "${CLPP_ASAN_RUNTIME}" "$<TARGET_FILE_DIR:${target}>"
                       VERBATIM)
    foreach(clpp_sanitizer_dependency libc++.dll libunwind.dll)
      if(EXISTS "${clpp_sanitizer_compiler_bin}/${clpp_sanitizer_dependency}")
        add_custom_command(TARGET ${target} POST_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                   "${clpp_sanitizer_compiler_bin}/${clpp_sanitizer_dependency}"
                                   "$<TARGET_FILE_DIR:${target}>"
                           VERBATIM)
      endif()
    endforeach()
  endif()
endfunction()
