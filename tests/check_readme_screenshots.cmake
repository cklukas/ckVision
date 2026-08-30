# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT

foreach(required IN ITEMS SOURCE_DIR ARTIFACT_DIR GALLERY_CAPTURE EDITOR_CAPTURE SYSINFO_CAPTURE TODO_CAPTURE)
    if(NOT DEFINED CKVISION_${required})
        message(FATAL_ERROR "readme screenshot check requires CKVISION_${required}")
    endif()
endforeach()

file(REMOVE_RECURSE "${CKVISION_ARTIFACT_DIR}")
file(MAKE_DIRECTORY "${CKVISION_ARTIFACT_DIR}")

foreach(capture IN ITEMS GALLERY_CAPTURE EDITOR_CAPTURE SYSINFO_CAPTURE TODO_CAPTURE)
    execute_process(
        COMMAND "${CKVISION_${capture}}" "${CKVISION_ARTIFACT_DIR}"
        RESULT_VARIABLE capture_result)
    if(NOT capture_result EQUAL 0)
        message(FATAL_ERROR "${capture} failed while refreshing README evidence")
    endif()
endforeach()

foreach(name IN ITEMS gallery-initial todo-guided editor-search sysinfo-benchmarks)
    set(checked_in "${CKVISION_SOURCE_DIR}/docs/generated/screenshots/${name}.svg")
    set(generated "${CKVISION_ARTIFACT_DIR}/${name}.svg")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${checked_in}" "${generated}"
        RESULT_VARIABLE compare_result)
    if(NOT compare_result EQUAL 0)
        message(FATAL_ERROR
            "README screenshot ${name}.svg is stale; regenerate it with tools/docgen/generate_docs.sh")
    endif()
endforeach()
