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

# The Classic palette table embedded in the internal plans; the
# classic_look gate compares the embedded table with this program's output.
add_executable(generate_classic_palette_table generate_classic_palette_table.cpp)
target_link_libraries(generate_classic_palette_table PRIVATE ckvision::cvision)
ckvision_strict(generate_classic_palette_table)

add_executable(generate_root_resize_goldens generate_root_resize_goldens.cpp)
target_link_libraries(generate_root_resize_goldens PRIVATE ckvision::cvision)
ckvision_strict(generate_root_resize_goldens)

add_executable(generate_window_activation_goldens generate_window_activation_goldens.cpp)
target_link_libraries(generate_window_activation_goldens PRIVATE ckvision::cvision)
ckvision_strict(generate_window_activation_goldens)

add_executable(generate_desktop_dialog_goldens generate_desktop_dialog_goldens.cpp)
target_link_libraries(generate_desktop_dialog_goldens PRIVATE ckvision::cvision)
ckvision_strict(generate_desktop_dialog_goldens)

add_executable(generate_shrink_storm_goldens generate_shrink_storm_goldens.cpp)
target_link_libraries(generate_shrink_storm_goldens PRIVATE ckvision::cvision)
ckvision_strict(generate_shrink_storm_goldens)

add_executable(generate_window_resize_goldens generate_window_resize_goldens.cpp)
target_link_libraries(generate_window_resize_goldens PRIVATE ckvision::cvision)
ckvision_strict(generate_window_resize_goldens)

add_executable(generate_window_z_order_golden generate_window_z_order_golden.cpp)
target_link_libraries(generate_window_z_order_golden PRIVATE ckvision::cvision)
ckvision_strict(generate_window_z_order_golden)

# The widget width sweeps (WP-30): each text-drawing family clipped at every
# width. The generator pins them; cvision_tests links the same library to
# compare the pinned files and check that no cell holds a partial cluster.
add_library(docgen_clip_sweep STATIC clip_sweep.cpp)
target_link_libraries(docgen_clip_sweep PUBLIC ckvision::cvision)
target_include_directories(docgen_clip_sweep PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
ckvision_strict(docgen_clip_sweep)

add_executable(generate_clip_sweep_goldens generate_clip_sweep_goldens.cpp)
target_link_libraries(generate_clip_sweep_goldens PRIVATE docgen_clip_sweep ckvision::cvision)
ckvision_strict(generate_clip_sweep_goldens)

# The hostile-content goldens: a recovered paste and hostile display text.
add_executable(generate_hostile_content_goldens generate_hostile_content_goldens.cpp)
target_link_libraries(generate_hostile_content_goldens PRIVATE ckvision::cvision)
ckvision_strict(generate_hostile_content_goldens)

# Per-window theme override goldens under each application scheme.
add_executable(generate_theme_override_goldens generate_theme_override_goldens.cpp)
target_link_libraries(generate_theme_override_goldens PRIVATE ckvision::cvision)
ckvision_strict(generate_theme_override_goldens)

# The appearance matrix (WP-38): every public element × state × scheme,
# public API only, so it needs no example application.
add_library(docgen_appearance_specimens STATIC
    appearance_stage.cpp
    appearance_controls.cpp
    appearance_data.cpp
    appearance_chrome.cpp
    appearance_components.cpp
    appearance_media.cpp)
target_link_libraries(docgen_appearance_specimens PUBLIC ckvision::cvision)
ckvision_strict(docgen_appearance_specimens)
add_executable(generate_appearance_matrix generate_appearance_matrix.cpp)
target_link_libraries(generate_appearance_matrix PRIVATE docgen_appearance_specimens docgen_frame_svg ckvision::cvision)
add_executable(capture_widget_presentations capture_widget_presentations.cpp)
target_link_libraries(capture_widget_presentations PRIVATE docgen_appearance_specimens docgen_frame_svg ckvision::cvision)
ckvision_strict(capture_widget_presentations)
ckvision_strict(generate_appearance_matrix)

# What every script-driven golden needs, whatever it plays: the event-script
# player (event_script.hpp), the captures of the frame a beat ends on -- its
# symbolic scene and the display HeadlessTerminal decoded (plane_capture.hpp)
# -- and the oracles a test asks of a decoded pixel plane (raster_checks.hpp).
# Private edges only, like every example object graph; a consumer names
# ckvision::cvision itself.
add_library(docgen_script_support STATIC event_script.cpp plane_capture.cpp raster_checks.cpp)
target_link_libraries(docgen_script_support PRIVATE ckvision::cvision)
target_include_directories(docgen_script_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
ckvision_strict(docgen_script_support)

add_executable(generate_terminal_plane_goldens generate_terminal_plane_goldens.cpp)
target_link_libraries(generate_terminal_plane_goldens PRIVATE
    docgen_script_support gallery_app terminal_app ckvision::cvision)
ckvision_strict(generate_terminal_plane_goldens)

# A scrolled picture re-anchored (D-081): an ImageView in a ScrollViewport
# and a picture inline in a FlowView, public API only. The generator and
# tests/test_raster_scroll_golden.cpp play the same script.
add_library(docgen_raster_scroll STATIC raster_scroll_script.cpp)
target_link_libraries(docgen_raster_scroll PRIVATE docgen_script_support ckvision::cvision)
target_include_directories(docgen_raster_scroll PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
ckvision_strict(docgen_raster_scroll)

add_executable(generate_raster_scroll_goldens generate_raster_scroll_goldens.cpp)
target_link_libraries(generate_raster_scroll_goldens PRIVATE
    docgen_raster_scroll docgen_script_support ckvision::cvision)
ckvision_strict(generate_raster_scroll_goldens)

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

# The per-widget figure set: one focused cut-out per public view, from
# scenes that are also the gallery's compiled usage examples. Standalone
# like capture_widget_gallery_screenshots — it drives public API only and
# needs no example application.
add_executable(capture_widget_shots
    capture_widget_shots.cpp
    widget_stage.cpp
    widget_shots_controls.cpp
    widget_shots_text.cpp
    widget_shots_data.cpp
    widget_shots_chrome.cpp
    widget_shots_composite.cpp)
target_link_libraries(capture_widget_shots PRIVATE docgen_frame_svg ckvision::cvision)
target_include_directories(capture_widget_shots PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
ckvision_strict(capture_widget_shots)

add_executable(capture_widget_gallery_screenshots capture_widget_gallery_screenshots.cpp)
target_link_libraries(capture_widget_gallery_screenshots PRIVATE docgen_frame_svg ckvision::cvision)
target_include_directories(capture_widget_gallery_screenshots PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
ckvision_strict(capture_widget_gallery_screenshots)

if(TARGET gallery_app)
    # The Gallery's picture and scheme scripts, played by
    # generate_gallery_goldens, by tests/test_gallery_visual_golden.cpp and,
    # for the documentation's figures, by capture_gallery_screenshots.
    add_library(docgen_gallery_scripts STATIC gallery_script.cpp)
    target_link_libraries(docgen_gallery_scripts PRIVATE docgen_script_support gallery_app ckvision::cvision)
    target_include_directories(docgen_gallery_scripts PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<TARGET_PROPERTY:gallery_app,INTERFACE_INCLUDE_DIRECTORIES>)
    ckvision_strict(docgen_gallery_scripts)

    add_executable(generate_gallery_goldens generate_gallery_goldens.cpp)
    target_link_libraries(generate_gallery_goldens PRIVATE
        docgen_gallery_scripts docgen_script_support gallery_app ckvision::cvision)
    ckvision_strict(generate_gallery_goldens)

    add_executable(capture_gallery_screenshots capture_gallery_screenshots.cpp)
    target_link_libraries(capture_gallery_screenshots PRIVATE
        docgen_frame_svg docgen_gallery_scripts docgen_script_support gallery_app ckvision::cvision)
    target_include_directories(capture_gallery_screenshots PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/gallery)
    ckvision_strict(capture_gallery_screenshots)
endif()

# The M10 container-suite and File Browser goldens, each driven through its
# example's real object graph and compared byte for byte by the
# generated_golden_bytes test.
if(TARGET layouts_app)
    add_executable(generate_layouts_goldens generate_layouts_goldens.cpp)
    target_link_libraries(generate_layouts_goldens PRIVATE layouts_app ckvision::cvision)
    ckvision_strict(generate_layouts_goldens)
endif()

if(TARGET filebrowser_app)
    add_executable(generate_filebrowser_goldens generate_filebrowser_goldens.cpp)
    target_link_libraries(generate_filebrowser_goldens PRIVATE filebrowser_app ckvision::cvision)
    ckvision_strict(generate_filebrowser_goldens)

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

# Event scripts (tools/docgen/event_script.hpp): named beats of terminal
# input played through HeadlessTerminal and Application::step. The golden
# generator below and the tests that own the scripts link the same library,
# so a pinned frame is always the frame of the script its test plays.
if(TARGET rootdialog_app)
    add_library(docgen_event_scripts STATIC
        rootdialog_script.cpp
        close_veto_script.cpp
        hyperlink_script.cpp
        menu_operability_script.cpp)
    # Private edges only, like every example object graph, and a consumer
    # names rootdialog_app itself after this library (as the generator below
    # and cvision_tests do): the framework then appears exactly once on its
    # link line. The scripts' headers include the example's, so its include
    # directory is public.
    target_link_libraries(docgen_event_scripts PRIVATE docgen_script_support rootdialog_app ckvision::cvision)
    target_include_directories(docgen_event_scripts PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<TARGET_PROPERTY:rootdialog_app,INTERFACE_INCLUDE_DIRECTORIES>)
    ckvision_strict(docgen_event_scripts)

    add_executable(generate_event_script_goldens generate_event_script_goldens.cpp)
    target_link_libraries(generate_event_script_goldens PRIVATE
        docgen_event_scripts docgen_script_support rootdialog_app ckvision::cvision)
    ckvision_strict(generate_event_script_goldens)
endif()

# The Echo example's raw-byte input script, shared by its smoke test and the
# generator of its pinned frames.
if(TARGET echo_app)
    add_library(docgen_echo_script STATIC echo_script.cpp)
    target_link_libraries(docgen_echo_script PRIVATE echo_app ckvision::cvision)
    target_include_directories(docgen_echo_script PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<TARGET_PROPERTY:echo_app,INTERFACE_INCLUDE_DIRECTORIES>)
    ckvision_strict(docgen_echo_script)

    add_executable(generate_echo_goldens generate_echo_goldens.cpp)
    target_link_libraries(generate_echo_goldens PRIVATE docgen_echo_script echo_app ckvision::cvision)
    ckvision_strict(generate_echo_goldens)
endif()

if(TARGET editor_app)
    # The Editor example's menu scripts, by keyboard alone and by mouse alone,
    # played by tests/test_editor_menu_scripts.cpp and by
    # generate_editor_goldens, which writes the frames they pin.
    add_library(docgen_editor_scripts STATIC editor_menu_script.cpp)
    target_link_libraries(docgen_editor_scripts PRIVATE docgen_script_support editor_app ckvision::cvision)
    target_include_directories(docgen_editor_scripts PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<TARGET_PROPERTY:editor_app,INTERFACE_INCLUDE_DIRECTORIES>)
    ckvision_strict(docgen_editor_scripts)

    add_executable(generate_editor_goldens generate_editor_goldens.cpp)
    target_link_libraries(generate_editor_goldens PRIVATE
        docgen_editor_scripts docgen_script_support editor_app ckvision::cvision)
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

if(UNIX AND TARGET terminal_app AND TARGET gallery_app)
    add_executable(capture_terminal_screenshots capture_terminal_screenshots.cpp)
    target_link_libraries(capture_terminal_screenshots PRIVATE docgen_frame_svg terminal_app gallery_app ckvision::cvision)
    target_include_directories(capture_terminal_screenshots PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/terminal
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/gallery)
    ckvision_strict(capture_terminal_screenshots)
endif()

# The remaining example applications are captured through exactly the same
# public Application + HeadlessTerminal path as their interactive binaries.
# This keeps every client-facing guide visually grounded in the actual app
# object graph rather than in illustrative mockups.
if(TARGET forms_app)
    add_executable(generate_forms_goldens generate_forms_goldens.cpp)
    target_link_libraries(generate_forms_goldens PRIVATE forms_app ckvision::cvision)
    target_include_directories(generate_forms_goldens PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/forms)
    ckvision_strict(generate_forms_goldens)
endif()

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
