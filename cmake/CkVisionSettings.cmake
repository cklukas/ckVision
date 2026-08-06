# Copyright (c) 2026 C. Klukas. All rights reserved.
#
# Central strict build settings for ckVision (AGENTS.md: warnings are
# errors; one place defines the flag set).

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CKVISION_SANITIZE "" CACHE STRING
    "Comma-separated sanitizer list applied to all ckVision targets, e.g. address,undefined")

function(ckvision_strict target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Werror)
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
