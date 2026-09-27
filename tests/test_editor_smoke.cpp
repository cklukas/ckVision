// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// End-to-end evidence for the shipped editor example. The test drives its
// public application path against HeadlessTerminal, the same object graph as
// the interactive executable and documentation capture tool.
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <fstream>
#include <sstream>

#include "cvision/testing/cktest.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/minimized_window_stub.hpp"
#include "cvision/widgets/menu.hpp"
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

// Event scripts: every key and mouse report goes in through the terminal and
// reaches the example by Application::step, the way a host's input does.
void press(Fixture& fixture, ckv::Key key, ckv::Modifier modifiers = ckv::Modifier::None,
           std::string text = {}) {
    fixture.term.inject_event(ckv::KeyEvent{ckv::KeyChord{key, modifiers, std::move(text)}});
    fixture.app.step(0);
}

void type(Fixture& fixture, std::string_view text) {
    fixture.term.inject_bytes(text, 0);
    fixture.app.step(0);
}

void mouse(Fixture& fixture, ckv::MouseAction action, ckv::MouseButton button, Point cell) {
    fixture.term.inject_event(ckv::MouseEvent{action, button, cell, std::nullopt, ckv::Modifier::None});
    fixture.app.step(0);
}

// Where `needle` starts on screen, cell by cell, or nothing.
std::optional<Point> find_on_screen(const ckv::term::VirtualDisplay& display, std::string_view needle) {
    const ckv::FrameView frame = display.frame();
    for (int y = 0; y < frame.size().height; ++y) {
        for (int x = 0; x + static_cast<int>(needle.size()) <= frame.size().width; ++x) {
            bool matches = true;
            for (std::size_t index = 0; index < needle.size() && matches; ++index)
                matches = frame.at(Point{x + static_cast<int>(index), y}).grapheme() == needle.substr(index, 1);
            if (matches) return Point{x, y};
        }
    }
    return std::nullopt;
}

// The text of screen row `y`.
std::string screen_row(const ckv::term::VirtualDisplay& display, int y) {
    const ckv::FrameView frame = display.frame();
    std::string row;
    for (int x = 0; x < frame.size().width; ++x) row += frame.at(Point{x, y}).grapheme();
    return row;
}

// Whether `view` lies inside `ancestor`'s subtree.
bool is_within(const ckv::ui::View* view, const ckv::ui::View* ancestor) {
    for (; view != nullptr; view = view->parent())
        if (view == ancestor) return true;
    return false;
}

std::string capture(const Fixture& fixture) {
    return ckv::golden::serialize(ckv::scene::capture(fixture.app.composed_surface(), fixture.app.current_cursor()));
}

void check_scheme_frame(Fixture& fixture, std::string_view command_key, const char* golden_path) {
    const auto command = fixture.app.commands().id_for(command_key);
    CK_CHECK(command.has_value());
    if (!command) return;
    CK_CHECK(fixture.app.commands().execute(*command));
    fixture.app.step(0);
    const std::string expected = read_file(golden_path);
    CK_CHECK(!expected.empty());
    CK_CHECK(capture(fixture) == expected);
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
    check_scheme_frame(f, "editor.scheme.dark", "golden/editor_initial_dark.dump");
    check_scheme_frame(f, "editor.scheme.light", "golden/editor_initial_light.dump");
    check_scheme_frame(f, "editor.scheme.mono", "golden/editor_initial_mono.dump");
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
    CK_CHECK(display_contains(f.term.display(), "View"));
    auto* const menu_bar = dynamic_cast<ckv::widgets::MenuBar*>(f.editor.desktop()->top_dock());
    CK_CHECK(menu_bar != nullptr);
    if (menu_bar == nullptr) return;
    // View holds the checkable Line Numbers row and a separator, then the
    // four schemes as one radio set.
    const auto& view_items = menu_bar->menus()[3].items;
    CK_CHECK(view_items.size() == 6U);
    if (view_items.size() != 6U) return;
    const std::span<const ckv::widgets::MenuItem> choices(view_items.data() + 2, 4U);
    CK_CHECK(choices[0].mark() == ckv::widgets::MenuMark::RadioOn);
    const auto dark_command = f.app.commands().id_for("editor.scheme.dark");
    CK_CHECK(dark_command.has_value());
    if (!dark_command) return;
    CK_CHECK(f.app.commands().execute(*dark_command));
    for (std::size_t index = 0; index < choices.size(); ++index)
        CK_CHECK(choices[index].mark() == (index == 1U
                     ? ckv::widgets::MenuMark::RadioOn : ckv::widgets::MenuMark::RadioOff));
    f.app.step(0);
    const std::string dark = capture(f);
    const auto light_command = f.app.commands().id_for("editor.scheme.light");
    CK_CHECK(light_command.has_value());
    if (!light_command) return;
    CK_CHECK(f.app.commands().execute(*light_command));
    for (std::size_t index = 0; index < choices.size(); ++index)
        CK_CHECK(choices[index].mark() == (index == 2U
                     ? ckv::widgets::MenuMark::RadioOn : ckv::widgets::MenuMark::RadioOff));
    f.app.step(0);
    const std::string light = capture(f);
    const auto mono_command = f.app.commands().id_for("editor.scheme.mono");
    CK_CHECK(mono_command.has_value());
    if (!mono_command) return;
    CK_CHECK(f.app.commands().execute(*mono_command));
    for (std::size_t index = 0; index < choices.size(); ++index)
        CK_CHECK(choices[index].mark() == (index == 3U
                     ? ckv::widgets::MenuMark::RadioOn : ckv::widgets::MenuMark::RadioOff));
    f.app.step(0);
    const std::string mono = capture(f);
    CK_CHECK(dark == read_file("golden/editor_search_dark.dump"));
    CK_CHECK(light == read_file("golden/editor_search_light.dump"));
    CK_CHECK(mono == read_file("golden/editor_search_mono.dump"));
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
    check_scheme_frame(f, "editor.scheme.dark", "golden/editor_close_confirm_dark.dump");
    check_scheme_frame(f, "editor.scheme.light", "golden/editor_close_confirm_light.dump");
    check_scheme_frame(f, "editor.scheme.mono", "golden/editor_close_confirm_mono.dump");
    CK_CHECK(f.app.is_modal());
    CK_CHECK(f.editor.document()->modified());
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
    f.app.step(0);
    CK_CHECK(display_contains(f.term.display(), "Editor — settings.json"));
    CK_CHECK(f.editor.open_sample("sample.sh") == ckv::widgets::EditorFileStatus::Ok);
    CK_CHECK(f.editor.editor()->profile_id() == "bash");
    CK_CHECK(f.editor.document()->text().starts_with("#!/usr/bin/env bash"));
    f.app.step(0);
    CK_CHECK(display_contains(f.term.display(), "Editor — sample.sh"));
    CK_CHECK(f.editor.open_sample("notes.txt") == ckv::widgets::EditorFileStatus::Ok);
    CK_CHECK(f.editor.editor()->profile_id() == "plain");
    f.app.step(0);
    CK_CHECK(display_contains(f.term.display(), "Editor — notes.txt"));
    CK_CHECK(f.editor.open_sample("config.yaml") == ckv::widgets::EditorFileStatus::Ok);
    CK_CHECK(f.editor.editor()->profile_id() == "yaml");
    CK_CHECK(f.editor.open_sample("settings.json") == ckv::widgets::EditorFileStatus::Ok);
    f.app.dispatch(ckv::TextEvent{"#", false});
    CK_CHECK(!f.editor.window()->close());
    f.app.step(0);
    CK_CHECK(display_contains(f.term.display(), "Save changes to settings.json?"));
}

CK_TEST(the_editor_sample_menu_confirms_save_discard_or_cancel_before_switching_a_dirty_document) {
    Fixture f;
    f.app.step(0);
    const auto json = f.app.commands().id_for("editor.open-json-sample");
    const auto bash = f.app.commands().id_for("editor.open-bash-sample");
    CK_CHECK(json.has_value());
    CK_CHECK(bash.has_value());
    if (!json || !bash) return;

    CK_CHECK(f.app.dispatch(ckv::TextEvent{"#", false}));
    CK_CHECK(f.app.execute_command(*json));
    CK_CHECK(f.app.is_modal());
    f.app.step(0);
    CK_CHECK(display_contains(f.term.display(), "Save changes to config.yaml before opening settings.json?"));
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Escape, ckv::Modifier::None, ""}}));
    f.app.step(0);
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.editor.document()->modified());
    CK_CHECK(f.editor.editor()->profile_id() == "yaml");

    CK_CHECK(f.app.execute_command(*json));
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}}));
    f.app.step(0);
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(!f.editor.document()->modified());
    CK_CHECK(f.editor.editor()->profile_id() == "json");

    CK_CHECK(f.app.dispatch(ckv::TextEvent{"#", false}));
    CK_CHECK(f.app.execute_command(*bash));
    CK_CHECK(f.app.is_modal());
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Tab, ckv::Modifier::None, ""}}));
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}}));
    f.app.step(0);
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.editor.editor()->profile_id() == "bash");
    CK_CHECK(!f.editor.document()->modified());

    CK_CHECK(f.editor.open_sample("settings.json") == ckv::widgets::EditorFileStatus::Ok);
    CK_CHECK(f.editor.document()->text().starts_with("{\n"));
    CK_CHECK(f.editor.open_sample("config.yaml") == ckv::widgets::EditorFileStatus::Ok);
    CK_CHECK(f.editor.document()->text().starts_with('#'));
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

CK_TEST(the_editor_example_replace_script_replaces_the_current_match_and_undo_redo_restore_it) {
    Fixture f;
    f.app.step(0);
    for (int index = 0; index < 4; ++index) press(f, ckv::Key::Right, ckv::Modifier::Shift);
    // Search > Replace... by its chord, with "name" selected.
    press(f, ckv::Key::Char, ckv::Modifier::Ctrl, "r");
    CK_CHECK(f.editor.replace_window() != nullptr);
    CK_CHECK(f.app.is_modal());
    CK_CHECK(display_contains(f.term.display(), "Replace with:"));
    auto* const find = dynamic_cast<ckv::widgets::InputLine*>(f.app.focused());
    CK_CHECK(find != nullptr);
    if (find == nullptr) return;
    CK_CHECK(find->text() == "name");

    press(f, ckv::Key::Tab);
    type(f, "title");
    // The first Replace only finds: the reader sees the match before it changes.
    press(f, ckv::Key::Enter);
    CK_CHECK(f.editor.document()->text().starts_with("name: ckVision"));
    CK_CHECK(f.editor.editor()->selection().has_value());
    CK_CHECK(f.editor.document()->text(*f.editor.editor()->selection()) == "name");
    press(f, ckv::Key::Enter);
    CK_CHECK(f.editor.document()->text().starts_with("title: ckVision"));
    CK_CHECK(f.editor.document()->modified());
    CK_CHECK(f.editor.replace_window() != nullptr);
    // That was the only match, and the press says there is no next one.
    CK_CHECK(display_contains(f.term.display(), "\"name\" does not occur in config.yaml."));
    CK_CHECK(!is_within(f.app.focused(), f.editor.replace_window()));
    press(f, ckv::Key::Enter);
    f.app.step(0);
    CK_CHECK(!display_contains(f.term.display(), "does not occur"));
    CK_CHECK(dynamic_cast<ckv::widgets::InputLine*>(f.app.focused()) != nullptr);
    CK_CHECK(is_within(f.app.focused(), f.editor.replace_window()));

    press(f, ckv::Key::Escape);
    f.app.step(0);
    CK_CHECK(f.editor.replace_window() == nullptr);
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.app.focused() == f.editor.editor());

    press(f, ckv::Key::Char, ckv::Modifier::Ctrl, "z");
    CK_CHECK(f.editor.document()->text().starts_with("name: ckVision"));
    CK_CHECK(!f.editor.document()->can_undo());
    press(f, ckv::Key::Char, ckv::Modifier::Ctrl, "y");
    CK_CHECK(f.editor.document()->text().starts_with("title: ckVision"));
    CK_CHECK(display_contains(f.term.display(), "title: ckVision"));
}

CK_TEST(the_editor_example_replace_all_asks_from_inside_the_replace_dialog_and_undoes_in_one_step) {
    Fixture f;
    f.app.step(0);
    const std::string original = f.editor.document()->text();
    press(f, ckv::Key::Char, ckv::Modifier::Ctrl, "r");
    ckv::widgets::Window* const dialog = f.editor.replace_window();
    CK_CHECK(dialog != nullptr);
    if (dialog == nullptr) return;
    type(f, "able");
    press(f, ckv::Key::Tab);
    type(f, "ABLE");
    auto* const replacement = dynamic_cast<ckv::widgets::InputLine*>(f.app.focused());
    CK_CHECK(replacement != nullptr);
    if (replacement == nullptr) return;

    // Replace All by its mnemonic: a confirmation, modal on top of the modal
    // Replace dialog, stating what it is about to do.
    press(f, ckv::Key::Char, ckv::Modifier::Alt, "a");
    CK_CHECK(display_contains(f.term.display(), "Replace 2 matches of \"able\" with \"ABLE\"?"));
    CK_CHECK(f.app.focused() != nullptr);
    CK_CHECK(!is_within(f.app.focused(), dialog));
    const ckv::ui::View* const confirmation_focus = f.app.focused();

    // Events are scoped to the innermost modal. Typing reaches neither the
    // Replace dialog's field nor the document; a background accelerator
    // (Alt+X, Quit) is not honoured; a click on the Replace dialog's field
    // is not delivered and moves no focus.
    type(f, "zz");
    press(f, ckv::Key::Char, ckv::Modifier::Alt, "x");
    CK_CHECK(!f.app.quit_requested());
    const Point replacement_cell{replacement->absolute_bounds().x + 1, replacement->absolute_bounds().y};
    mouse(f, ckv::MouseAction::Down, ckv::MouseButton::Left, replacement_cell);
    mouse(f, ckv::MouseAction::Up, ckv::MouseButton::Left, replacement_cell);
    CK_CHECK(f.app.focused() == confirmation_focus);
    CK_CHECK(replacement->text() == "ABLE");
    CK_CHECK(f.editor.document()->text() == original);

    // Esc answers No and closes only the confirmation. The Replace dialog is
    // still modal, and the focus is back where it was when the question was
    // asked: the field the mnemonic was pressed in.
    press(f, ckv::Key::Escape);
    f.app.step(0);
    CK_CHECK(f.editor.replace_window() == dialog);
    CK_CHECK(f.app.is_modal());
    CK_CHECK(f.editor.document()->text() == original);
    CK_CHECK(f.app.focused() == replacement);

    // Asked again, this time from the button itself, and answered Yes (the
    // confirmation's default button): every match goes.
    press(f, ckv::Key::Tab);
    press(f, ckv::Key::Tab);
    press(f, ckv::Key::Tab);
    press(f, ckv::Key::Tab);
    press(f, ckv::Key::Tab);
    auto* const replace_all = dynamic_cast<ckv::widgets::Button*>(f.app.focused());
    CK_CHECK(replace_all != nullptr && replace_all->text() == "Replace &All");
    press(f, ckv::Key::Enter);
    CK_CHECK(display_contains(f.term.display(), "Replace 2 matches"));
    CK_CHECK(!is_within(f.app.focused(), dialog));
    press(f, ckv::Key::Enter);
    f.app.step(0);
    CK_CHECK(f.editor.document()->text().find("enABLEd: true") != std::string::npos);
    CK_CHECK(f.editor.document()->text().find("stABLE editor") != std::string::npos);
    CK_CHECK(f.editor.document()->text().find("able") == std::string::npos);
    CK_CHECK(f.editor.replace_window() == dialog);
    CK_CHECK(f.app.focused() == replace_all);

    // Esc closes the Replace dialog, and the focus returns one level further,
    // to the editor.
    press(f, ckv::Key::Escape);
    f.app.step(0);
    CK_CHECK(f.editor.replace_window() == nullptr);
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.app.focused() == f.editor.editor());

    // One transaction: one Undo takes both replacements back, one Redo
    // replays both.
    press(f, ckv::Key::Char, ckv::Modifier::Ctrl, "z");
    CK_CHECK(f.editor.document()->text() == original);
    CK_CHECK(!f.editor.document()->can_undo());
    press(f, ckv::Key::Char, ckv::Modifier::Ctrl, "y");
    CK_CHECK(f.editor.document()->text().find("enABLEd: true") != std::string::npos);
    CK_CHECK(f.editor.document()->text().find("stABLE editor") != std::string::npos);
}

CK_TEST(the_editor_example_replace_dialog_matches_its_pinned_visual_contract) {
    Fixture f;
    f.app.step(0);
    for (int index = 0; index < 4; ++index) press(f, ckv::Key::Right, ckv::Modifier::Shift);
    press(f, ckv::Key::Char, ckv::Modifier::Ctrl, "r");
    press(f, ckv::Key::Tab);
    type(f, "title");
    press(f, ckv::Key::Enter);
    CK_CHECK(capture(f) == read_file("golden/editor_replace.dump"));
    check_scheme_frame(f, "editor.scheme.dark", "golden/editor_replace_dark.dump");
    check_scheme_frame(f, "editor.scheme.light", "golden/editor_replace_light.dump");
    check_scheme_frame(f, "editor.scheme.mono", "golden/editor_replace_mono.dump");
    CK_CHECK(f.editor.replace_window() != nullptr);
}

CK_TEST(the_editor_example_mouse_wheel_scrolls_the_text_without_moving_the_caret) {
    Fixture f;
    f.app.step(0);
    // A document longer than the window, pasted the way a terminal delivers a
    // paste, then the caret back at the top.
    std::string lines;
    for (int line = 1; line <= 30; ++line) lines += "line " + std::string(line < 10 ? "0" : "") + std::to_string(line) + "\n";
    f.term.inject_event(ckv::TextEvent{lines, true});
    f.app.step(0);
    press(f, ckv::Key::Home, ckv::Modifier::Ctrl);
    const ckv::Rect text = f.editor.editor()->absolute_bounds();
    CK_CHECK(screen_row(f.term.display(), text.y).find("line 01") != std::string::npos);

    const Point over_text{text.x + text.width / 2, text.y + 4};
    // Each notch scrolls ui::kWheelRows rows, as it does over every view.
    for (int notch = 0; notch < 3; ++notch) mouse(f, ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown, over_text);
    CK_CHECK(screen_row(f.term.display(), text.y).find("line 10") != std::string::npos);
    CK_CHECK(!display_contains(f.term.display(), "line 01"));
    CK_CHECK(f.editor.editor()->status().line == 1U);
    CK_CHECK(display_contains(f.term.display(), "Ln 1, Col 1"));

    mouse(f, ckv::MouseAction::Wheel, ckv::MouseButton::WheelUp, over_text);
    CK_CHECK(screen_row(f.term.display(), text.y).find("line 07") != std::string::npos);
}

CK_TEST(the_editor_example_double_click_selects_the_clicked_word) {
    Fixture f;
    f.app.step(0);
    const auto word = find_on_screen(f.term.display(), "ckVision");
    CK_CHECK(word.has_value());
    if (!word) return;
    // Terminals report presses and releases, never double clicks: the
    // Application counts two presses on one cell within its double-click
    // interval, on its injected clock, as one.
    const Point at{word->x + 3, word->y};
    const auto click = [&f, at] {
        mouse(f, ckv::MouseAction::Down, ckv::MouseButton::Left, at);
        mouse(f, ckv::MouseAction::Up, ckv::MouseButton::Left, at);
    };
    click();
    f.clock.advance(ckv::ui::kDoubleClickIntervalNanos + 1);
    click();
    // Too far apart: two single clicks, which only place the caret.
    CK_CHECK(!f.editor.editor()->selection().has_value());
    f.clock.advance(150'000'000);
    click();
    CK_CHECK(f.editor.editor()->selection().has_value());
    CK_CHECK(f.editor.document()->text(*f.editor.editor()->selection()) == "ckVision");
    press(f, ckv::Key::Char, ckv::Modifier::Ctrl, "c");
    CK_CHECK(f.app.clipboard_text() == "ckVision");
}

CK_TEST(the_editor_example_drag_selects_text_with_the_mouse) {
    Fixture f;
    f.app.step(0);
    const auto from = find_on_screen(f.term.display(), "version");
    const auto to = find_on_screen(f.term.display(), "0.1");
    CK_CHECK(from.has_value() && to.has_value());
    if (!from || !to) return;
    mouse(f, ckv::MouseAction::Down, ckv::MouseButton::Left, *from);
    mouse(f, ckv::MouseAction::Move, ckv::MouseButton::Left, Point{to->x + 1, to->y});
    mouse(f, ckv::MouseAction::Move, ckv::MouseButton::Left, Point{to->x + 3, to->y});
    mouse(f, ckv::MouseAction::Up, ckv::MouseButton::Left, Point{to->x + 3, to->y});
    CK_CHECK(f.editor.editor()->selection().has_value());
    CK_CHECK(f.editor.document()->text(*f.editor.editor()->selection()) == "version: 0.1");
    // The drag left the caret at its end, and the frame says where that is.
    CK_CHECK(display_contains(f.term.display(), "Ln 2, Col 13"));
}

CK_TEST(the_editor_example_window_resizes_by_its_grip_and_the_text_reflows) {
    Fixture f;
    f.app.step(0);
    ckv::widgets::Window* const window = f.editor.window();
    const ckv::Rect before = window->bounds();
    const int editor_width = f.editor.editor()->bounds().width;
    const Point grip{before.x + before.width - 1, before.y + before.height - 1};
    mouse(f, ckv::MouseAction::Down, ckv::MouseButton::Left, grip);
    mouse(f, ckv::MouseAction::Move, ckv::MouseButton::Left, Point{grip.x - 20, grip.y - 4});
    mouse(f, ckv::MouseAction::Up, ckv::MouseButton::Left, Point{grip.x - 20, grip.y - 4});
    CK_CHECK(window->bounds() == (ckv::Rect{before.x, before.y, before.width - 20, before.height - 4}));
    CK_CHECK(f.editor.editor()->bounds().width == editor_width - 20);
    // Word wrap follows the narrower viewport, and the position overlay
    // stays on the moved bottom edge.
    const auto status = find_on_screen(f.term.display(), "Ln 1, Col 1");
    CK_CHECK(status.has_value() && status->y == before.y + before.height - 5);
    CK_CHECK(display_contains(f.term.display(), "description: This is a"));
}
