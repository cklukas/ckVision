# Copyright (c) 2026 C. Klukas. All rights reserved.
cmake_minimum_required(VERSION 3.25)
project(ckvision VERSION 0.1.0 LANGUAGES CXX)

include(cmake/CkVisionSettings.cmake)

option(CKVISION_BUILD_FUZZERS
    "Build libFuzzer targets and bounded corpus tests (Clang only)" OFF)
if(CKVISION_BUILD_FUZZERS AND MSVC)
    message(FATAL_ERROR "CKVISION_BUILD_FUZZERS requires a Clang-family compiler")
endif()

add_library(cvision
    src/core/assert.cpp
    src/core/cell.cpp
    src/core/filesystem.cpp
    src/core/golden.cpp
    src/core/key.cpp
    src/core/text.cpp
    src/core/utf8.cpp
    src/core/version.cpp
    src/scene/box_drawing.cpp
    src/scene/compositor.cpp
    src/scene/golden_capture.cpp
    src/scene/painter.cpp
    src/scene/rect_ops.cpp
    src/scene/surface.cpp
    src/term/headless_terminal.cpp
    src/term/input_decoder.cpp
    src/term/presenter.cpp
    src/term/sixel_encoder.cpp
    src/term/virtual_display.cpp
    src/ui/anchor_pane.cpp
    src/ui/application.cpp
    src/ui/command.cpp
    src/ui/dock.cpp
    src/ui/grid.cpp
    src/ui/history.cpp
    src/ui/layout.cpp
    src/ui/overlay.cpp
    src/ui/standard_roles.cpp
    src/ui/theme.cpp
    src/ui/view.cpp
    src/widgets/button.cpp
    src/widgets/canvas.cpp
    src/widgets/desktop.cpp
    src/widgets/dialog.cpp
    src/widgets/directory_picker.cpp
    src/widgets/file_dialog.cpp
    src/widgets/help_viewer.cpp
    src/widgets/image_view.cpp
    src/widgets/input_line.cpp
    src/widgets/label.cpp
    src/widgets/list_view.cpp
    src/widgets/menu.cpp
    src/widgets/memo.cpp
    src/widgets/message_box.cpp
    src/widgets/mnemonic.cpp
    src/widgets/option_group.cpp
    src/widgets/scroll_viewport.cpp
    src/widgets/scrollbar.cpp
    src/widgets/splitter.cpp
    src/widgets/static_text.cpp
    src/widgets/status_line.cpp
    src/widgets/table.cpp
    src/widgets/text_view.cpp
    src/widgets/tree_view.cpp
    src/widgets/window.cpp
    src/widgets/window_list_dialog.cpp)
add_library(ckvision::cvision ALIAS cvision)
target_include_directories(cvision PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
ckvision_strict(cvision)
if(CKVISION_BUILD_FUZZERS)
    target_compile_options(cvision PRIVATE -fsanitize=fuzzer-no-link)
endif()

# The POSIX backend (termios/signal-based) only compiles on UNIX-like
# systems; the Windows (ConPTY) backend is M7 scope (ROADMAP.md).
if(UNIX)
    target_sources(cvision PRIVATE src/term/posix_terminal.cpp src/term/posix_clock.cpp
        src/term/posix_filesystem.cpp)
    target_compile_definitions(cvision PUBLIC CKVISION_HAS_POSIX_TERMINAL=1)
endif()

option(CKVISION_BUILD_EXAMPLES
    "Build ckVision example applications" ${PROJECT_IS_TOP_LEVEL})
if(CKVISION_BUILD_EXAMPLES)
    # Ahead of tests/: test_gallery_smoke.cpp links the gallery_app
    # target and needs it to already exist.
    add_subdirectory(examples)
    add_subdirectory(tools/docgen)
endif()

option(CKVISION_BUILD_TESTING
    "Build ckVision tests and benchmarks" ${PROJECT_IS_TOP_LEVEL})
if(CKVISION_BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
    add_subdirectory(benchmarks)
endif()

if(CKVISION_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()
