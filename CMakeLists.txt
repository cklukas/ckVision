# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.25)
project(ckvision VERSION 0.1.4 LANGUAGES CXX)

include(cmake/CkVisionSettings.cmake)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CheckCXXSourceCompiles)
find_package(Threads REQUIRED)

option(CKVISION_BUILD_FUZZERS
    "Build libFuzzer targets and bounded corpus tests (Clang only)" OFF)
if(CKVISION_BUILD_FUZZERS AND MSVC)
    message(FATAL_ERROR "CKVISION_BUILD_FUZZERS requires a Clang-family compiler")
endif()

add_library(cvision
    src/core/assert.cpp
    src/core/base64.cpp
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
    src/term/capability_report.cpp
    src/term/presenter.cpp
    src/term/graphics_log.cpp
    src/term/sixel_decoder.cpp
    src/term/sixel_encoder.cpp
    src/term/terminal_emulator.cpp
    src/term/terminal_subsession.cpp
    src/term/virtual_display.cpp
    src/ui/anchor_pane.cpp
    src/ui/animation.cpp
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
    src/widgets/application_shell.cpp
    src/widgets/button.cpp
    src/widgets/canvas.cpp
    src/widgets/combo_box.cpp
    src/widgets/common_components.cpp
    src/widgets/desktop.cpp
    src/widgets/dialog.cpp
    src/widgets/directory_picker.cpp
    src/widgets/editor_document.cpp
    src/widgets/editor_search.cpp
    src/widgets/editor_window.cpp
    src/widgets/file_editor_controller.cpp
    src/widgets/file_dialog.cpp
    src/widgets/flow_view.cpp
    src/widgets/help_viewer.cpp
    src/widgets/image_view.cpp
    src/widgets/input_line.cpp
    src/widgets/key_chord_capture.cpp
    src/widgets/label.cpp
    src/widgets/list_view.cpp
    src/widgets/menu.cpp
    src/widgets/memo.cpp
    src/widgets/message_box.cpp
    src/widgets/minimized_window_stub.cpp
    src/widgets/mnemonic.cpp
    src/widgets/option_group.cpp
    src/widgets/paged_strip.cpp
    src/widgets/popup_list.cpp
    src/widgets/progress.cpp
    src/widgets/scroll_viewport.cpp
    src/widgets/scrollbar.cpp
    src/widgets/splitter.cpp
    src/widgets/standard_strings.cpp
    src/widgets/static_text.cpp
    src/widgets/status_line.cpp
    src/widgets/syntax_cache.cpp
    src/widgets/syntax_profile.cpp
    src/widgets/table.cpp
    src/widgets/tab_control.cpp
    src/widgets/terminal_report_dialog.cpp
    src/widgets/terminal_scrollbar.cpp
    src/widgets/terminal_view.cpp
    src/widgets/text_editor.cpp
    src/widgets/text_layout.cpp
    src/widgets/text_view.cpp
    src/widgets/tree_view.cpp
    src/widgets/window.cpp
    src/widgets/window_list_dialog.cpp
    src/widgets/window_switcher_bar.cpp)
add_library(ckvision::cvision ALIAS cvision)
target_include_directories(cvision PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(cvision PUBLIC Threads::Threads)
# The public headers use C++20 library types.  Export the requirement with the
# package target so an independent consumer is configured correctly instead
# of depending on this project's directory-wide build setting.
target_compile_features(cvision PUBLIC cxx_std_20)
# Floating-point <charconv> arrived later than the integer overloads in some
# otherwise C++20-capable standard libraries (notably libstdc++ 10, shipped by
# Ubuntu 20.04). Detect the actual overloads instead of guessing from compiler
# or library version macros; table.cpp supplies the locale-independent fallback.
check_cxx_source_compiles([[#include <charconv>
int main() {
    char buffer[64]{};
    double value = 1.25;
    const auto written = std::to_chars(buffer, buffer + sizeof(buffer), value);
    const auto read = std::from_chars(buffer, written.ptr, value);
    return read.ec == std::errc{} ? 0 : 1;
}
]] CKVISION_HAS_FLOAT_CHARCONV)
if(CKVISION_HAS_FLOAT_CHARCONV)
    target_compile_definitions(cvision PRIVATE CKVISION_HAS_FLOAT_CHARCONV=1)
endif()
# One version, stated once. `project()` above is where ckVision's version
# lives: it is what `write_basic_package_version_file` writes into
# ckvisionConfigVersion.cmake, and therefore what `find_package(ckvision 0.1.0)`
# matches against. The C++ API used to repeat the same three numbers in
# src/core/version.cpp, so a release could ship a library reporting one version
# to a consumer that had found it by another — and nothing would have said so,
# because the only test compared the repeated numbers to each other.
#
# PRIVATE: the definitions are for compiling this library, not a promise to
# anything that links it. A consumer asks `ckv::version()`, which is what these
# make correct.
target_compile_definitions(cvision PRIVATE
    CKVISION_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    CKVISION_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    CKVISION_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    CKVISION_VERSION_STRING="${PROJECT_VERSION}")
ckvision_strict(cvision)
# GCC 13 raises -Wmaybe-uninitialized from INSIDE <bits/shared_ptr_base.h>
# against `std::optional<Application::ViewHandle>`, and -Werror makes it fatal.
# It is a false positive, and the code was audited rather than assumed before
# switching it off:
#
#   * `ViewHandle` default-initialises both members (`View* = nullptr` and a
#     default-constructed `weak_ptr`), so no uninitialised state is even
#     constructible.
#   * every dereference of the two optionals is guarded by an engagement check
#     -- all nine uses read, none unguarded -- and every handle in existence
#     comes from `make_view_handle(View&)`, so an ENGAGED optional always holds
#     a non-null view.
#   * it fires only in the optimised lane. The Debug and sanitiser lanes are
#     clean, which is the signature of GCC's post-inlining predicate analysis
#     losing track of the guard, not of a reachable read.
#   * Clang's own -Wconditional-uninitialized, -Wsometimes-uninitialized and
#     -Wuninitialized on an optimised build of the same file: zero.
#
# Deliberately narrowed to ONE FILE and ONE COMPILER rather than dropped into
# `ckvision_strict`: -Wmaybe-uninitialized catches real defects, and this
# leaves it live everywhere else. The alternative -- restructuring correct code
# to satisfy a warning nobody here can reproduce -- would be changing working
# code on speculation, with no way to confirm the change did anything.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(src/ui/application.cpp
        PROPERTIES COMPILE_OPTIONS -Wno-maybe-uninitialized)
    # The same GCC-13 post-inlining analysis, relocated to
    # <bits/basic_string.h> (4 sites in this file). The audit here is a TYPE
    # argument, stronger than application.cpp's site enumeration: a
    # std::string cannot be uninitialised — every declaration runs a
    # constructor — and this file holds zero std::optional<std::string>,
    # zero unions and zero reinterpret_casts to string, the only constructs
    # that could put a string object over never-constructed storage.
    # Optimised lane only; Debug, the sanitiser lanes and Clang's
    # uninitialised-warning family are all clean. Audited by ckmux-33.
    # Precondition for the file-scoped property (directory scoping):
    # posix_terminal.cpp is compiled into exactly one target, cvision.
    set_source_files_properties(src/term/posix_terminal.cpp
        PROPERTIES COMPILE_OPTIONS -Wno-maybe-uninitialized)
endif()
if(CKVISION_BUILD_FUZZERS)
    target_compile_options(cvision PRIVATE -fsanitize=fuzzer-no-link)
endif()

# The POSIX backend (termios/signal-based) only compiles on UNIX-like
# systems; the Windows (ConPTY) backend is M7 scope (the roadmap).
if(UNIX)
    target_sources(cvision PRIVATE src/term/posix_terminal.cpp src/term/posix_terminal_subsession.cpp src/term/posix_clock.cpp
        src/term/posix_filesystem.cpp)
    target_compile_definitions(cvision PUBLIC CKVISION_HAS_POSIX_TERMINAL=1)
    if(NOT APPLE)
        target_link_libraries(cvision PRIVATE util)
    endif()
endif()

option(CKVISION_BUILD_EXAMPLES
    "Build ckVision example applications" ${PROJECT_IS_TOP_LEVEL})
if(CKVISION_BUILD_EXAMPLES)
    # Ahead of tests/: test_gallery_smoke.cpp links the gallery_app
    # target and needs it to already exist.
    add_subdirectory(examples)
    add_subdirectory(tools/docgen)
endif()

# A field diagnostic: what this terminal actually reports and sends, as
# ckVision's own layer decodes it. Built with the library because the
# question it answers ("why does the mouse behave differently here?")
# arises on hosts that build neither examples nor tests.
if(UNIX)
    add_executable(ckvision_terminal_probe tools/diag/terminal_probe.cpp)
    target_link_libraries(ckvision_terminal_probe PRIVATE cvision)
    ckvision_strict(ckvision_terminal_probe)

    # The same question narrowed to pictures: can this terminal show one,
    # and if it can, what would make it refuse a particular one. A window
    # showing a cell fallback cannot tell those two apart on its own.
    add_executable(ckvision_graphics_check tools/diag/graphics_check.cpp)
    target_link_libraries(ckvision_graphics_check PRIVATE cvision)
    ckvision_strict(ckvision_graphics_check)
endif()

# Half of the VT conformance harness (tools/vtconform/README.md): it prints
# what a byte script made of the embedded emulator's screen, so the same
# script's effect on an established terminal can be compared against it.
# Built with the library because a fidelity question can arise on any host.
add_executable(ckvision_vt_dump tools/vtconform/dump_emulator.cpp)
target_link_libraries(ckvision_vt_dump PRIVATE cvision)
ckvision_strict(ckvision_vt_dump)

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

# The public package is deliberately small: the framework library and headers
# are always installed; runnable examples are installed only when the caller
# elected to build them.  This makes the documented find_package contract work
# for both library consumers and a local client-handoff build.
install(TARGETS cvision
    EXPORT ckvisionTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/cvision DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT ckvisionTargets
    FILE ckvisionTargets.cmake
    NAMESPACE ckvision::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ckvision)
configure_package_config_file(
    cmake/ckvisionConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ckvisionConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ckvision)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ckvisionConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ckvisionConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ckvisionConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ckvision)
