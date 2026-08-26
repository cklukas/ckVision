// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// End-to-end evidence for the shipped editor example. The test drives its
// public application path against HeadlessTerminal, the same object graph as
// the interactive executable and documentation capture tool.
#include <string>
#include <string_view>
#include <fstream>
#include <sstream>

#include "cvision/testing/cktest.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/minimized_window_stub.hpp"
#include "cvision/widgets/text_editor.hpp"
#include "cvision/widgets/window.hpp"
#include "editor_app.hpp"

using ckv::ManualClock;
using ckv::Point;
using ckv::ui::Application;

namespace {
struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile()};
    ManualClock clock;
    Application app{term, clock};
    ckv::editor_example::EditorApp editor{app};
};

bool display_contains(const ckv::term::VirtualDisplay& display, std::string_view needle) {
    const ckv::FrameView frame = display.frame();
    for (int y = 0; y < frame.size().height; ++y) {
        std::string row;
        for (int x = 0; x < frame.size().width; ++x) row += frame.at(Point{x, y}).grapheme();
        if (row.find(needle) != std::string::npos) return true;
    }
    return false;
}

std::string read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string capture(const Fixture& fixture) {
    return ckv::golden::serialize(ckv::scene::capture(fixture.app.composed_surface(), fixture.app.current_cursor()));
}
}  // namespace

CK_TEST(the_editor_about_dialog_carries_the_project_copyright) {
    Fixture f;
    CK_CHECK(f.app.execute_command(f.app.commands().standard().help));
    f.app.step(0);
    CK_CHECK(display_contains(f.term.display(),
                              "Copyright (c) 2026 C. Klukas. All rights reserved."));
}

CK_TEST(the_editor_example_initial_frame_matches_its_pinned_visual_contract) {
    Fixture f;
    f.app.step(0);

    const std::string actual = capture(f);
    const std::string expected = read_file("golden/editor_initial.dump");
    CK_CHECK(!expected.empty());
    CK_CHECK(actual == expected);
}

CK_TEST(the_editor_example_search_highlight_matches_its_pinned_visual_contract) {
    Fixture f;
    f.app.step(0);
    for (int index = 0; index < 4; ++index)
        f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::Shift, ""}});
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "f"}});
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F3, ckv::Modifier::None, ""}});
    f.app.step(0);
    CK_CHECK(capture(f) == read_file("golden/editor_search.dump"));
}

CK_TEST(the_editor_example_keeps_search_selection_and_caret_state_across_all_builtin_themes) {
    Fixture f;
    f.app.step(0);
    for (int index = 0; index < 4; ++index)
        f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::Shift, ""}});
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "f"}});
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F3, ckv::Modifier::None, ""}});
    f.app.step(0);
    const std::string classic = capture(f);
    const auto selected = f.editor.editor()->selection();
    CK_CHECK(selected.has_value());
    CK_CHECK(f.app.current_cursor().visible);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(f.app.roles());
    f.app.theme() = ckv::ui::make_dark_theme(f.app.roles(), roles);
    f.app.root().invalidate();
    f.app.step(0);
    const std::string dark = capture(f);
    f.app.theme() = ckv::ui::make_light_theme(f.app.roles(), roles);
    f.app.root().invalidate();
    f.app.step(0);
    const std::string light = capture(f);
    f.app.theme() = ckv::ui::make_mono_theme(f.app.roles(), roles);
    f.app.root().invalidate();
    f.app.step(0);
    const std::string mono = capture(f);
    CK_CHECK(!dark.empty() && !light.empty() && !mono.empty());
    CK_CHECK(dark != classic);
    CK_CHECK(light != classic);
    CK_CHECK(mono != classic);
    CK_CHECK(f.editor.editor()->selection().has_value());
    CK_CHECK(f.editor.editor()->selection()->begin.byte == selected->begin.byte);
    CK_CHECK(f.editor.editor()->selection()->end.byte == selected->end.byte);
    CK_CHECK(f.app.current_cursor().visible);
}

CK_TEST(the_editor_example_dirty_close_prompt_matches_its_pinned_visual_contract) {
    Fixture f;
    f.app.step(0);
    f.app.dispatch(ckv::TextEvent{"#", false});
    (void)f.editor.window()->close();
    f.app.step(0);
    CK_CHECK(capture(f) == read_file("golden/editor_close_confirm.dump"));
}

CK_TEST(the_editor_example_renders_document_chrome_and_yaml_source) {
    Fixture f;
    f.app.step(0);

    CK_CHECK(display_contains(f.term.display(), "Editor"));
    CK_CHECK(display_contains(f.term.display(), "Edit"));
    CK_CHECK(display_contains(f.term.display(), "Search"));
    CK_CHECK(display_contains(f.term.display(), "config.yaml"));
    CK_CHECK(display_contains(f.term.display(), "name: ckVision"));
    CK_CHECK(display_contains(f.term.display(), "version: 0.1"));
    CK_CHECK(display_contains(f.term.display(), "Quit"));
    CK_CHECK(f.app.focused() == f.editor.editor());
}

CK_TEST(the_editor_example_opens_json_bash_and_plain_samples_through_its_public_file_workflow) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.editor.open_sample("settings.json") == ckv::widgets::EditorFileStatus::Ok);
    CK_CHECK(f.editor.editor()->profile_id() == "json");
    CK_CHECK(f.editor.document()->text().find("\"enabled\"") != std::string::npos);
    CK_CHECK(f.editor.open_sample("sample.sh") == ckv::widgets::EditorFileStatus::Ok);
    CK_CHECK(f.editor.editor()->profile_id() == "bash");
    CK_CHECK(f.editor.document()->text().starts_with("#!/usr/bin/env bash"));
    CK_CHECK(f.editor.open_sample("notes.txt") == ckv::widgets::EditorFileStatus::Ok);
    CK_CHECK(f.editor.editor()->profile_id() == "plain");
    CK_CHECK(f.editor.open_sample("config.yaml") == ckv::widgets::EditorFileStatus::Ok);
    CK_CHECK(f.editor.editor()->profile_id() == "yaml");
}

CK_TEST(the_editor_example_publishes_a_wrap_aware_visible_caret_for_the_focused_editor) {
    Fixture f;
    f.app.step(0);
    const ckv::CursorState initial = f.app.current_cursor();
    CK_CHECK(initial.visible);
    CK_CHECK(initial.shape == ckv::CursorShape::Bar);
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::None, ""}}));
    f.app.step(0);
    const ckv::CursorState moved = f.app.current_cursor();
    CK_CHECK(moved.visible);
    CK_CHECK(moved.position.x > initial.position.x);
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Insert, ckv::Modifier::None, ""}}));
    f.app.step(0);
    CK_CHECK(f.app.current_cursor().shape == ckv::CursorShape::Block);
}

CK_TEST(the_editor_example_frame_shows_the_new_edit_mode_on_the_frame_after_insert_alone) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(display_contains(f.term.display(), "INS"));
    CK_CHECK(!display_contains(f.term.display(), "OVR"));
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Insert, ckv::Modifier::None, ""}}));
    // One frame, and no second keystroke behind it: the toggle itself has to
    // reach the frame overlay, not the next cursor move that happens to.
    f.app.step(0);
    CK_CHECK(display_contains(f.term.display(), "OVR"));
    CK_CHECK(!display_contains(f.term.display(), "INS"));
}

CK_TEST(the_editor_example_minimized_window_is_parked_where_the_reader_can_get_it_back) {
    // The reported bug, in the example it was reported against: minimizing
    // the editor's only window used to leave a bare desktop with no way back
    // — no switcher bar in this example, and no key bound to the window list
    // — so the reader had to quit. D-064 puts the window's own top frame on
    // the bottom edge instead.
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.editor.window() != nullptr);
    CK_CHECK(display_contains(f.term.display(), "Editor — config.yaml"));

    f.editor.window()->set_minimized(true);
    f.app.step(0);
    CK_CHECK(!f.editor.window()->visible());
    // Still on screen, and readable as a window: its name, its close control
    // and the control that brings it back.
    CK_CHECK(display_contains(f.term.display(), "Editor — config.yaml"));
    CK_CHECK(display_contains(f.term.display(), "[↑]"));
    CK_CHECK(display_contains(f.term.display(), "[■]"));

    const ckv::widgets::Desktop* const desktop = f.editor.desktop();
    CK_CHECK(desktop != nullptr);
    CK_CHECK(desktop->parked_windows().size() == 1U);
    const ckv::Rect stub = desktop->parked_windows().front()->bounds();
    // Above the status line, not over it.
    CK_CHECK(display_contains(f.term.display(), "Alt+X Quit"));
    CK_CHECK(stub.y == 22);

    // One click on the parked frame and the reader has their editor back,
    // the size and shape it was. A click is a press AND a release: the row
    // decides on the release, like the frame's own controls, so the press
    // alone leaves the window where it is.
    const ckv::Rect before = f.editor.window()->bounds();
    const ckv::Point on_stub{stub.x + stub.width / 2, stub.y};
    CK_CHECK(f.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, on_stub,
                                            std::nullopt, ckv::Modifier::None}));
    f.app.step(0);
    CK_CHECK(f.editor.window()->minimized());  // armed, not decided
    CK_CHECK(f.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, on_stub,
                                            std::nullopt, ckv::Modifier::None}));
    f.app.step(0);
    CK_CHECK(!f.editor.window()->minimized());
    CK_CHECK(f.editor.window()->visible());
    CK_CHECK(f.editor.window()->bounds() == before);
    CK_CHECK(desktop->parked_windows().empty());
}

CK_TEST(the_editor_example_search_command_path_finds_the_current_selection) {
    Fixture f;
    f.app.step(0);
    for (int index = 0; index < 4; ++index)
        CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::Shift, ""}}));
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "f"}}));
    CK_CHECK(f.editor.editor()->search_query().text == "name");
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F3, ckv::Modifier::None, ""}}));
    CK_CHECK(f.editor.editor()->selection().has_value());
}

CK_TEST(the_editor_example_routes_control_shift_selection_and_clipboard_editing) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Home, ckv::Modifier::Ctrl, ""}}));
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::Ctrl | ckv::Modifier::Shift, ""}}));
    CK_CHECK(f.editor.editor()->selection().has_value());
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "c"}}));
    CK_CHECK(f.app.clipboard_text() == "name: ");
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Insert, ckv::Modifier::Ctrl, ""}}));
    CK_CHECK(f.app.clipboard_text() == "name: ");
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "x"}}));
    CK_CHECK(f.editor.document()->text().starts_with("ckVision"));
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Insert, ckv::Modifier::Shift, ""}}));
    CK_CHECK(f.editor.document()->text().starts_with("name: ckVision"));
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Home, ckv::Modifier::Ctrl, ""}}));
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Right, ckv::Modifier::Ctrl | ckv::Modifier::Shift, ""}}));
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Delete, ckv::Modifier::None, ""}}));
    CK_CHECK(f.editor.document()->text().starts_with("ckVision"));
}

CK_TEST(the_editor_example_reflows_long_lines_with_a_visible_marker_and_can_disable_wrap) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(display_contains(f.term.display(), "\xE2\x86\xAA"));  // U+21AA ↪

    f.editor.editor()->set_wrap_mode(ckv::widgets::WrapMode::None);
    f.app.step(0);
    CK_CHECK(!display_contains(f.term.display(), "\xE2\x86\xAA"));
}

CK_TEST(the_editor_window_footer_tracks_the_logical_cursor_position_through_wrapped_rows) {
    Fixture f;
    f.app.step(0);
    for (int index = 0; index < 3; ++index)
        CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Down, ckv::Modifier::None, ""}}));
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::End, ckv::Modifier::None, ""}}));
    f.app.step(0);

    const std::string document = f.editor.document()->text();
    const std::size_t start = document.rfind('\n', document.size() - 2U) + 1U;
    const std::size_t end = document.find('\n', start);
    const std::size_t column = end - start + 1U;
    const std::string expected = "Ln 4, Col " + std::to_string(column);
    CK_CHECK(f.editor.editor()->status().line == 4U);
    CK_CHECK(f.editor.editor()->status().column == column);
    CK_CHECK(display_contains(f.term.display(), expected));
}

CK_TEST(the_editor_example_routes_typing_into_its_shared_document) {
    Fixture f;
    f.app.step(0);
    f.app.dispatch(ckv::TextEvent{"#", false});
    f.app.step(0);

    CK_CHECK(f.editor.document()->text().starts_with('#'));
    CK_CHECK(display_contains(f.term.display(), "#name: ckVision"));
}

CK_TEST(the_editor_example_save_command_uses_its_injected_file_controller) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.app.dispatch(ckv::TextEvent{"#", false}));
    CK_CHECK(f.editor.document()->modified());
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "s"}}));
    CK_CHECK(!f.editor.document()->modified());
}

CK_TEST(the_editor_example_requires_an_explicit_save_discard_or_cancel_choice_before_close) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.app.dispatch(ckv::TextEvent{"#", false}));
    CK_CHECK(f.editor.document()->modified());
    CK_CHECK(!f.editor.window()->close());
    CK_CHECK(f.app.is_modal());
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}}));
    f.app.step(0);
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(!f.editor.document()->modified());
}
