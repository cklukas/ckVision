# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
if(NOT DEFINED CKTEST_PROBE)
    message(FATAL_ERROR "cktest check contract requires its exact probe executable")
endif()
foreach(case IN ITEMS constant_true condition_evaluates_once_and_preserves_contextual_negation)
    execute_process(COMMAND "${CKTEST_PROBE}" --case "${case}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    if(NOT result EQUAL 0 OR NOT output MATCHES "RUN [^\n]*:${case}")
        message(FATAL_ERROR "check contract ${case} did not execute successfully (${result}): ${output}${errors}")
    endif()
endforeach()
execute_process(COMMAND "${CKTEST_PROBE}" --case constant_false
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 1 OR NOT errors MATCHES "FAIL constant_false: false")
    message(FATAL_ERROR "constant false did not report its real assertion failure (${result}): ${output}${errors}")
endif()
