# Copyright (c) 2026 C. Klukas. All rights reserved.
add_executable(cvision_tests
    test_anchor_pane.cpp
    test_application.cpp
    test_application_render.cpp
    test_box_drawing.cpp
    test_canvas.cpp
    test_clock_diagnostics_error.cpp
    test_color_style_cell.cpp
    test_command.cpp
    test_compositor.cpp
    test_filesystem.cpp
    test_desktop.cpp
    test_desktop_dialog_golden.cpp
    test_dialog.cpp
    test_directory_picker.cpp
    test_dock.cpp
    test_file_dialog.cpp
    test_frame_view.cpp
    test_geometry.cpp
    test_golden_capture.cpp
    test_golden_format.cpp
    test_golden_hostile_content.cpp
    test_grid.cpp
    test_headless_terminal.cpp
    test_help_viewer.cpp
    test_history.cpp
    test_image.cpp
    test_image_view.cpp
    test_input_decoder.cpp
    test_key_chord.cpp
    test_key_event.cpp
    test_layout.cpp
    test_list_view.cpp
    test_m4_integration.cpp
    test_m8_integration.cpp
    test_menu.cpp
    test_memo.cpp
    test_message_box.cpp
    test_mnemonic.cpp
    test_option_group.cpp
    test_overlay.cpp
    test_painter.cpp
    test_posix_terminal.cpp
    test_presenter.cpp
    test_record_replay_terminal.cpp
    test_rect_ops.cpp
    test_root_resize_golden.cpp
    test_scroll_viewport.cpp
    test_scrollbar.cpp
    test_sixel_encoder.cpp
    test_splitter.cpp
    test_standard_roles.cpp
    test_status_line.cpp
    test_surface.cpp
    test_table.cpp
    test_text_view.cpp
    test_text_width.cpp
    test_theme.cpp
    test_tree_view.cpp
    test_utf8.cpp
    test_version.cpp
    test_virtual_display.cpp
    test_view.cpp
    test_window_activation_golden.cpp
    test_widgets.cpp
    test_window.cpp
    test_window_list_dialog.cpp
    main.cpp)
target_link_libraries(cvision_tests PRIVATE ckvision::cvision)
find_package(Threads REQUIRED)
target_link_libraries(cvision_tests PRIVATE Threads::Threads)
ckvision_strict(cvision_tests)

if(UNIX)
    target_sources(cvision_tests PRIVATE test_posix_filesystem.cpp)
endif()

# gallery_app only exists when CKVISION_BUILD_EXAMPLES is on (default,
# but a build config could disable it) — the gallery smoke test is
# additive, not required for the rest of the suite to be meaningful.
if(TARGET gallery_app)
    target_sources(cvision_tests PRIVATE test_gallery_smoke.cpp)
    target_link_libraries(cvision_tests PRIVATE gallery_app)
endif()

if(TARGET filebrowser_app)
    target_sources(cvision_tests PRIVATE test_filebrowser_smoke.cpp)
    target_link_libraries(cvision_tests PRIVATE filebrowser_app)
endif()

if(TARGET hello_app)
    target_sources(cvision_tests PRIVATE test_hello_golden.cpp)
    target_link_libraries(cvision_tests PRIVATE hello_app)
endif()

if(TARGET docgen_frame_svg)
    target_sources(cvision_tests PRIVATE test_frame_svg.cpp)
    target_link_libraries(cvision_tests PRIVATE docgen_frame_svg)
    target_include_directories(cvision_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen)
endif()

add_test(NAME cvision_tests
    COMMAND cvision_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Keep a human-inspectable rendering artifact beside the machine assertions.
# This drives Presenter output through HeadlessTerminal's VT/Sixel decoder;
# capture code never receives the source Image directly (D-035).
if(TARGET capture_gallery_screenshots)
    add_test(NAME gallery_visual_capture
        COMMAND capture_gallery_screenshots
                ${CMAKE_BINARY_DIR}/test-artifacts/gallery)
    set_tests_properties(gallery_visual_capture PROPERTIES LABELS "visual;artifact")
endif()

if(TARGET capture_filebrowser_screenshots)
    add_test(NAME filebrowser_visual_capture
        COMMAND capture_filebrowser_screenshots
                ${CMAKE_BINARY_DIR}/test-artifacts/filebrowser)
    set_tests_properties(filebrowser_visual_capture PROPERTIES LABELS "visual;artifact")
endif()

if(TARGET capture_hello_screenshots)
    add_test(NAME hello_visual_capture
        COMMAND capture_hello_screenshots
                ${CMAKE_BINARY_DIR}/test-artifacts/hello)
    set_tests_properties(hello_visual_capture PROPERTIES LABELS "visual;artifact")
endif()

# Python is build/CI tooling, not a library dependency (D-004 untouched).
# REQUIRED so the layer/include enforcement gate can never vanish silently.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME layer_check
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_layers.py)

# The M9 60-line Hello exit criterion (ROADMAP.md), wired for real:
# never vanish silently, same reasoning as layer_check above.
add_test(NAME hello_line_budget
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_hello_line_budget.py)

# D-012/WP-23: scan every tracked and non-ignored text artifact for
# prohibited source-fidelity claims, and separately prove that the checker
# rejects representative positive cases while accepting clean-room wording.
add_test(NAME provenance_check
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_provenance.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME provenance_checker_self_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_provenance.py
            --self-test)
