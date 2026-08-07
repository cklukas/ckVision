# Copyright (c) 2026 C. Klukas. All rights reserved.
add_executable(cvision_tests
    test_application.cpp
    test_box_drawing.cpp
    test_clock_diagnostics_error.cpp
    test_color_style_cell.cpp
    test_command.cpp
    test_compositor.cpp
    test_desktop.cpp
    test_dialog.cpp
    test_frame_view.cpp
    test_geometry.cpp
    test_golden_capture.cpp
    test_golden_format.cpp
    test_golden_hostile_content.cpp
    test_headless_terminal.cpp
    test_history.cpp
    test_image.cpp
    test_input_decoder.cpp
    test_key_event.cpp
    test_layout.cpp
    test_list_view.cpp
    test_m4_integration.cpp
    test_menu.cpp
    test_memo.cpp
    test_message_box.cpp
    test_mnemonic.cpp
    test_option_group.cpp
    test_painter.cpp
    test_posix_terminal.cpp
    test_presenter.cpp
    test_record_replay_terminal.cpp
    test_rect_ops.cpp
    test_scroll_viewport.cpp
    test_scrollbar.cpp
    test_sixel_encoder.cpp
    test_standard_roles.cpp
    test_status_line.cpp
    test_surface.cpp
    test_text_view.cpp
    test_text_width.cpp
    test_theme.cpp
    test_utf8.cpp
    test_version.cpp
    test_view.cpp
    test_widgets.cpp
    test_window.cpp
    main.cpp)
target_link_libraries(cvision_tests PRIVATE ckvision::cvision)
ckvision_strict(cvision_tests)

add_test(NAME cvision_tests
    COMMAND cvision_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Python is build/CI tooling, not a library dependency (D-004 untouched).
# REQUIRED so the layer/include enforcement gate can never vanish silently.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME layer_check
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_layers.py)
