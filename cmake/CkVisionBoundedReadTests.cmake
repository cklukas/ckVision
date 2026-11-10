# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
include_guard(GLOBAL)

function(ckvision_add_bounded_read_tests)
    if(WIN32)
        set(CKVISION_TEST_WINDOWS_UNC_ROOT "" CACHE STRING
            "Existing UNC mirror of CKVISION_TEST_TEMP_ROOT for the six-profile bounded reader qualification")
        set(fixture_args "${CKVISION_TEST_TEMP_ROOT}/bounded-reader-controls")
        if(NOT CKVISION_TEST_WINDOWS_UNC_ROOT STREQUAL "")
            string(SUBSTRING "${CKVISION_TEST_WINDOWS_UNC_ROOT}" 0 2 unc_prefix)
            if(NOT unc_prefix STREQUAL "//" AND NOT unc_prefix STREQUAL "\\\\")
                message(FATAL_ERROR "CKVISION_TEST_WINDOWS_UNC_ROOT must be an explicit UNC address")
            endif()
            list(APPEND fixture_args "${CKVISION_TEST_WINDOWS_UNC_ROOT}/bounded-reader-controls")
        endif()
        add_library(windows_bounded_read_provider STATIC
            "${PROJECT_SOURCE_DIR}/src/core/filesystem.cpp"
            "${PROJECT_SOURCE_DIR}/src/term/windows_filesystem.cpp"
            "${PROJECT_SOURCE_DIR}/src/term/windows_text.cpp")
        target_include_directories(windows_bounded_read_provider
            PUBLIC "${PROJECT_SOURCE_DIR}/include"
            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_compile_features(windows_bounded_read_provider PUBLIC cxx_std_20)
        target_compile_definitions(windows_bounded_read_provider PRIVATE CKV_WINDOWS_BOUNDED_READ_TEST_HOOKS=1)
        target_link_libraries(windows_bounded_read_provider PUBLIC bcrypt advapi32)
        ckvision_strict(windows_bounded_read_provider)

        add_executable(windows_bounded_read_contract windows_bounded_read_contract.cpp
            installed_bounded_read_windows/longpath.manifest)
        target_link_libraries(windows_bounded_read_contract PRIVATE windows_bounded_read_provider)
        target_link_options(windows_bounded_read_contract PRIVATE
            "$<$<CXX_COMPILER_ID:MSVC>:/ENTRY:wmainCRTStartup>"
            "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-municode>")
        ckvision_strict(windows_bounded_read_contract)
        add_test(NAME windows_bounded_read_native_controls COMMAND windows_bounded_read_contract ${fixture_args})
        set_tests_properties(windows_bounded_read_native_controls PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE
            LABELS "windows;filesystem;bounded;native-faults;allocation")
    elseif(UNIX)
        add_library(posix_bounded_read_provider STATIC
            "${PROJECT_SOURCE_DIR}/src/core/filesystem.cpp"
            "${PROJECT_SOURCE_DIR}/src/term/posix_filesystem.cpp")
        target_include_directories(posix_bounded_read_provider PUBLIC "${PROJECT_SOURCE_DIR}/include")
        target_compile_features(posix_bounded_read_provider PUBLIC cxx_std_20)
        target_compile_definitions(posix_bounded_read_provider PRIVATE CKV_POSIX_BOUNDED_READ_TEST_HOOKS=1)
        ckvision_strict(posix_bounded_read_provider)

        add_executable(posix_bounded_read_contract posix_bounded_read_contract.cpp)
        target_link_libraries(posix_bounded_read_contract PRIVATE posix_bounded_read_provider)
        ckvision_strict(posix_bounded_read_contract)
        add_test(NAME posix_bounded_read_native_controls COMMAND posix_bounded_read_contract "${CKVISION_TEST_TEMP_ROOT}")
        set_tests_properties(posix_bounded_read_native_controls PROPERTIES TIMEOUT 30
            LABELS "posix;filesystem;bounded;native-race")
    endif()
endfunction()
