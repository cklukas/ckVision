# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
file(REMOVE_RECURSE "${CKVISION_SCRATCH}")
file(MAKE_DIRECTORY "${CKVISION_SCRATCH}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${CKVISION_BUILD_DIR}" --config "${CKVISION_CONFIG}"
    --prefix "${CKVISION_SCRATCH}/installed" RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "ckVision package install failed: ${output}${error}")
endif()
set(executable "${CKVISION_SCRATCH}/installed/bin/ck-sysmon${CKVISION_SUFFIX}")
execute_process(COMMAND "${executable}" --version RESULT_VARIABLE status OUTPUT_VARIABLE version)
if(NOT status EQUAL 0 OR NOT version STREQUAL "ck-sysmon ${CKVISION_VERSION}\n")
    message(FATAL_ERROR "Installed monitor has wrong version: ${version}")
endif()
foreach(required IN ITEMS include/cvision/core/clock.hpp lib/cmake/ckvision/ckvisionConfig.cmake
        bin/ckvision_gallery${CKVISION_SUFFIX} bin/ckvision_logmonitor${CKVISION_SUFFIX}
        bin/ckvision_editor_profile_sample${CKVISION_SUFFIX})
    if(NOT EXISTS "${CKVISION_SCRATCH}/installed/${required}")
        message(FATAL_ERROR "Full ckVision package is missing ${required}")
    endif()
endforeach()
execute_process(COMMAND "${executable}" --sample RESULT_VARIABLE status OUTPUT_VARIABLE sample)
if(NOT status EQUAL 0 OR NOT sample MATCHES "^CKSYSMON 2\nSYSTEM\t")
    message(FATAL_ERROR "Installed monitor collector failed")
endif()
