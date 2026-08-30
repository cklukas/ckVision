# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT

foreach(required IN ITEMS CKVISION_SOURCE_DIR CKVISION_ARTIFACT_DIR CKVISION_WIDGET_CAPTURE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${CKVISION_ARTIFACT_DIR}")
file(MAKE_DIRECTORY "${CKVISION_ARTIFACT_DIR}")
execute_process(
    COMMAND "${CKVISION_WIDGET_CAPTURE}" "${CKVISION_ARTIFACT_DIR}"
    RESULT_VARIABLE capture_result
    OUTPUT_VARIABLE capture_output
    ERROR_VARIABLE capture_error)
if(NOT capture_result EQUAL 0)
    message(FATAL_ERROR
        "widget capture failed (${capture_result})\n${capture_output}${capture_error}")
endif()

file(STRINGS "${CKVISION_SOURCE_DIR}/tools/docgen/screenshot-manifest.txt" screenshot_names)
set(widget_count 0)
foreach(name IN LISTS screenshot_names)
    # The historical navigation overview has the same prefix but is emitted by
    # capture_widget_gallery_screenshots, whose own test runs immediately above
    # this one. Every other widget-* name belongs to capture_widget_shots.
    if(NOT name MATCHES "^widget-" OR name STREQUAL "widget-navigation")
        continue()
    endif()
    math(EXPR widget_count "${widget_count} + 1")
    set(actual "${CKVISION_ARTIFACT_DIR}/${name}.svg")
    set(expected "${CKVISION_SOURCE_DIR}/docs/generated/screenshots/${name}.svg")
    if(NOT EXISTS "${actual}")
        message(FATAL_ERROR "widget capture did not produce ${name}.svg")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${actual}" "${expected}"
        RESULT_VARIABLE compare_result)
    if(NOT compare_result EQUAL 0)
        message(FATAL_ERROR
            "${name}.svg is stale; regenerate with tools/docgen/generate_docs.sh")
    endif()
endforeach()

if(widget_count EQUAL 0)
    message(FATAL_ERROR "screenshot manifest contains no widget-* figures")
endif()

message(STATUS "validated ${widget_count} current widget screenshots")
