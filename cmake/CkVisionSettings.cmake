# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Central strict build settings for ckVision (the engineering standard: warnings are
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
# Clang's -Wshadow-all is a superset of -Wshadow that also catches a lambda
# capture or parameter shadowing an uncaptured outer local — the class GCC's
# plain -Wshadow reports and Apple Clang's stays silent about, which is how
# thirteen such sites reached a public CI runner before any local build
# complained (WP-22, 2026-08-20). Probed, not assumed: GCC has no such
# spelling, and a flag the compiler does not know is a guard that protects
# nothing while looking like one.
check_cxx_compiler_flag(-Wshadow-all CKVISION_HAS_WSHADOW_ALL)

function(ckvision_strict target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
        # MSVC deprecates `std::getenv` and `std::fopen` and `/WX` turns the
        # notice into an error. Both are standard C++ used correctly here, and
        # the deprecation is Microsoft's opinion about the C runtime rather
        # than a defect in the call: the sanctioned replacements are
        # `_dupenv_s`, which ALLOCATES and so hands every call site a free()
        # that `getenv`'s static buffer never needed, and `fopen_s`, which
        # changes the return convention. Trading a vendor opinion for five
        # hand-written lifetimes is how a warning becomes a leak.
        #
        # Checked rather than assumed before switching it off: ckVision calls
        # none of the genuinely dangerous CRT functions this family also
        # covers — no strcpy, strcat, sprintf, gets, scanf, strtok, ctime or
        # tmpnam anywhere in src/ or include/. The five sites are `std::getenv`
        # (graphics_log x2, posix_terminal, cktest.hpp) and `std::fopen`
        # (graphics_log, posix_terminal). **If one of those functions is ever
        # added, this hides it** -- that is the cost, and it is the thing to
        # re-examine rather than this comment.
        #
        # The MACRO rather than `/wd4996`: C4996 is also what MSVC emits for a
        # user `[[deprecated]]` attribute, so disabling the number would
        # silence ckVision's own deprecation markers along with the CRT's.
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Werror)
        if(CKVISION_HAS_WSHADOW_ALL)
            # See the probe above: the superset shadow class fails here, on
            # the developer's machine, instead of rounds later on a runner.
            # It belongs in THIS arm and not the MSVC one -- a Clang warning
            # flag in the MSVC branch is applied to a compiler that does not
            # take it, which is a guard that never fires while reading as one.
            target_compile_options(${target} PRIVATE -Wshadow-all)
        endif()
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
        # PUBLIC, not PRIVATE: a sanitizer is a USAGE requirement of the
        # artifact, not merely a way of building it. libcvision.a carries
        # __asan_*/__ubsan_* references, so anything linking it must compile
        # and link with the same sanitizer or the link fails on hundreds of
        # undefined symbols. PRIVATE keeps the flags out of
        # INTERFACE_COMPILE_OPTIONS/INTERFACE_LINK_OPTIONS and therefore out of
        # the installed ckvisionTargets.cmake — which is exactly how
        # install_package_smoke's generated consumer came to be built without
        # them against an instrumented archive, and why that gate failed on the
        # sanitizer lanes only.
        if(MSVC)
            if(NOT CKVISION_SANITIZE STREQUAL "address")
                message(FATAL_ERROR "MSVC supports only CKVISION_SANITIZE=address "
                                    "(got '${CKVISION_SANITIZE}')")
            endif()
            target_compile_options(${target} PUBLIC /fsanitize=address)
        else()
            # -fno-sanitize-recover=all: findings must abort, not scroll by
            # as green-looking log noise.
            target_compile_options(${target} PUBLIC
                -fsanitize=${CKVISION_SANITIZE}
                -fno-sanitize-recover=all
                -fno-omit-frame-pointer)
            target_link_options(${target} PUBLIC
                -fsanitize=${CKVISION_SANITIZE}
                -fno-sanitize-recover=all)
        endif()
    endif()
endfunction()
