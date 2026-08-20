# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Central strict build settings for ckVision (AGENTS.md: warnings are
# errors; one place defines the flag set).

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CKVISION_SANITIZE "" CACHE STRING
    "Comma-separated sanitizer list applied to all ckVision targets, e.g. address,undefined")

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wmissing-designated-field-initializers
                        CKVISION_HAS_WMISSING_DESIGNATED)

function(ckvision_strict target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Werror)
        if(CKVISION_HAS_WMISSING_DESIGNATED)
            # Newer LLVM Clang puts this under -Wextra; AppleClang does not
            # have it at all. Switched off on purpose: descriptor structs
            # (CommandDescriptor, FieldDescriptor, …) default every field so a
            # call site names only what differs — that is the API's design, and
            # a warning that forces every caller to enumerate every field would
            # dismantle it one -Werror build at a time.
            target_compile_options(${target} PRIVATE
                -Wno-missing-designated-field-initializers)
        else()
            # And the same decision in the older spelling, which is the half
            # that was missing. A Clang without the designated-specific flag
            # folds the identical diagnostic into -Wmissing-field-initializers,
            # which -Wextra above turns on — so the suppression was silently
            # absent on exactly the compilers that emit the warning, and
            # present on the ones that do not. GitHub's Clang found it the
            # first time ckVision was built anywhere but this machine:
            # `command.cpp:31: missing field 'context'`.
            #
            # Deliberately narrowed to that case rather than switched off
            # everywhere: where the specific flag exists it stays the one in
            # use, so ordinary aggregate initialisation keeps its warning on
            # modern compilers.
            target_compile_options(${target} PRIVATE
                -Wno-missing-field-initializers)
        endif()
    endif()
    if(CKVISION_SANITIZE)
        if(MSVC)
            if(NOT CKVISION_SANITIZE STREQUAL "address")
                message(FATAL_ERROR "MSVC supports only CKVISION_SANITIZE=address "
                                    "(got '${CKVISION_SANITIZE}')")
            endif()
            target_compile_options(${target} PRIVATE /fsanitize=address)
        else()
            # -fno-sanitize-recover=all: findings must abort, not scroll by
            # as green-looking log noise.
            target_compile_options(${target} PRIVATE
                -fsanitize=${CKVISION_SANITIZE}
                -fno-sanitize-recover=all
                -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE
                -fsanitize=${CKVISION_SANITIZE}
                -fno-sanitize-recover=all)
        endif()
    endif()
endfunction()
