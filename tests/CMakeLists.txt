# Copyright (c) 2026 C. Klukas. All rights reserved.
# SPDX-License-Identifier: MIT
set(CKVISION_PORTABLE_TEST_SOURCES
    test_anchor_pane.cpp
    test_animation.cpp
    test_application.cpp
    test_application_render.cpp
    test_base64.cpp
    test_box_drawing.cpp
    test_canvas.cpp
    test_clock_diagnostics_error.cpp
    test_color_style_cell.cpp
    test_command.cpp
    test_common_components.cpp
    test_compositor.cpp
    test_combo_box.cpp
    test_filesystem.cpp
    test_desktop.cpp
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
    test_hot_path_allocations.cpp
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
    test_paged_strip.cpp
    test_popup_list.cpp
    test_overlay.cpp
    test_painter.cpp
    test_paste_security.cpp
    test_platform_services.cpp
    test_pointer_shape.cpp
    test_presenter.cpp
    test_progress.cpp
    test_record_replay_terminal.cpp
    test_rect_ops.cpp
    test_root_resize_golden.cpp
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
    test_unicode_grapheme_break.cpp
    test_theme.cpp
    test_tree_view.cpp
    test_utf8.cpp
    test_version.cpp
    test_virtual_display.cpp
    test_virtual_display_visual_golden.cpp
    test_view.cpp
    test_window_activation_golden.cpp
    test_wp35.cpp
    test_wp36.cpp
    test_widgets.cpp
    test_window.cpp
    test_window_list_dialog.cpp
    test_window_switcher_bar.cpp)

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

if(UNIX)
    set(CKVISION_POSIX_TEST_SOURCES
        test_posix_filesystem.cpp
        test_posix_terminal_subsession.cpp
        test_posix_terminal.cpp)
    target_sources(cvision_tests PRIVATE ${CKVISION_POSIX_TEST_SOURCES})

    add_executable(terminal_nested_child terminal_nested_child.cpp)
    target_link_libraries(terminal_nested_child PRIVATE ckvision::cvision)
    ckvision_strict(terminal_nested_child)
    add_dependencies(cvision_tests terminal_nested_child)
    target_compile_definitions(cvision_tests PRIVATE
        CKV_NESTED_TERMINAL_CHILD_PATH="$<TARGET_FILE:terminal_nested_child>")

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
    target_link_libraries(cvision_tests PRIVATE gallery_app)
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
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_editor_smoke.cpp)
    target_sources(cvision_tests PRIVATE test_editor_smoke.cpp)
    target_link_libraries(cvision_tests PRIVATE editor_app)
endif()

if(TARGET spin_app)
    list(APPEND CKVISION_PORTABLE_TEST_SOURCES test_spin_smoke.cpp)
    target_sources(cvision_tests PRIVATE test_spin_smoke.cpp)
    target_link_libraries(cvision_tests PRIVATE spin_app)
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
           test_source STREQUAL "test_virtual_display_visual_golden.cpp")
            list(APPEND test_labels visual)
        endif()
    elseif(test_source MATCHES "smoke")
        set(test_labels "script;portable")
    endif()
    if(test_source STREQUAL "test_presenter.cpp")
        list(APPEND test_labels raw-byte)
    elseif(test_source STREQUAL "test_golden_capture.cpp")
        list(APPEND test_labels symbolic-scene)
    elseif(test_source STREQUAL "test_virtual_display.cpp" OR
           test_source STREQUAL "test_virtual_display_visual_golden.cpp")
        list(APPEND test_labels visual-protocol)
    endif()
    ckvision_add_test_suite(${test_source} "${test_labels}")
endforeach()

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

# Keep a human-inspectable rendering artifact beside the machine assertions.
# This drives Presenter output through HeadlessTerminal's VT/Sixel decoder;
# capture code never receives the source Image directly (D-035).
if(TARGET capture_widget_gallery_screenshots)
    add_test(NAME widget_gallery_visual_capture
        COMMAND capture_widget_gallery_screenshots
                ${CMAKE_BINARY_DIR}/test-artifacts/widget-gallery)
    set_tests_properties(widget_gallery_visual_capture PROPERTIES LABELS "golden;visual;artifact" TIMEOUT 60)
endif()

if(TARGET capture_gallery_screenshots)
    add_test(NAME gallery_visual_capture
        COMMAND capture_gallery_screenshots
                ${CMAKE_BINARY_DIR}/test-artifacts/gallery)
    set_tests_properties(gallery_visual_capture PROPERTIES LABELS "golden;visual;artifact" TIMEOUT 60)
endif()

if(TARGET capture_filebrowser_screenshots)
    add_test(NAME filebrowser_visual_capture
        COMMAND capture_filebrowser_screenshots
                ${CMAKE_BINARY_DIR}/test-artifacts/filebrowser)
    set_tests_properties(filebrowser_visual_capture PROPERTIES LABELS "golden;visual;artifact" TIMEOUT 60)
endif()

if(TARGET capture_hello_screenshots)
    add_test(NAME hello_visual_capture
        COMMAND capture_hello_screenshots
                ${CMAKE_BINARY_DIR}/test-artifacts/hello)
    set_tests_properties(hello_visual_capture PROPERTIES LABELS "golden;visual;artifact" TIMEOUT 60)
endif()

foreach(example_name IN ITEMS layouts forms workbench graphics spin)
    if(TARGET capture_${example_name}_screenshots)
        add_test(NAME ${example_name}_visual_capture
            COMMAND capture_${example_name}_screenshots
                    ${CMAKE_BINARY_DIR}/test-artifacts/${example_name})
        set_tests_properties(${example_name}_visual_capture PROPERTIES
            LABELS "golden;visual;artifact" TIMEOUT 60)
    endif()
endforeach()

if(TARGET capture_terminal_screenshots)
    add_test(NAME terminal_visual_capture
        COMMAND capture_terminal_screenshots
                ${CMAKE_BINARY_DIR}/test-artifacts/terminal)
    set_tests_properties(terminal_visual_capture PROPERTIES
        LABELS "golden;visual;artifact;terminal" TIMEOUT 60)
endif()

add_test(NAME install_package_smoke
    COMMAND ${CMAKE_COMMAND}
        -DCKVISION_BUILD_DIR=${CMAKE_BINARY_DIR}
        -DCKVISION_SMOKE_DIR=${CMAKE_BINARY_DIR}/test-artifacts/install-package
        -DCKVISION_EXPECT_TERMINAL=$<BOOL:$<TARGET_EXISTS:ckvision_terminal>>
        -DCKVISION_EXPECT_PROFILE_SAMPLE=$<BOOL:$<TARGET_EXISTS:ckvision_editor_profile_sample>>
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

add_test(NAME doc_snippet_check
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/docgen/extract_snippets.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_tests_properties(doc_snippet_check PROPERTIES LABELS "script;documentation" TIMEOUT 30)

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
            --test-dir ${CMAKE_BINARY_DIR})
set_tests_properties(test_inventory PROPERTIES LABELS "script;ci" TIMEOUT 30)
