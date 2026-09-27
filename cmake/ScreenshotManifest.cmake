# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Reads tools/docgen/screenshot-manifest.txt: `[capture_<tool>]` opens a group,
# and each name below it is a figure that tool writes. Shared by the CTest
# registration in tests/CMakeLists.txt and by tests/check_screenshots.cmake, so
# the test that exists for a tool and the figures it checks come from one
# reading of one file.

# Sets `${out_tools}` to the capture tools in manifest order and, for each,
# `${out_tools}_<tool>` to its figure names. A name outside any group, or a
# group with no names, is a malformed manifest and stops configuration.
function(ckvision_read_screenshot_manifest manifest out_tools)
    file(STRINGS "${manifest}" lines)
    set(tools "")
    set(current "")
    foreach(line IN LISTS lines)
        string(STRIP "${line}" line)
        if(line STREQUAL "" OR line MATCHES "^#")
            continue()
        endif()
        if(line MATCHES "^\\[(capture_[a-z_]+)\\]$")
            set(current "${CMAKE_MATCH_1}")
            if(current IN_LIST tools)
                message(FATAL_ERROR "${manifest}: [${current}] appears twice")
            endif()
            list(APPEND tools "${current}")
            set(names_${current} "")
            continue()
        endif()
        if(NOT line MATCHES "^[a-z0-9-]+$")
            message(FATAL_ERROR "${manifest}: '${line}' is neither a [capture_<tool>] group nor a figure name")
        endif()
        if(current STREQUAL "")
            message(FATAL_ERROR "${manifest}: '${line}' is not under a [capture_<tool>] group")
        endif()
        list(APPEND names_${current} "${line}")
    endforeach()
    foreach(tool IN LISTS tools)
        list(LENGTH names_${tool} count)
        if(count EQUAL 0)
            message(FATAL_ERROR "${manifest}: [${tool}] names no figure")
        endif()
        set(${out_tools}_${tool} "${names_${tool}}" PARENT_SCOPE)
    endforeach()
    set(${out_tools} "${tools}" PARENT_SCOPE)
endfunction()
