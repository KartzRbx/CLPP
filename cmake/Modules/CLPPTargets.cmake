function(clpp_configure_target target)
  clpp_set_compiler_warnings(${target})
  clpp_apply_sanitizers(${target})
endfunction()
