# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
#
# libFuzzer targets are opt-in: ordinary developers and release builds do not
# need a compiler runtime that supplies libFuzzer. The CI corpus lane enables
# this directory explicitly with CKVISION_BUILD_FUZZERS=ON.
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
        "CKVISION_BUILD_FUZZERS requires an LLVM Clang toolchain with the libFuzzer runtime; "
        "AppleClang is insufficient - use the Ubuntu Clang CI lane or an installed LLVM toolchain")
endif()

set(CKVISION_FUZZ_SANITIZERS fuzzer)
if(CKVISION_SANITIZE)
    string(REPLACE "," ";" CKVISION_FUZZ_EXTRA_SANITIZERS "${CKVISION_SANITIZE}")
    list(APPEND CKVISION_FUZZ_SANITIZERS ${CKVISION_FUZZ_EXTRA_SANITIZERS})
endif()
list(JOIN CKVISION_FUZZ_SANITIZERS "," CKVISION_FUZZ_SANITIZER_FLAGS)

function(ckvision_add_fuzzer name source corpus)
    add_executable(${name} ${source} ${ARGN})
    target_link_libraries(${name} PRIVATE ckvision::cvision)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    ckvision_strict(${name})
    target_compile_options(${name} PRIVATE -fsanitize=${CKVISION_FUZZ_SANITIZER_FLAGS})
    target_link_options(${name} PRIVATE -fsanitize=${CKVISION_FUZZ_SANITIZER_FLAGS})

    if(CKVISION_BUILD_TESTING)
        file(GLOB CKVISION_FUZZ_${name}_CORPUS CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${corpus}/*")
        add_test(NAME ${name}_corpus
            COMMAND ${name} ${CKVISION_FUZZ_${name}_CORPUS}
                    -runs=1 -max_len=4096 -timeout=2 -rss_limit_mb=512)
        set_tests_properties(${name}_corpus PROPERTIES
            LABELS "fuzz-corpus;portable"
            TIMEOUT 45)
    endif()
endfunction()

ckvision_add_fuzzer(fuzz_input_decoder fuzz_input_decoder.cpp input_decoder)
ckvision_add_fuzzer(fuzz_text fuzz_text.cpp text)
ckvision_add_fuzzer(fuzz_golden fuzz_golden.cpp golden)
ckvision_add_fuzzer(fuzz_osc fuzz_osc.cpp osc)
ckvision_add_fuzzer(fuzz_virtual_display fuzz_virtual_display.cpp virtual_display)
ckvision_add_fuzzer(fuzz_terminal_emulator fuzz_terminal_emulator.cpp terminal_emulator)
ckvision_add_fuzzer(fuzz_editor_document fuzz_editor_document.cpp editor_document)
ckvision_add_fuzzer(fuzz_syntax_profile fuzz_syntax_profile.cpp syntax_profile)
ckvision_add_fuzzer(fuzz_todo_codec fuzz_todo_codec.cpp todo_codec
    ../examples/todo/todo_model.cpp
    ../examples/todo/todo_codec.cpp)
target_include_directories(fuzz_todo_codec PRIVATE ${PROJECT_SOURCE_DIR}/examples/todo)
