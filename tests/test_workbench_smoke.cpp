// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/flow_view.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/key_chord_capture.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/memo.hpp"
#include "cvision/widgets/progress.hpp"
#include "cvision/widgets/tab_control.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/text_view.hpp"
#include "cvision/widgets/tree_view.hpp"
#include "cvision/widgets/window.hpp"
#include "workbench_app.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Point;
using ckv::ui::Application;

namespace {
struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    ckv::workbench::WorkbenchApp workbench{app};
};
}  // namespace

CK_TEST(workbench_about_dialog_carries_the_project_copyright) {
    Fixture f;
    CK_CHECK(f.app.execute_command(f.app.commands().standard().help));
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find(
                 "Copyright (c) 2026 C. Klukas. All rights reserved.") != std::string::npos);
}

CK_TEST(workbench_example_renders_text_page_and_chrome) {
    Fixture f;
    f.app.step(0);
    const auto bytes = f.term.written_bytes();
    CK_CHECK(bytes.find("Workbench") != std::string::npos);
    CK_CHECK(bytes.find("Text") != std::string::npos);
    CK_CHECK(bytes.find("Quit") != std::string::npos);
}

CK_TEST(workbench_text_page_exposes_memo_history_and_links) {
    Fixture f;
    CK_CHECK(f.workbench.memo()->wrap_mode() == ckv::widgets::WrapMode::Word);
    CK_CHECK(f.workbench.command_input()->text() == "test");
    CK_CHECK(f.workbench.text_view()->link_count() == 1);

    CK_CHECK(f.workbench.text_view()->activate_current_link());
    CK_CHECK(f.workbench.last_link() == std::string{"https://example.invalid/osc8"});
    CK_CHECK(f.workbench.text_view()->osc8_text().find("https://example.invalid/osc8") != std::string::npos);
    CK_CHECK(f.workbench.flow_view()->link_count() == 1);
    CK_CHECK(f.workbench.flow_view()->activate_current_link());
    CK_CHECK(f.workbench.last_link() == std::string{"https://example.invalid/flow"});
}

CK_TEST(workbench_data_page_contains_table_tree_list_combo_and_progress) {
    Fixture f;
    f.workbench.tabs()->set_active_index(1);
    f.app.step(0);

    CK_CHECK(f.workbench.tabs()->active_index() == 1);
    CK_CHECK(f.workbench.tree()->selected()->label == "Project");
    CK_CHECK(f.workbench.list()->selected_indices().size() == 1);
    CK_CHECK(f.workbench.combo()->selected_index() == std::optional<std::size_t>{1});
    CK_CHECK(f.workbench.progress()->fraction() > 0.6);
    CK_CHECK(f.workbench.search_box()->query() == "alpha");
    CK_CHECK(f.workbench.breadcrumb()->segments().size() == 3);

    f.workbench.table()->sort_by(1, true);
    CK_CHECK(f.workbench.table()->sort_column() == 1);
}

CK_TEST(workbench_help_page_contains_command_and_utility_components) {
    Fixture f;
    f.workbench.tabs()->set_active_index(2);
    f.app.step(0);

    // The palette lists what declares itself browsable, so the workbench's
    // own command is what it highlights -- named by key, never by number.
    CK_CHECK(f.workbench.command_palette()->highlighted_command() ==
             f.app.commands().id_for("workbench.build-project"));
    CK_CHECK(f.workbench.property_inspector()->items().size() == 2);
    CK_CHECK(f.workbench.notifications()->notifications().size() == 1);
    CK_CHECK(f.workbench.tooltip()->shown());
}

// --- Overlapping windows and runtime rebinding, as a reader drives them ----
//
// Application-level scripts: every key and pointer report enters through the
// HeadlessTerminal and is delivered by Application::step.

namespace {

struct Script {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    ckv::workbench::WorkbenchApp workbench{app};

    Script() { app.step(0); }

    void key(KeyChord chord) {
        term.inject_event(ckv::KeyEvent{std::move(chord)});
        app.step(0);
    }
    void bytes(std::string_view raw) {
        term.inject_bytes(raw, 0);
        app.step(0);
    }
    void mouse(ckv::MouseAction action, Point cell) {
        term.inject_event(ckv::MouseEvent{action, ckv::MouseButton::Left, cell, std::nullopt, Modifier::None});
    }
    void click(Point cell) {
        mouse(ckv::MouseAction::Down, cell);
        mouse(ckv::MouseAction::Up, cell);
        app.step(0);
    }
    void drag(Point from, Point to) {
        mouse(ckv::MouseAction::Down, from);
        mouse(ckv::MouseAction::Move, to);  // held: a drag
        mouse(ckv::MouseAction::Up, to);
        app.step(0);
    }
    std::string row(int y) const {
        std::string text;
        const ckv::FrameView frame = term.display().frame();
        for (int x = 0; x < frame.size().width; ++x) text += frame.at(Point{x, y}).grapheme();
        return text;
    }
    // The column, in cells, where `needle` starts on row `y`, or -1.
    int column_of(int y, std::string_view needle) const {
        const ckv::FrameView frame = term.display().frame();
        for (int x = 0; x < frame.size().width; ++x) {
            std::string tail;
            for (int k = x; k < frame.size().width && tail.size() < needle.size(); ++k)
                tail += frame.at(Point{k, y}).grapheme();
            if (tail.rfind(needle, 0) == 0) return x;
        }
        return -1;
    }
    // Opens the console through the Window menu, by keyboard.
    void open_console() {
        key(KeyChord{Key::Char, Modifier::Alt, "w"});
        key(KeyChord{Key::Char, Modifier::None, "o"});
    }
    ckv::widgets::Window* active() { return workbench.desktop().active_window(); }
};

}  // namespace

CK_TEST(workbench_window_menu_opens_a_console_that_overrides_the_classic_scheme_with_the_dark_one) {
    Script s;
    CK_CHECK(s.workbench.desktop().windows().size() == 1);
    s.open_console();
    CK_CHECK(s.workbench.desktop().windows().size() == 2);
    CK_CHECK(s.workbench.console_window() != nullptr);
    CK_CHECK(s.active() == s.workbench.console_window());
    CK_CHECK(s.row(s.workbench.console_window()->bounds().y).find("Console") != std::string::npos);

    // The override re-themes the console's frame: active, it is drawn in the
    // dark scheme's frame colours, where the workbench window, active in its
    // turn, keeps the application's classic ones.
    const ckv::Rect console = s.workbench.console_window()->bounds();
    const ckv::Rect main = s.workbench.window()->bounds();
    const ckv::Style console_frame = s.term.display().frame().at(Point{console.x, console.y}).style();
    s.key(KeyChord{Key::Char, Modifier::Alt, "1"});
    CK_CHECK(s.active() == s.workbench.window());
    const ckv::Style main_frame = s.term.display().frame().at(Point{main.x, main.y}).style();
    CK_CHECK(console_frame.bg != main_frame.bg || console_frame.fg != main_frame.fg);
}

CK_TEST(workbench_alt_digits_select_windows_by_number) {
    Script s;
    s.open_console();
    CK_CHECK(s.active() == s.workbench.console_window());
    s.bytes("\x1B" "1");  // Alt+1, as a terminal sends it
    CK_CHECK(s.active() == s.workbench.window());
    s.bytes("\x1B" "2");
    CK_CHECK(s.active() == s.workbench.console_window());
    s.key(KeyChord{Key::Char, Modifier::Alt, "1"});
    CK_CHECK(s.active() == s.workbench.window());
}

CK_TEST(workbench_window_list_selects_a_window_by_keyboard) {
    Script s;
    s.open_console();
    CK_CHECK(s.active() == s.workbench.console_window());
    s.key(KeyChord{Key::Char, Modifier::Alt, "w"});
    s.key(KeyChord{Key::Char, Modifier::None, "w"});  // Window List
    CK_CHECK(s.app.is_modal());
    s.key(KeyChord{Key::Home, Modifier::None, ""});  // the first window: Workbench
    s.key(KeyChord{Key::Enter, Modifier::None, ""});
    CK_CHECK(!s.app.is_modal());
    CK_CHECK(s.active() == s.workbench.window());
}

CK_TEST(workbench_zoom_by_keyboard_and_by_the_zoom_glyph_toggles_the_active_window) {
    Script s;
    s.open_console();
    ckv::widgets::Window* console = s.workbench.console_window();
    const ckv::Rect restored = console->bounds();

    s.key(KeyChord{Key::F5, Modifier::None, ""});
    CK_CHECK(console->zoomed());
    CK_CHECK(console->bounds() == s.workbench.desktop().content_area());
    s.key(KeyChord{Key::F5, Modifier::None, ""});
    CK_CHECK(!console->zoomed());
    CK_CHECK(console->bounds() == restored);

    const int glyph = s.column_of(restored.y, "[↑]");
    CK_CHECK(glyph > restored.x);
    s.click(Point{glyph + 1, restored.y});
    CK_CHECK(console->zoomed());
    const ckv::Rect zoomed = console->bounds();
    const int restore_glyph = s.column_of(zoomed.y, "[↕]");  // zoomed, the control restores
    CK_CHECK(restore_glyph > zoomed.x);
    s.click(Point{restore_glyph + 1, zoomed.y});
    CK_CHECK(!console->zoomed());
    CK_CHECK(console->bounds() == restored);
}

CK_TEST(workbench_grip_drag_resizes_and_the_close_glyph_closes_the_console) {
    Script s;
    s.open_console();
    ckv::widgets::Window* console = s.workbench.console_window();
    const ckv::Rect before = console->bounds();
    const Point grip{before.x + before.width - 1, before.y + before.height - 1};
    s.drag(grip, Point{grip.x - 6, grip.y - 3});
    CK_CHECK(console->bounds().x == before.x);
    CK_CHECK(console->bounds().y == before.y);
    CK_CHECK(console->bounds().width == before.width - 6);
    CK_CHECK(console->bounds().height == before.height - 3);

    const int close_glyph = s.column_of(before.y, "[■]");
    CK_CHECK(close_glyph >= before.x);
    s.click(Point{close_glyph + 1, before.y});
    s.app.step(0);
    CK_CHECK(s.workbench.console_window() == nullptr);
    CK_CHECK(s.workbench.desktop().windows().size() == 1);
    CK_CHECK(s.active() == s.workbench.window());
}

CK_TEST(workbench_keys_dialog_rebinds_a_command_and_the_new_chord_runs_it) {
    Script s;
    CK_CHECK(s.app.commands().chord_text(s.workbench.build_command()) == "F7");
    s.key(KeyChord{Key::F7, Modifier::None, ""});
    CK_CHECK(s.workbench.builds_run() == 1);

    s.key(KeyChord{Key::Char, Modifier::Alt, "f"});
    s.key(KeyChord{Key::Char, Modifier::None, "k"});  // File > Keys...
    CK_CHECK(s.workbench.keys_dialog() != nullptr);
    CK_CHECK(s.app.is_modal());
    auto* capture = dynamic_cast<ckv::widgets::KeyChordCapture*>(s.app.focused());
    CK_CHECK(capture != nullptr);
    CK_CHECK(capture->chord() == KeyChord::parse("F7"));

    s.key(KeyChord{Key::Enter, Modifier::None, ""});  // start capturing
    CK_CHECK(capture->capturing());
    s.key(KeyChord{Key::Char, Modifier::Ctrl, "b"});  // the new binding
    CK_CHECK(!capture->capturing());
    CK_CHECK(capture->chord() == KeyChord::parse("Ctrl+B"));
    CK_CHECK(s.app.commands().chord_text(s.workbench.build_command()) == "Ctrl+B");
    CK_CHECK(s.workbench.builds_run() == 1);  // captured, not run

    s.key(KeyChord{Key::Escape, Modifier::None, ""});
    s.app.step(0);
    CK_CHECK(s.workbench.keys_dialog() == nullptr);
    CK_CHECK(!s.app.is_modal());

    s.key(KeyChord{Key::F7, Modifier::None, ""});  // the old chord no longer builds
    CK_CHECK(s.workbench.builds_run() == 1);
    s.key(KeyChord{Key::Char, Modifier::Ctrl, "b"});
    CK_CHECK(s.workbench.builds_run() == 2);
}

CK_TEST(workbench_keys_dialog_labels_focus_their_capture_by_mnemonic) {
    Script s;
    s.key(KeyChord{Key::Char, Modifier::Alt, "f"});
    s.key(KeyChord{Key::Char, Modifier::None, "k"});
    auto* first = dynamic_cast<ckv::widgets::KeyChordCapture*>(s.app.focused());
    CK_CHECK(first != nullptr);
    s.key(KeyChord{Key::Char, Modifier::Alt, "z"});  // &Zoom
    auto* zoom = dynamic_cast<ckv::widgets::KeyChordCapture*>(s.app.focused());
    CK_CHECK(zoom != nullptr);
    CK_CHECK(zoom != first);
    CK_CHECK(zoom->chord() == KeyChord::parse("F5"));
}
