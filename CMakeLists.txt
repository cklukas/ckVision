# Copyright (c) 2026 C. Klukas. All rights reserved.
cmake_minimum_required(VERSION 3.25)
project(ckvision VERSION 0.1.0 LANGUAGES CXX)

include(cmake/CkVisionSettings.cmake)

add_library(cvision
    src/core/assert.cpp
    src/core/cell.cpp
    src/core/golden.cpp
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
    src/ui/application.cpp
    src/ui/command.cpp
    src/ui/history.cpp
    src/ui/layout.cpp
    src/ui/standard_roles.cpp
    src/ui/theme.cpp
    src/ui/view.cpp
    src/widgets/button.cpp
    src/widgets/desktop.cpp
    src/widgets/dialog.cpp
    src/widgets/input_line.cpp
    src/widgets/label.cpp
    src/widgets/menu.cpp
    src/widgets/message_box.cpp
    src/widgets/mnemonic.cpp
    src/widgets/static_text.cpp
    src/widgets/status_line.cpp
    src/widgets/window.cpp)
add_library(ckvision::cvision ALIAS cvision)
target_include_directories(cvision PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
ckvision_strict(cvision)

# The POSIX backend (termios/signal-based) only compiles on UNIX-like
# systems; the Windows (ConPTY) backend is M7 scope (ROADMAP.md).
if(UNIX)
    target_sources(cvision PRIVATE src/term/posix_terminal.cpp)
    target_compile_definitions(cvision PUBLIC CKVISION_HAS_POSIX_TERMINAL=1)
endif()

option(CKVISION_BUILD_TESTING
    "Build ckVision tests and benchmarks" ${PROJECT_IS_TOP_LEVEL})
if(CKVISION_BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
    add_subdirectory(benchmarks)
endif()
