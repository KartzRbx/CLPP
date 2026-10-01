function(clpp_set_compiler_warnings target)
  target_compile_features(${target} PUBLIC cxx_std_20)

  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wshadow
      -Wnon-virtual-dtor
      -Wold-style-cast
      -Wnull-dereference
      -Wdouble-promotion
    )
  endif()
endfunction()
