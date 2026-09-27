# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
#
# The freshness gate of one capture tool's published screenshots. Runs the
# tool into an empty artifact directory and requires it to write exactly the
# figures its group in tools/docgen/screenshot-manifest.txt names, each
# byte-identical to docs/generated/screenshots/<name>.svg. A figure that
# changed, one the tool stopped writing, and one it writes that nobody
# publishes all fail, so a UI change cannot leave the documentation showing an
# older ckVision.
#
#   cmake -DCKVISION_SOURCE_DIR=<repo> -DCKVISION_ARTIFACT_DIR=<dir>
#         -DCKVISION_CAPTURE_TOOL=<capture_name> -DCKVISION_CAPTURE=<executable>
#         -P check_screenshots.cmake
cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SOURCE_DIR ARTIFACT_DIR CAPTURE_TOOL CAPTURE)
    if(NOT DEFINED CKVISION_${required})
        message(FATAL_ERROR "the screenshot check requires CKVISION_${required}")
    endif()
endforeach()

include("${CKVISION_SOURCE_DIR}/cmake/ScreenshotManifest.cmake")
ckvision_read_screenshot_manifest("${CKVISION_SOURCE_DIR}/tools/docgen/screenshot-manifest.txt" manifest)
if(NOT CKVISION_CAPTURE_TOOL IN_LIST manifest)
    message(FATAL_ERROR "the screenshot manifest has no [${CKVISION_CAPTURE_TOOL}] group")
endif()
set(expected "${manifest_${CKVISION_CAPTURE_TOOL}}")

file(REMOVE_RECURSE "${CKVISION_ARTIFACT_DIR}")
file(MAKE_DIRECTORY "${CKVISION_ARTIFACT_DIR}")
execute_process(
    COMMAND "${CKVISION_CAPTURE}" "${CKVISION_ARTIFACT_DIR}"
    RESULT_VARIABLE capture_result
    OUTPUT_VARIABLE capture_output
    ERROR_VARIABLE capture_error)
if(NOT capture_result EQUAL 0)
    message(FATAL_ERROR
        "${CKVISION_CAPTURE_TOOL} failed (${capture_result})\n${capture_output}${capture_error}")
endif()

file(GLOB written RELATIVE "${CKVISION_ARTIFACT_DIR}" "${CKVISION_ARTIFACT_DIR}/*")
set(failures "")
foreach(file IN LISTS written)
    string(REGEX REPLACE "\\.svg$" "" name "${file}")
    if(NOT file MATCHES "\\.svg$" OR NOT name IN_LIST expected)
        string(APPEND failures "\n  ${file} is written but not listed under [${CKVISION_CAPTURE_TOOL}]")
    endif()
endforeach()
foreach(name IN LISTS expected)
    set(generated "${CKVISION_ARTIFACT_DIR}/${name}.svg")
    set(published "${CKVISION_SOURCE_DIR}/docs/generated/screenshots/${name}.svg")
    if(NOT EXISTS "${generated}")
        string(APPEND failures "\n  ${name}.svg is listed but was not written")
        continue()
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${generated}" "${published}"
        RESULT_VARIABLE compare_result)
    if(NOT compare_result EQUAL 0)
        string(APPEND failures "\n  ${name}.svg is stale")
    endif()
endforeach()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "${CKVISION_CAPTURE_TOOL}:${failures}\n"
                        "regenerate with tools/docgen/generate_docs.sh and review the SVG diffs")
endif()

list(LENGTH expected count)
message(STATUS "${CKVISION_CAPTURE_TOOL}: ${count} published screenshots current")
