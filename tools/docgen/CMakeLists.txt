# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Documentation tooling — not part of the cvision library, not
# installed, not part of ckVision's own public API. Only built when
# the gallery example (whose object graph it screenshots) is also
# built.
add_executable(generate_standard_commands_table generate_standard_commands_table.cpp)
target_link_libraries(generate_standard_commands_table PRIVATE ckvision::cvision)
ckvision_strict(generate_standard_commands_table)

add_executable(generate_root_resize_goldens generate_root_resize_goldens.cpp)
target_link_libraries(generate_root_resize_goldens PRIVATE ckvision::cvision)
ckvision_strict(generate_root_resize_goldens)

add_executable(generate_window_activation_goldens generate_window_activation_goldens.cpp)
target_link_libraries(generate_window_activation_goldens PRIVATE ckvision::cvision)
ckvision_strict(generate_window_activation_goldens)

add_executable(generate_desktop_dialog_goldens generate_desktop_dialog_goldens.cpp)
target_link_libraries(generate_desktop_dialog_goldens PRIVATE ckvision::cvision)
ckvision_strict(generate_desktop_dialog_goldens)

add_executable(generate_window_z_order_golden generate_window_z_order_golden.cpp)
target_link_libraries(generate_window_z_order_golden PRIVATE ckvision::cvision)
ckvision_strict(generate_window_z_order_golden)

if(NOT TARGET docgen_frame_svg)
    add_library(docgen_frame_svg STATIC frame_svg.cpp)
    target_link_libraries(docgen_frame_svg PRIVATE ckvision::cvision)
    ckvision_strict(docgen_frame_svg)
    # GCC 13/14 raises -Wfree-nonheap-object from inside libstdc++ against the
    # std::vector churn in the PNG/zlib packer under -O2 (`operator delete …
    # with nonzero offset`). It is a known optimiser false positive on
    # move-into-by-value-vector code, optimised-lane only, and this is
    # docgen tooling rather than the shipped library. Narrowed to this file and
    # this compiler, exactly like the -Wmaybe-uninitialized suppressions the
    # library carries for the same class of GCC post-inlining analysis.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_source_files_properties(frame_svg.cpp
            PROPERTIES COMPILE_OPTIONS -Wno-free-nonheap-object)
    endif()
endif()

add_executable(capture_widget_gallery_screenshots capture_widget_gallery_screenshots.cpp)
target_link_libraries(capture_widget_gallery_screenshots PRIVATE docgen_frame_svg ckvision::cvision)
target_include_directories(capture_widget_gallery_screenshots PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
ckvision_strict(capture_widget_gallery_screenshots)

if(TARGET gallery_app)
    add_executable(capture_gallery_screenshots capture_gallery_screenshots.cpp)
    target_link_libraries(capture_gallery_screenshots PRIVATE docgen_frame_svg gallery_app ckvision::cvision)
    target_include_directories(capture_gallery_screenshots PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/gallery)
    ckvision_strict(capture_gallery_screenshots)
endif()

if(TARGET filebrowser_app)
    if(NOT TARGET docgen_frame_svg)
        add_library(docgen_frame_svg STATIC frame_svg.cpp)
        target_link_libraries(docgen_frame_svg PRIVATE ckvision::cvision)
        ckvision_strict(docgen_frame_svg)
    endif()

    add_executable(capture_filebrowser_screenshots capture_filebrowser_screenshots.cpp)
    target_link_libraries(capture_filebrowser_screenshots PRIVATE docgen_frame_svg filebrowser_app ckvision::cvision)
    target_include_directories(capture_filebrowser_screenshots PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/filebrowser)
    ckvision_strict(capture_filebrowser_screenshots)
endif()

if(TARGET hello_app)
    add_executable(generate_hello_golden generate_hello_golden.cpp)
    target_link_libraries(generate_hello_golden PRIVATE hello_app ckvision::cvision)
    target_include_directories(generate_hello_golden PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/hello)
    ckvision_strict(generate_hello_golden)

    if(NOT TARGET docgen_frame_svg)
        add_library(docgen_frame_svg STATIC frame_svg.cpp)
        target_link_libraries(docgen_frame_svg PRIVATE ckvision::cvision)
        ckvision_strict(docgen_frame_svg)
    endif()

    add_executable(capture_hello_screenshots capture_hello_screenshots.cpp)
    target_link_libraries(capture_hello_screenshots PRIVATE docgen_frame_svg hello_app ckvision::cvision)
    target_include_directories(capture_hello_screenshots PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/hello)
    ckvision_strict(capture_hello_screenshots)
endif()

if(TARGET editor_app)
    add_executable(generate_editor_goldens generate_editor_goldens.cpp)
    target_link_libraries(generate_editor_goldens PRIVATE editor_app ckvision::cvision)
    target_include_directories(generate_editor_goldens PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/editor)
    ckvision_strict(generate_editor_goldens)

    add_executable(capture_editor_screenshots capture_editor_screenshots.cpp)
    target_link_libraries(capture_editor_screenshots PRIVATE docgen_frame_svg editor_app ckvision::cvision)
    target_include_directories(capture_editor_screenshots PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/editor)
    ckvision_strict(capture_editor_screenshots)
endif()

if(UNIX AND TARGET terminal_app)
    add_executable(capture_terminal_screenshots capture_terminal_screenshots.cpp)
    target_link_libraries(capture_terminal_screenshots PRIVATE docgen_frame_svg terminal_app ckvision::cvision)
    target_include_directories(capture_terminal_screenshots PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/terminal)
    ckvision_strict(capture_terminal_screenshots)
endif()

# The remaining example applications are captured through exactly the same
# public Application + HeadlessTerminal path as their interactive binaries.
# This keeps every client-facing guide visually grounded in the actual app
# object graph rather than in illustrative mockups.
foreach(example_name IN ITEMS layouts forms workbench graphics spin sysinfo)
    if(TARGET ${example_name}_app)
        if(NOT TARGET docgen_frame_svg)
            add_library(docgen_frame_svg STATIC frame_svg.cpp)
            target_link_libraries(docgen_frame_svg PRIVATE ckvision::cvision)
            ckvision_strict(docgen_frame_svg)
        endif()

        add_executable(capture_${example_name}_screenshots capture_${example_name}_screenshots.cpp)
        target_link_libraries(capture_${example_name}_screenshots PRIVATE
            docgen_frame_svg ${example_name}_app ckvision::cvision)
        target_include_directories(capture_${example_name}_screenshots PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/${example_name})
        ckvision_strict(capture_${example_name}_screenshots)
    endif()
endforeach()

if(TARGET todo_app)
    add_executable(capture_todo_screenshots capture_todo_screenshots.cpp)
    target_link_libraries(capture_todo_screenshots PRIVATE
        docgen_frame_svg todo_app ckvision::cvision)
    target_include_directories(capture_todo_screenshots PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/todo)
    ckvision_strict(capture_todo_screenshots)
endif()
