# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
set(CKVISION_PORTABLE_TEST_SOURCES
    test_activation_focus_scripts.cpp
    test_anchor_pane.cpp
    test_animation.cpp
    test_application.cpp
    test_application_render.cpp
    test_ascii.cpp
    test_base64.cpp
    test_big_clock.cpp
    test_box_drawing.cpp
    test_canvas.cpp
    test_cell_grid.cpp
    test_clock_diagnostics.cpp
    test_color_style_cell.cpp
    test_command.cpp
    test_command_palette.cpp
    test_common_components.cpp
    test_compositor.cpp
    test_combo_box.cpp
    test_file_trace_sink.cpp
    test_filesystem.cpp
    test_focus.cpp
    test_frame_text.cpp
    test_date_time_dialog.cpp
    test_desktop.cpp
    test_disabled_state.cpp
    test_desktop_dialog_golden.cpp
    test_dialog.cpp
    test_dialog_presentation.cpp
    test_directory_picker.cpp
    test_dock.cpp
    test_editor_document.cpp
    test_editor_search.cpp
    test_editor_window.cpp
    test_file_dialog.cpp
    test_file_editor_controller.cpp
    test_flow_view.cpp
    test_frame_view.cpp
    test_geometry.cpp
    test_golden_capture.cpp
    test_golden_format.cpp
    test_golden_hostile_content.cpp
    test_grid.cpp
    test_headless_terminal.cpp
    test_help_viewer.cpp
    test_history.cpp
    test_history_sharing.cpp
    test_hyperlink.cpp
    test_image.cpp
    test_image_view.cpp
    test_input_decoder.cpp
    test_key_chord.cpp
    test_key_chord_capture.cpp
    test_key_event.cpp
    test_layout.cpp
    test_list_view.cpp
    test_m4_integration.cpp
    test_m8_integration.cpp
    test_menu.cpp
    test_memo.cpp
    test_message_box.cpp
    test_minimized_window_stub.cpp
    test_mnemonic.cpp
    test_option_group.cpp
    test_osc_emission.cpp
    test_paged_strip.cpp
    test_popup_list.cpp
    test_property_inspector.cpp
    test_overlay.cpp
    test_painter.cpp
    test_paste_security.cpp
    test_platform_services.cpp
    test_pointer_shape.cpp
    test_presenter.cpp
    test_presenter_hyperlinks.cpp
    test_progress.cpp
    test_record_replay_terminal.cpp
    test_rect_ops.cpp
    test_root_resize_golden.cpp
    test_scene_hyperlinks.cpp
    test_scroll_viewport.cpp
    test_scrollbar.cpp
    test_sixel_encoder.cpp
    test_splitter.cpp
    test_standard_strings.cpp
    test_standard_roles.cpp
    test_status_line.cpp
    test_syntax_cache.cpp
    test_syntax_profile.cpp
    test_surface.cpp
    test_table.cpp
    test_tab_control.cpp
    test_terminal_emulator.cpp
    test_terminal_cell_golden.cpp
    test_terminal_report_dialog.cpp
    test_terminal_scrollbar.cpp
    test_terminal_view.cpp
    test_text_layout.cpp
    test_text_view.cpp
    test_text_editor.cpp
    test_text_width.cpp
    test_text_width_divergent.cpp
    test_unicode_grapheme_break.cpp
    test_theme.cpp
    test_theme_editor.cpp
    test_theme_format.cpp
    test_theme_override_golden.cpp
    test_tooltip_controller.cpp
    test_tree_view.cpp
    test_utf8.cpp
    test_version.cpp
    test_virtual_display.cpp
    test_virtual_display_visual_golden.cpp
    test_view.cpp
    test_window_activation_golden.cpp
    test_window_resize_golden.cpp
    test_wp35.cpp
    test_wp36.cpp
    test_widgets.cpp
    test_window.cpp
    test_window_list_dialog.cpp
    test_window_move_size.cpp
    test_window_switcher_bar.cpp)

# The allocation-budget suite deliberately replaces the process-wide global
# new/delete family so it can count hot-path allocations. ThreadSanitizer
# supplies the same replacements as part of its allocator interception and
# the two implementations cannot coexist in one executable. Keep the budget
# gate in every compatible configuration, and let the TSan lane concentrate
# on the race instrumentation it is uniquely able to provide.
if(NOT CKVISION_SANITIZE MATCHES "(^|,)thread(,|$)")
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_hot_path_allocations.cpp)
endif()

add_executable(cvision_tests ${CKVISION_PORTABLE_TEST_SOURCES} main.cpp)
# Keep the test translation units' compile interface explicit even when the
# final framework archive arrives transitively through example object graphs.
target_include_directories(cvision_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_compile_definitions(cvision_tests PRIVATE
    CKV_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
if(UNIX)
    target_compile_definitions(cvision_tests PRIVATE CKVISION_HAS_POSIX_TERMINAL=1)
endif()
# Example object graphs carry a private cvision edge for their compile
# interface. When those targets are present, their transitive closure supplies
# the single framework archive; adding a direct edge here would repeat it on
# older CMake/linker combinations. A tests-only build still links the
# framework directly.
if(NOT TARGET gallery_app)
    target_link_libraries(cvision_tests PRIVATE ckvision::cvision)
endif()
find_package(Threads REQUIRED)
target_link_libraries(cvision_tests PRIVATE Threads::Threads)
ckvision_strict(cvision_tests)

add_executable(terminal_redraw_contract terminal_redraw_contract.cpp)
target_link_libraries(terminal_redraw_contract PRIVATE ckvision::cvision)
ckvision_strict(terminal_redraw_contract)
add_test(NAME terminal_redraw_contract COMMAND terminal_redraw_contract)
set_tests_properties(terminal_redraw_contract PROPERTIES LABELS "contract;portable;terminal" TIMEOUT 30)

# The harness's own CK_EXPECT_ABORT contract. Two of these cases are meant to
# fail, so they run in their own program, one case per CTest test, and the
# verdict is read from what cktest prints rather than from the exit code.
add_executable(cktest_expect_abort_contract cktest_expect_abort_contract.cpp)
target_link_libraries(cktest_expect_abort_contract PRIVATE ckvision::cvision)
ckvision_strict(cktest_expect_abort_contract)
add_test(NAME cktest_expect_abort_each_body_that_aborts_passes
    COMMAND cktest_expect_abort_contract --case two_expected_aborts_that_both_abort)
add_test(NAME cktest_expect_abort_a_later_body_that_returns_fails
    COMMAND cktest_expect_abort_contract --case a_second_expected_abort_that_returns)
add_test(NAME cktest_expect_abort_an_earlier_body_that_returns_fails_once
    COMMAND cktest_expect_abort_contract --case a_first_expected_abort_that_returns)
set_tests_properties(cktest_expect_abort_each_body_that_aborts_passes
    cktest_expect_abort_a_later_body_that_returns_fails
    cktest_expect_abort_an_earlier_body_that_returns_fails_once
    PROPERTIES LABELS "contract;portable" TIMEOUT 30)
set_tests_properties(cktest_expect_abort_a_later_body_that_returns_fails
    cktest_expect_abort_an_earlier_body_that_returns_fails_once
    PROPERTIES PASS_REGULAR_EXPRESSION "cktest: 1 failure\\(s\\)")

if(WIN32 OR UNIX)
    add_executable(terminal_nested_child terminal_nested_child.cpp)
    target_link_libraries(terminal_nested_child PRIVATE ckvision::cvision)
    ckvision_strict(terminal_nested_child)
    add_dependencies(cvision_tests terminal_nested_child)
    target_compile_definitions(cvision_tests PRIVATE
        CKV_NESTED_TERMINAL_CHILD_PATH="$<TARGET_FILE:terminal_nested_child>")
endif()

if(WIN32)
    add_executable(windows_terminal_child windows_terminal_child.cpp)
    target_link_libraries(windows_terminal_child PRIVATE cvision)
    ckvision_strict(windows_terminal_child)
    target_sources(cvision_tests PRIVATE test_windows_terminal_subsession.cpp)
    target_compile_definitions(cvision_tests PRIVATE
        CKV_WINDOWS_TERMINAL_CHILD_PATH="$<TARGET_FILE:windows_terminal_child>")
    add_dependencies(cvision_tests windows_terminal_child)
endif()

if(UNIX)
    set(CKVISION_POSIX_TEST_SOURCES
        test_posix_filesystem.cpp
        test_posix_terminal_subsession.cpp
        test_posix_terminal.cpp)
    target_sources(cvision_tests PRIVATE ${CKVISION_POSIX_TEST_SOURCES})

    add_executable(posix_terminal_launch_contract posix_terminal_launch_contract.cpp)
    target_link_libraries(posix_terminal_launch_contract PRIVATE ckvision::cvision)
    ckvision_strict(posix_terminal_launch_contract)
    add_test(NAME posix_terminal_launch_contract COMMAND posix_terminal_launch_contract)
    set_tests_properties(posix_terminal_launch_contract PROPERTIES
        LABELS "contract;platform;posix;terminal"
        TIMEOUT 30)

    # Assertion contracts fork from their own single-threaded CTest process.
    # Forking from the monolithic test binary after its thread-owning cases have
    # run is not sanitizer-safe and can turn the intended SIGABRT into SIGKILL.
    add_executable(theme_assertion_contract test_theme_assertion_contract.cpp)
    target_link_libraries(theme_assertion_contract PRIVATE ckvision::cvision)
    ckvision_strict(theme_assertion_contract)
    add_test(NAME theme_assertion_contract COMMAND theme_assertion_contract)
    set_tests_properties(theme_assertion_contract PROPERTIES
        LABELS "contract;platform;posix"
        TIMEOUT 30)

    add_executable(assertion_diagnostic_contract test_assertion_diagnostic_contract.cpp)
    target_link_libraries(assertion_diagnostic_contract PRIVATE ckvision::cvision)
    ckvision_strict(assertion_diagnostic_contract)
    add_test(NAME assertion_diagnostic_contract COMMAND assertion_diagnostic_contract)
    set_tests_properties(assertion_diagnostic_contract PROPERTIES
        LABELS "contract;platform;posix;terminal"
        TIMEOUT 30)
endif()

# gallery_app only exists when CKVISION_BUILD_EXAMPLES is on (default,
# but a build config could disable it) — the gallery smoke test is
# additive, not required for the rest of the suite to be meaningful.
if(TARGET gallery_app)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES
        test_gallery_smoke.cpp
        test_gallery_visual_golden.cpp)
    target_sources(cvision_tests PRIVATE
        test_gallery_smoke.cpp
        test_gallery_visual_golden.cpp)
    # The Gallery's picture and scheme scripts are shared with the generator
    # that writes their pinned files (generate_gallery_goldens).
    target_link_libraries(cvision_tests PRIVATE docgen_gallery_scripts gallery_app)
endif()

# D-081's scrolled pictures, played by the same script the generator of
# their pinned files plays (generate_raster_scroll_goldens).
if(TARGET docgen_raster_scroll)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_raster_scroll_golden.cpp)
    target_sources(cvision_tests PRIVATE test_raster_scroll_golden.cpp)
    target_link_libraries(cvision_tests PRIVATE docgen_raster_scroll)
endif()

if(TARGET filebrowser_app)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_filebrowser_smoke.cpp)
    target_sources(cvision_tests PRIVATE test_filebrowser_smoke.cpp)
    target_link_libraries(cvision_tests PRIVATE filebrowser_app)
endif()

if(TARGET hello_app)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_hello_golden.cpp)
    target_sources(cvision_tests PRIVATE test_hello_golden.cpp)
    target_link_libraries(cvision_tests PRIVATE hello_app)
endif()

if(TARGET layouts_app)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_layouts_smoke.cpp)
    target_sources(cvision_tests PRIVATE test_layouts_smoke.cpp)
    target_link_libraries(cvision_tests PRIVATE layouts_app)
endif()

if(TARGET forms_app)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_forms_smoke.cpp)
    target_sources(cvision_tests PRIVATE test_forms_smoke.cpp)
    target_link_libraries(cvision_tests PRIVATE forms_app)
endif()

# The event scripts (tools/docgen/event_script.hpp) are shared with the
# generator that writes their pinned frames; the M4 form demo's script plays
# the shipped examples/rootdialog object graph.
if(TARGET docgen_event_scripts)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES
        test_rootdialog_smoke.cpp
        test_close_veto_scripts.cpp
        test_menu_operability_scripts.cpp
        test_text_view_hyperlinks.cpp)
    target_sources(cvision_tests PRIVATE
        test_rootdialog_smoke.cpp
        test_close_veto_scripts.cpp
        test_menu_operability_scripts.cpp
        test_text_view_hyperlinks.cpp)
    target_link_libraries(cvision_tests PRIVATE docgen_event_scripts rootdialog_app)
endif()

# The Echo example's raw-byte input script (tools/docgen/echo_script.hpp) is
# shared with the generator that writes its pinned frames.
if(TARGET docgen_echo_script)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_echo_smoke.cpp)
    target_sources(cvision_tests PRIVATE test_echo_smoke.cpp)
    target_link_libraries(cvision_tests PRIVATE docgen_echo_script echo_app)
endif()

if(TARGET workbench_app)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_workbench_smoke.cpp)
    target_sources(cvision_tests PRIVATE test_workbench_smoke.cpp)
    target_link_libraries(cvision_tests PRIVATE workbench_app)
endif()

if(TARGET graphics_app)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_graphics_smoke.cpp)
    target_sources(cvision_tests PRIVATE test_graphics_smoke.cpp)
    target_link_libraries(cvision_tests PRIVATE graphics_app)
endif()

if(TARGET editor_app)
    # The menu scripts are shared with the generator that writes their pinned
    # frames (generate_editor_goldens).
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_editor_smoke.cpp test_editor_menu_scripts.cpp)
    target_sources(cvision_tests PRIVATE test_editor_smoke.cpp test_editor_menu_scripts.cpp)
    target_link_libraries(cvision_tests PRIVATE docgen_editor_scripts editor_app)
endif()

if(TARGET spin_app)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_spin_smoke.cpp)
    target_sources(cvision_tests PRIVATE test_spin_smoke.cpp)
    target_link_libraries(cvision_tests PRIVATE spin_app)
endif()

if(TARGET todo_app)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES
        test_todo_model.cpp test_todo_codec.cpp test_todo_repository.cpp
        test_todo_calendar_clock.cpp test_todo_controller.cpp test_todo_board.cpp test_todo_smoke.cpp
        test_todo_golden.cpp test_todo_host.cpp)
    target_sources(cvision_tests PRIVATE
        test_todo_model.cpp test_todo_codec.cpp test_todo_repository.cpp
        test_todo_calendar_clock.cpp test_todo_controller.cpp test_todo_board.cpp test_todo_smoke.cpp
        test_todo_golden.cpp test_todo_host.cpp)
    target_link_libraries(cvision_tests PRIVATE todo_app)
endif()

if(UNIX AND TARGET todo_app)
    list(APPEND CKVISION_POSIX_TEST_SOURCES test_todo_repository_posix.cpp)
    target_sources(cvision_tests PRIVATE test_todo_repository_posix.cpp)
endif()

if(TARGET sysinfo_app)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_sysinfo_analysis.cpp test_sysinfo_benchmark.cpp
        test_sysinfo_chart.cpp test_sysinfo_format.cpp test_sysinfo_reference_points.cpp
        test_sysinfo_report.cpp test_sysinfo_smoke.cpp)
    target_sources(cvision_tests PRIVATE test_sysinfo_analysis.cpp test_sysinfo_benchmark.cpp
        test_sysinfo_chart.cpp test_sysinfo_format.cpp test_sysinfo_reference_points.cpp
        test_sysinfo_report.cpp test_sysinfo_smoke.cpp)
    target_link_libraries(cvision_tests PRIVATE sysinfo_app)
endif()

# The real machine can only be read where there is one to read: this suite
# asserts the POSIX probe's internal consistency, never a value, because the
# machine it runs on is not the machine it was written on.
if(UNIX AND TARGET sysinfo_app)
    list(APPEND CKVISION_POSIX_TEST_SOURCES test_posix_system_probe.cpp)
    target_sources(cvision_tests PRIVATE test_posix_system_probe.cpp)
endif()

if(TARGET ckvision_editor_profile_sample)
    add_test(NAME editor_profile_extension_sample COMMAND ckvision_editor_profile_sample)
    set_tests_properties(editor_profile_extension_sample PROPERTIES LABELS "portable;example;editor" TIMEOUT 30)
endif()

if(UNIX AND TARGET terminal_app)
    list(APPEND CKVISION_POSIX_TEST_SOURCES test_terminal_app.cpp)
    target_sources(cvision_tests PRIVATE test_terminal_app.cpp)
    target_link_libraries(cvision_tests PRIVATE terminal_app)
endif()

# The widget width sweeps are rendered by the same docgen library that
# generates their pinned goldens.
if(TARGET docgen_clip_sweep)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_clip_sweep_golden.cpp)
    target_sources(cvision_tests PRIVATE test_clip_sweep_golden.cpp)
    target_link_libraries(cvision_tests PRIVATE docgen_clip_sweep)
endif()

if(TARGET docgen_frame_svg)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_frame_svg.cpp)
    target_sources(cvision_tests PRIVATE test_frame_svg.cpp)
    target_link_libraries(cvision_tests PRIVATE docgen_frame_svg)
    target_include_directories(cvision_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen)
endif()

# A suite is one source file: its cases share focused setup, yet every suite
# receives an independent process and bounded CTest timeout.  cktest also
# accepts --case for the narrower rerun printed by a failing test's output.
function(ckvision_add_test_suite source labels)
    get_filename_component(suite_name ${source} NAME_WE)
    add_test(NAME suite_${suite_name}
        COMMAND cvision_tests --suite ${source}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(suite_${suite_name} PROPERTIES LABELS "${labels}" TIMEOUT 60)
endfunction()

function(ckvision_add_sharded_test_suites source labels shard_count)
    get_filename_component(suite_name ${source} NAME_WE)
    math(EXPR last_shard "${shard_count} - 1")
    foreach(shard_index RANGE ${last_shard})
        add_test(NAME suite_${suite_name}_${shard_index}
            COMMAND cvision_tests --suite ${source} --shard ${shard_index}/${shard_count}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        set_tests_properties(suite_${suite_name}_${shard_index} PROPERTIES
            LABELS "${labels}"
            # PTY shards spawn real child processes and are I/O-bound; 30s was
            # tight enough that a loaded GitHub macOS runner tripped it on
            # suite_test_posix_terminal_0 (seconds locally). A hang would run
            # past any ceiling, so a higher one gives slow-but-progressing runs
            # room without hiding a real deadlock. The pty CI lane also retries.
            TIMEOUT 120)
    endforeach()
endfunction()

foreach(test_source IN LISTS CKVISION_PORTABLE_TEST_SOURCES)
    set(test_labels "unit;portable")
    if(test_source MATCHES "golden" OR test_source STREQUAL "test_frame_svg.cpp")
        set(test_labels "golden;portable")
        if(test_source STREQUAL "test_gallery_visual_golden.cpp" OR
           test_source STREQUAL "test_raster_scroll_golden.cpp" OR
           test_source STREQUAL "test_virtual_display_visual_golden.cpp")
            list(APPEND test_labels visual)
        endif()
    elseif(test_source MATCHES "smoke|_scripts\\.cpp$")
        set(test_labels "script;portable")
    endif()
    if(test_source STREQUAL "test_presenter.cpp")
        list(APPEND test_labels raw-byte)
    elseif(test_source STREQUAL "test_golden_capture.cpp")
        list(APPEND test_labels symbolic-scene)
    elseif(test_source STREQUAL "test_text_width_divergent.cpp")
        # D-019: every case in this suite is a known terminal-divergent
        # width class; `ctest -L d019` runs exactly that set.
        list(APPEND test_labels d019)
    elseif(test_source STREQUAL "test_virtual_display.cpp" OR
           test_source STREQUAL "test_virtual_display_visual_golden.cpp")
        list(APPEND test_labels visual-protocol)
    endif()
    ckvision_add_test_suite(${test_source} "${test_labels}")
endforeach()

if(WIN32)
    ckvision_add_test_suite(test_windows_terminal_subsession.cpp "platform;windows;conpty")
endif()

if(UNIX)
    foreach(test_source IN LISTS CKVISION_POSIX_TEST_SOURCES)
        set(test_labels "platform;posix")
        if(test_source STREQUAL "test_posix_terminal.cpp")
            list(APPEND test_labels pty)
            ckvision_add_sharded_test_suites(${test_source} "${test_labels}" 4)
        else()
            ckvision_add_test_suite(${test_source} "${test_labels}")
        endif()
    endforeach()
endif()

# Every published screenshot is a product claim, not a decorative copy. Each
# group of tools/docgen/screenshot-manifest.txt names a capture tool and the
# figures it draws; each gets one `<name>_visual_capture` test that re-runs the
# tool through the real Presenter and HeadlessTerminal's VT/Sixel decoder
# (capture code never receives a source Image, D-035) and requires exactly the
# group's figures, byte-identical to docs/generated/screenshots/. check_docs.py
# holds the manifest to the published directory, so no screenshot there goes
# ungated. A tool this build does not have (the terminal capture needs a POSIX
# host) registers nothing.
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/ScreenshotManifest.cmake)
set(ckvision_screenshot_manifest ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen/screenshot-manifest.txt)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ckvision_screenshot_manifest})
ckvision_read_screenshot_manifest(${ckvision_screenshot_manifest} ckvision_screenshot_tools)
foreach(capture_tool IN LISTS ckvision_screenshot_tools)
    if(NOT TARGET ${capture_tool})
        continue()
    endif()
    string(REGEX REPLACE "^capture_" "" capture_name "${capture_tool}")
    string(REGEX REPLACE "_screenshots$" "" capture_name "${capture_name}")
    add_test(NAME ${capture_name}_visual_capture
        COMMAND ${CMAKE_COMMAND}
            -DCKVISION_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
            -DCKVISION_ARTIFACT_DIR=${CMAKE_BINARY_DIR}/test-artifacts/screenshots/${capture_name}
            -DCKVISION_CAPTURE_TOOL=${capture_tool}
            -DCKVISION_CAPTURE=$<TARGET_FILE:${capture_tool}>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_screenshots.cmake)
    set_tests_properties(${capture_name}_visual_capture PROPERTIES
        LABELS "golden;visual;documentation;${capture_name}" TIMEOUT 60)
endforeach()

add_test(NAME install_package_smoke
    COMMAND ${CMAKE_COMMAND}
        -DCKVISION_BUILD_DIR=${CMAKE_BINARY_DIR}
        -DCKVISION_SMOKE_DIR=${CMAKE_BINARY_DIR}/test-artifacts/install-package
        -DCKVISION_EXPECT_TERMINAL=$<BOOL:$<TARGET_EXISTS:ckvision_terminal>>
        -DCKVISION_EXPECT_PROFILE_SAMPLE=$<BOOL:$<TARGET_EXISTS:ckvision_editor_profile_sample>>
        -DCKVISION_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
        -DCKVISION_CONFIG=$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/install_package_smoke.cmake)
# Both package tests drive `cmake --install` / `cmake --build` against the
# one shared build tree. Run concurrently under `ctest -j`, they race each
# other over the same object files and one of them fails on a half-written
# artifact -- a flake in the gate that says nothing about the library.
set_tests_properties(install_package_smoke PROPERTIES LABELS "package;install;client" TIMEOUT 120
    RESOURCE_LOCK ckvision_build_tree)

if(TARGET ckvision_terminal AND TARGET capture_terminal_screenshots)
    add_test(NAME client_bundle_smoke
        COMMAND ${CMAKE_COMMAND}
            -DCKVISION_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
            -DCKVISION_BUILD_DIR=${CMAKE_BINARY_DIR}
            -DCKVISION_BUNDLE_DIR=${CMAKE_BINARY_DIR}/test-artifacts/client-bundle
            -P ${CMAKE_CURRENT_SOURCE_DIR}/client_bundle_smoke.cmake)
    set_tests_properties(client_bundle_smoke PROPERTIES LABELS "package;handoff;artifact;client" TIMEOUT 180
        RESOURCE_LOCK ckvision_build_tree)
endif()

# Python is build/CI tooling, not a library dependency (D-004 untouched).
# REQUIRED so the layer/include enforcement gate can never vanish silently.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
# Regenerate the pinned scene dumps and decoded pixel planes in an isolated build-owned
# directory and compare raw bytes. This makes each host validate the same
# portable output without modifying the reviewed source fixtures.
if(TARGET generate_hello_golden AND TARGET generate_editor_goldens AND
   TARGET generate_window_activation_goldens AND TARGET generate_root_resize_goldens AND
   TARGET generate_desktop_dialog_goldens AND TARGET generate_window_z_order_golden AND
   TARGET generate_shrink_storm_goldens AND TARGET generate_window_resize_goldens AND
   TARGET generate_layouts_goldens AND TARGET generate_filebrowser_goldens AND
   TARGET generate_clip_sweep_goldens AND TARGET generate_hostile_content_goldens AND
   TARGET generate_event_script_goldens AND TARGET generate_theme_override_goldens AND
   TARGET generate_forms_goldens AND TARGET generate_terminal_plane_goldens AND
   TARGET generate_raster_scroll_goldens AND TARGET generate_gallery_goldens AND
   TARGET generate_echo_goldens)
    add_test(NAME generated_golden_bytes
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_generated_goldens.py
            --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/golden
            --scratch ${CMAKE_BINARY_DIR}/test-artifacts/generated-goldens
            --generator "generate_hello_golden=$<TARGET_FILE:generate_hello_golden>"
            --generator "generate_editor_goldens=$<TARGET_FILE:generate_editor_goldens>"
            --generator "generate_forms_goldens=$<TARGET_FILE:generate_forms_goldens>"
            --generator "generate_echo_goldens=$<TARGET_FILE:generate_echo_goldens>"
            --generator "generate_window_activation_goldens=$<TARGET_FILE:generate_window_activation_goldens>"
            --generator "generate_root_resize_goldens=$<TARGET_FILE:generate_root_resize_goldens>"
            --generator "generate_desktop_dialog_goldens=$<TARGET_FILE:generate_desktop_dialog_goldens>"
            --generator "generate_shrink_storm_goldens=$<TARGET_FILE:generate_shrink_storm_goldens>"
            --generator "generate_window_resize_goldens=$<TARGET_FILE:generate_window_resize_goldens>"
            --generator "generate_layouts_goldens=$<TARGET_FILE:generate_layouts_goldens>"
            --generator "generate_filebrowser_goldens=$<TARGET_FILE:generate_filebrowser_goldens>"
            --generator "generate_window_z_order_golden=$<TARGET_FILE:generate_window_z_order_golden>"
            --generator "generate_clip_sweep_goldens=$<TARGET_FILE:generate_clip_sweep_goldens>"
            --generator "generate_hostile_content_goldens=$<TARGET_FILE:generate_hostile_content_goldens>"
            --generator "generate_event_script_goldens=$<TARGET_FILE:generate_event_script_goldens>"
            --generator "generate_theme_override_goldens=$<TARGET_FILE:generate_theme_override_goldens>"
            --generator "generate_terminal_plane_goldens=$<TARGET_FILE:generate_terminal_plane_goldens>"
            --generator "generate_raster_scroll_goldens=$<TARGET_FILE:generate_raster_scroll_goldens>"
            --generator "generate_gallery_goldens=$<TARGET_FILE:generate_gallery_goldens>")
    set_tests_properties(generated_golden_bytes PROPERTIES
        LABELS "golden;visual;raw-byte;portable;script" TIMEOUT 60)
endif()

# The appearance matrix (WP-38): every public element x state x built-in
# scheme, regenerated and compared byte for byte, and proved complete against
# the public view types and the states each element's traits require.
if(TARGET generate_appearance_matrix)
    add_test(NAME appearance_matrix
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_appearance_matrix.py
            --generator $<TARGET_FILE:generate_appearance_matrix>
            --fixtures ${CMAKE_CURRENT_SOURCE_DIR}/golden/appearance
            --scratch ${CMAKE_BINARY_DIR}/test-artifacts/appearance-matrix)
    set_tests_properties(appearance_matrix PROPERTIES
        LABELS "golden;visual;symbolic-scene;portable;script" TIMEOUT 120)
    # The expanded four-scheme matrix takes 131 s with TSan instrumentation
    # on the verification host (D-119). Retain headroom for shared CI runners;
    # this deadline does not change any runtime benchmark or golden check.
    if(CKVISION_SANITIZE MATCHES "(^|,)thread(,|$)")
        set_tests_properties(appearance_matrix PROPERTIES TIMEOUT 300)
    endif()
endif()

# The classic-look specification (the internal plans, ROADMAP M5): every
# checklist item's cited cells are verified in the pinned goldens, and the
# embedded palette table is regenerated from the Classic theme and compared.
# The palette half needs the docgen tool, which exists only in builds with
# the examples; the golden half never depends on it. The specification is a
# planning document, which the published snapshot does not carry
# (tools/publish/publicignore); a tree without it has nothing to hold the
# goldens to and registers neither gate. The goldens themselves stay pinned
# byte for byte by the appearance matrix and the golden suites.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../plans/classic-look.md)
    if(TARGET generate_classic_palette_table)
        add_test(NAME classic_look
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_classic_look.py
                --root ${CMAKE_CURRENT_SOURCE_DIR}/..
                --palette-tool $<TARGET_FILE:generate_classic_palette_table>)
    else()
        add_test(NAME classic_look
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_classic_look.py
                --root ${CMAKE_CURRENT_SOURCE_DIR}/..)
    endif()
    set_tests_properties(classic_look PROPERTIES LABELS "golden;script" TIMEOUT 30)
    add_test(NAME classic_look_self_test
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_classic_look.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}/.. --self-test)
    set_tests_properties(classic_look_self_test PROPERTIES LABELS "golden;script" TIMEOUT 30)
endif()

add_test(NAME layer_check
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_layers.py)
set_tests_properties(layer_check PROPERTIES LABELS "script;architecture" TIMEOUT 30)
add_test(NAME layer_checker_self_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_layers.py --self-test)
set_tests_properties(layer_checker_self_test PROPERTIES LABELS "script;architecture" TIMEOUT 30)

# The M9 60-line Hello exit criterion (the roadmap), wired for real:
# never vanish silently, same reasoning as layer_check above.
add_test(NAME hello_line_budget
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_hello_line_budget.py)
set_tests_properties(hello_line_budget PROPERTIES LABELS "script;budget" TIMEOUT 30)

add_test(NAME example_hygiene
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_example_hygiene.py)
set_tests_properties(example_hygiene PROPERTIES LABELS "script;api" TIMEOUT 30)
add_test(NAME example_hygiene_self_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_example_hygiene.py --self-test)
set_tests_properties(example_hygiene_self_test PROPERTIES LABELS "script;api" TIMEOUT 30)

add_test(NAME doc_snippet_check
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen/extract_snippets.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_tests_properties(doc_snippet_check PROPERTIES LABELS "script;documentation" TIMEOUT 30)

# The gallery's value-type field tables are generated from the struct
# declarations; this fails when a field is added, renamed, retyped or
# re-defaulted without the table following.
add_test(NAME doc_field_tables
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen/sync_field_tables.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_tests_properties(doc_field_tables PROPERTIES LABELS "script;documentation" TIMEOUT 30)

add_test(NAME doc_widget_coverage
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen/check_docs.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}/.. --coverage)
set_tests_properties(doc_widget_coverage PROPERTIES LABELS "script;documentation" TIMEOUT 30)

add_test(NAME doc_links
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen/check_docs.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}/.. --links --screenshots)
set_tests_properties(doc_links PROPERTIES LABELS "script;documentation" TIMEOUT 30)

add_test(NAME doc_generation_smoke
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen/check_docs.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_tests_properties(doc_generation_smoke PROPERTIES LABELS "script;documentation" TIMEOUT 30)

# WP-38: every public declaration in the installed headers carries a
# reference comment; the self-test proves the scanner still tells a
# documented declaration from an undocumented one.
add_test(NAME doc_api_comments
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen/check_api_comments.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_tests_properties(doc_api_comments PROPERTIES LABELS "script;documentation" TIMEOUT 30)
add_test(NAME doc_api_comments_self_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen/check_api_comments.py
            --self-test)
set_tests_properties(doc_api_comments_self_test PROPERTIES LABELS "script;documentation" TIMEOUT 30)

# WP-38: every public view type is driven by at least one Application-level
# interaction script -- a named CK_TEST that reaches it and sends events
# through dispatch, inject_event or step, never its handlers directly. The map
# is the "Interaction scripts" table in docs/coverage.md; the self-test proves
# the checker still rejects each way a row can be wrong.
add_test(NAME interaction_script_coverage
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen/check_interaction_scripts.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_tests_properties(interaction_script_coverage PROPERTIES LABELS "script;documentation" TIMEOUT 30)
add_test(NAME interaction_script_coverage_self_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen/check_interaction_scripts.py
            --self-test)
set_tests_properties(interaction_script_coverage_self_test PROPERTIES LABELS "script;documentation" TIMEOUT 30)

# The engineering standard: every source, build and tooling file opens with the copyright
# line and the SPDX licence line; the self-test proves the rule still bites.
add_test(NAME license_headers
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_license_headers.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_tests_properties(license_headers PROPERTIES LABELS "script;provenance" TIMEOUT 30)
add_test(NAME license_headers_self_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_license_headers.py
            --self-test)
set_tests_properties(license_headers_self_test PROPERTIES LABELS "script;provenance" TIMEOUT 30)

# D-012/WP-23: scan every tracked and non-ignored text artifact for
# prohibited source-fidelity claims, and separately prove that the checker
# rejects representative positive cases while accepting clean-room wording.
add_test(NAME provenance_check
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_provenance.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_tests_properties(provenance_check PROPERTIES LABELS "script;provenance" TIMEOUT 30)
add_test(NAME provenance_checker_self_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_provenance.py
            --self-test)
set_tests_properties(provenance_checker_self_test PROPERTIES LABELS "script;provenance" TIMEOUT 30)

# Differential VT conformance (tools/vtconform/README.md): the same byte
# scripts through ckVision's emulator and through an established terminal,
# compared cell by cell. It reports and skips when no reference terminal is
# installed, so it never fails a host for not having one -- but where one
# exists, an unexplained difference in terminal semantics fails the build.
if(TARGET ckvision_vt_dump)
    add_test(NAME vt_conformance
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/vtconform/conform.py
                --dump-tool $<TARGET_FILE:ckvision_vt_dump>)
    set_tests_properties(vt_conformance PROPERTIES LABELS "script;conformance;terminal" TIMEOUT 180)
endif()

add_test(NAME test_inventory
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/report_test_inventory.py
            --test-dir ${CMAKE_BINARY_DIR} --ctest ${CMAKE_CTEST_COMMAND})
set_tests_properties(test_inventory PROPERTIES LABELS "script;ci" TIMEOUT 30)
