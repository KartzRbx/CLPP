# Runs one example with the clpp CLI and compares stdout with the expected file.
# Arguments: CLPP, SCRIPT, EXPECTED (optional: when absent, only a clean exit is required).
execute_process(
  COMMAND "${CLPP}" "${SCRIPT}"
  OUTPUT_VARIABLE actual
  ERROR_VARIABLE errors
  RESULT_VARIABLE code)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "${SCRIPT} exited with ${code}\nstderr:\n${errors}")
endif()
if(DEFINED EXPECTED AND NOT EXPECTED STREQUAL "")
  file(READ "${EXPECTED}" expected)
  string(REPLACE "\r\n" "\n" actual "${actual}")
  string(REPLACE "\r\n" "\n" expected "${expected}")
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "${SCRIPT}: output differs\n--- expected\n${expected}--- actual\n${actual}")
  endif()
endif()
