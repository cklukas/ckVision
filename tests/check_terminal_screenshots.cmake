# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT

foreach(required IN ITEMS SOURCE_DIR ARTIFACT_DIR TERMINAL_CAPTURE)
    if(NOT DEFINED CKVISION_${required})
        message(FATAL_ERROR "terminal screenshot check requires CKVISION_${required}")
    endif()
endforeach()

file(REMOVE_RECURSE "${CKVISION_ARTIFACT_DIR}")
file(MAKE_DIRECTORY "${CKVISION_ARTIFACT_DIR}")
execute_process(
    COMMAND "${CKVISION_TERMINAL_CAPTURE}" "${CKVISION_ARTIFACT_DIR}"
    RESULT_VARIABLE capture_result)
if(NOT capture_result EQUAL 0)
    message(FATAL_ERROR "TerminalApp screenshot capture failed")
endif()

foreach(name IN ITEMS terminal-initial terminal-initial-dark terminal-initial-light
                      terminal-initial-mono terminal-menu terminal-full-screen
                      terminal-nested terminal-sixel terminal-no-graphics)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${CKVISION_SOURCE_DIR}/docs/generated/screenshots/${name}.svg"
                "${CKVISION_ARTIFACT_DIR}/${name}.svg"
        RESULT_VARIABLE compare_result)
    if(NOT compare_result EQUAL 0)
        message(FATAL_ERROR
            "${name}.svg is stale; regenerate it with tools/docgen/generate_docs.sh")
    endif()
endforeach()
