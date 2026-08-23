# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
#
# The interactive gallery binary only builds where a real terminal
# backend exists (POSIX today; the Windows/ConPTY backend is M7 scope
# per the roadmap) — but the object graph it wires up (gallery_app.cpp)
# is plain library code, so it is always built as a static library and
# always exercised headlessly by tests/test_gallery_smoke.cpp even on
# a platform without an interactive backend yet.
# Every example executable and test links ckvision::cvision explicitly. Keep
# the object-graph edge private so static link lines contain the framework
# exactly once (and do not trigger duplicate-library linker warnings).
add_library(gallery_app STATIC gallery/gallery_app.cpp)
target_link_libraries(gallery_app PRIVATE ckvision::cvision)
target_include_directories(gallery_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/gallery)
ckvision_strict(gallery_app)

# CKVISION_HAS_POSIX_TERMINAL is a preprocessor definition on the
# cvision target, not visible here as a CMake variable — UNIX is the
# same condition CMakeLists.txt itself uses to add the POSIX backend.
if(UNIX)
    add_executable(ckvision_gallery gallery/main.cpp)
    target_link_libraries(ckvision_gallery PRIVATE gallery_app ckvision::cvision)
    ckvision_strict(ckvision_gallery)
    install(TARGETS ckvision_gallery RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# File Browser: a master-detail example (TreeView drives a ListView) —
# same split as gallery_app above: the object graph is a plain static
# library, always built and always headlessly testable; only the
# interactive binary (which needs PosixFileSystem to browse the real
# disk) is POSIX-only.
add_library(filebrowser_app STATIC filebrowser/filebrowser_app.cpp)
target_link_libraries(filebrowser_app PRIVATE ckvision::cvision)
target_include_directories(filebrowser_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/filebrowser)
ckvision_strict(filebrowser_app)

if(UNIX)
    add_executable(ckvision_filebrowser filebrowser/main.cpp)
    target_link_libraries(ckvision_filebrowser PRIVATE filebrowser_app ckvision::cvision)
    ckvision_strict(ckvision_filebrowser)
    install(TARGETS ckvision_filebrowser RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# ckVision Hello: a compact, ckVision-owned visual integration example,
# pinned by tests/test_hello_golden.cpp and documented in docs/hello-example.md.
add_library(hello_app STATIC hello/hello_app.cpp)
target_link_libraries(hello_app PRIVATE ckvision::cvision)
target_include_directories(hello_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/hello)
ckvision_strict(hello_app)

if(UNIX)
    add_executable(ckvision_hello hello/main.cpp)
    target_link_libraries(ckvision_hello PRIVATE hello_app ckvision::cvision)
    ckvision_strict(ckvision_hello)
    install(TARGETS ckvision_hello RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# WP-36A comprehensive client-facing examples.  Each app follows the same
# split as the established examples above: reusable object graph library for
# headless tests plus a thin terminal main where a backend exists.
add_library(layouts_app STATIC layouts/layouts_app.cpp)
target_link_libraries(layouts_app PRIVATE ckvision::cvision)
target_include_directories(layouts_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/layouts)
ckvision_strict(layouts_app)

if(UNIX)
    add_executable(ckvision_layouts layouts/main.cpp)
    target_link_libraries(ckvision_layouts PRIVATE layouts_app ckvision::cvision)
    ckvision_strict(ckvision_layouts)
    install(TARGETS ckvision_layouts RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

add_library(forms_app STATIC forms/forms_app.cpp)
target_link_libraries(forms_app PRIVATE ckvision::cvision)
target_include_directories(forms_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/forms)
ckvision_strict(forms_app)

if(UNIX)
    add_executable(ckvision_forms forms/main.cpp)
    target_link_libraries(ckvision_forms PRIVATE forms_app ckvision::cvision)
    ckvision_strict(ckvision_forms)
    install(TARGETS ckvision_forms RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

add_library(workbench_app STATIC workbench/workbench_app.cpp)
target_link_libraries(workbench_app PRIVATE ckvision::cvision)
target_include_directories(workbench_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/workbench)
ckvision_strict(workbench_app)

if(UNIX)
    add_executable(ckvision_workbench workbench/main.cpp)
    target_link_libraries(ckvision_workbench PRIVATE workbench_app ckvision::cvision)
    ckvision_strict(ckvision_workbench)
    install(TARGETS ckvision_workbench RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

add_library(graphics_app STATIC graphics/graphics_app.cpp)
target_link_libraries(graphics_app PRIVATE ckvision::cvision)
target_include_directories(graphics_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/graphics)
ckvision_strict(graphics_app)

if(UNIX)
    add_executable(ckvision_graphics graphics/main.cpp)
    target_link_libraries(ckvision_graphics PRIVATE graphics_app ckvision::cvision)
    ckvision_strict(ckvision_graphics)
    install(TARGETS ckvision_graphics RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

add_library(editor_app STATIC editor/editor_app.cpp)
target_link_libraries(editor_app PRIVATE ckvision::cvision)
target_include_directories(editor_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/editor)
ckvision_strict(editor_app)

# A non-interactive, public-API-only profile extension sample. It deliberately
# needs neither an Application nor a terminal backend, so documentation and CI
# can prove the registry extension seam on every build host.
add_executable(ckvision_editor_profile_sample editor/profile_sample.cpp)
target_link_libraries(ckvision_editor_profile_sample PRIVATE ckvision::cvision)
ckvision_strict(ckvision_editor_profile_sample)
install(TARGETS ckvision_editor_profile_sample RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(UNIX)
    add_executable(ckvision_editor editor/main.cpp)
    target_link_libraries(ckvision_editor PRIVATE editor_app ckvision::cvision)
    ckvision_strict(ckvision_editor)
    install(TARGETS ckvision_editor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Embedded terminal proof point (WP-49).  The object graph remains headless
# testable; the runnable host is POSIX until the ConPTY adapter is accepted.
add_library(terminal_app STATIC terminal/terminal_app.cpp)
target_link_libraries(terminal_app PRIVATE ckvision::cvision)
target_include_directories(terminal_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/terminal)
ckvision_strict(terminal_app)

if(UNIX)
    add_executable(ckvision_terminal terminal/main.cpp)
    target_link_libraries(ckvision_terminal PRIVATE terminal_app ckvision::cvision)
    ckvision_strict(ckvision_terminal)
    install(TARGETS ckvision_terminal RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Spin: animated raster content produced off the owning thread. The model
# and the software renderer are ordinary library code with no platform
# dependency at all; the object graph adds worker threads, which is why
# this is the one example target that links Threads.
add_library(spin_app STATIC
    spin/mesh.cpp
    spin/renderer.cpp
    spin/render_service.cpp
    spin/spin_app.cpp)
target_link_libraries(spin_app PRIVATE ckvision::cvision)
find_package(Threads REQUIRED)
target_link_libraries(spin_app PUBLIC Threads::Threads)
target_include_directories(spin_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/spin)
ckvision_strict(spin_app)

if(UNIX)
    add_executable(ckvision_spin spin/main.cpp)
    target_link_libraries(ckvision_spin PRIVATE spin_app ckvision::cvision)
    ckvision_strict(ckvision_spin)
    install(TARGETS ckvision_spin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# SysInfo: what this machine is and how fast it is, read through a single
# injected probe (the internal plans). Same split as every example
# above, with one addition: the POSIX probe is a source of the otherwise
# portable object-graph library rather than of the interactive binary, so
# a host with no probe implementation still builds and tests everything
# except the reading of that host.
add_library(sysinfo_app STATIC
    sysinfo/bar_chart_view.cpp
    sysinfo/benchmark.cpp
    sysinfo/benchmark_service.cpp
    sysinfo/build_report.cpp
    sysinfo/fixed_benchmark_runner.cpp
    sysinfo/fixed_system_probe.cpp
    sysinfo/latency_plot.cpp
    sysinfo/reference_points.cpp
    sysinfo/report_format.cpp
    sysinfo/sysinfo_app.cpp)
target_link_libraries(sysinfo_app PRIVATE ckvision::cvision)
# The benchmark service runs its measurements on a worker thread; this is
# the second example target (after spin) that needs one.
find_package(Threads REQUIRED)
target_link_libraries(sysinfo_app PUBLIC Threads::Threads)
target_include_directories(sysinfo_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sysinfo)
ckvision_strict(sysinfo_app)
# The one fact about this binary that the compiler does not know, and that a
# program reporting benchmark scores must not omit: an unoptimized build
# measures itself rather than the machine.
target_compile_definitions(sysinfo_app PRIVATE CKV_SYSINFO_BUILD_TYPE="$<CONFIG>")

if(UNIX)
    target_sources(sysinfo_app PRIVATE sysinfo/posix_system_probe.cpp)
    if(APPLE)
        # The battery is the one figure macOS does not put behind sysctl.
        # IOKit is the platform's own answer to that question and ships with
        # the system, so this is a platform API like sysctl and termios --
        # not a dependency in the sense the engineering standard forbids -- and it is
        # confined to the one target that reads the host.
        target_link_libraries(sysinfo_app PRIVATE "-framework IOKit" "-framework CoreFoundation")
    endif()

    add_executable(ckvision_sysinfo sysinfo/main.cpp)
    target_link_libraries(ckvision_sysinfo PRIVATE sysinfo_app ckvision::cvision)
    ckvision_strict(ckvision_sysinfo)
    install(TARGETS ckvision_sysinfo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
