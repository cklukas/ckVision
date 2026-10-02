// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/filesystem.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/theme_format.hpp"
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
    // The link is painted with its target, so a terminal that renders
    // hyperlinks makes it clickable (D-088).
    f.app.step(0);
    const ckv::FrameView frame = f.app.current_frame();
    int linked = 0;
    for (int y = 0; y < frame.size().height; ++y)
        for (int x = 0; x < frame.size().width; ++x)
            if (frame.link_target(ckv::Point{x, y}) == "https://example.invalid/osc8") ++linked;
    CK_CHECK(linked == 5);  // the five cells of "OSC 8"
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

CK_TEST(workbench_the_tooltip_key_explains_the_focused_help_page_control) {
    Fixture f;
    f.workbench.tabs()->set_active_index(2);
    f.app.set_focus(f.workbench.property_inspector());
    f.app.step(0);
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F1, ckv::Modifier::Ctrl, ""}}));
    f.app.step(0);
    const ckv::widgets::Tooltip* tip = f.workbench.tips().tooltip();
    CK_CHECK(tip != nullptr && tip->text() == "Enter edits the value on the cursor row");
    CK_CHECK(f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Escape, ckv::Modifier::None, ""}}));
    f.app.step(0);
    CK_CHECK(f.workbench.tips().tooltip() == nullptr);
    CK_CHECK(f.app.focused() == f.workbench.property_inspector());
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

// --- The theme editor, from the View menu to the saved file ----------------

namespace {

constexpr std::string_view kThemePath = "/home/reader/.ckvision/workbench.theme";

// The Workbench over a memory file system that holds the reader's home
// directory, driven through the terminal as Script drives it.
struct ThemeScript {
    ckv::MemoryFileSystem files;
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    std::optional<ckv::workbench::WorkbenchApp> workbench;

    explicit ThemeScript(std::optional<std::string> saved = std::nullopt) {
        files.add_directory("/home/reader");
        if (saved) {
            files.add_directory("/home/reader/.ckvision");
            files.add_file(kThemePath, std::move(*saved));
        }
        workbench.emplace(app, ckv::workbench::WorkbenchThemeFile{&files, std::string(kThemePath)});
        app.step(0);
    }

    void key(KeyChord chord) {
        term.inject_event(ckv::KeyEvent{std::move(chord)});
        app.step(0);
    }
    void type(std::string_view text) {
        for (const char c : text) term.inject_event(ckv::KeyEvent{KeyChord{Key::Char, Modifier::None, std::string(1, c)}});
        app.step(0);
    }
    ckv::Style displayed(Point cell) const { return term.display().frame().at(cell).style(); }
};

// A cell of bare desktop: left of the Workbench window, and left of the
// theme editor, which fills the rest of the width while it is open.
constexpr Point kDesktopCell{0, 10};

}  // namespace

CK_TEST(workbench_edit_theme_changes_one_role_repaints_the_desktop_and_saves_the_theme) {
    ThemeScript s;
    const ckv::ui::RoleId desktop_background = s.app.roles().find("ckv.desktop.background");
    const ckv::Style classic = s.displayed(kDesktopCell);
    CK_CHECK(classic.bg != ckv::Color::indexed(22));

    s.key(KeyChord{Key::Char, Modifier::Alt, "v"});
    s.key(KeyChord{Key::Char, Modifier::None, "t"});  // View > Edit theme...
    CK_CHECK(s.app.is_modal());
    CK_CHECK(dynamic_cast<ckv::widgets::Table*>(s.app.focused()) != nullptr);

    // The role table lists the roles by name; walk down to the desktop's.
    std::vector<std::string> names;
    for (std::size_t role = 0; role < s.app.roles().size(); ++role)
        names.push_back(s.app.roles().name(static_cast<ckv::ui::RoleId>(role)));
    std::sort(names.begin(), names.end());
    const auto row = std::find(names.begin(), names.end(), "ckv.desktop.background") - names.begin();
    for (std::ptrdiff_t step = 0; step < row; ++step) s.key(KeyChord{Key::Down, Modifier::None, ""});
    for (int tab = 0; tab < 3; ++tab) s.key(KeyChord{Key::Tab, Modifier::None, ""});  // the background's kind
    s.key(KeyChord{Key::Left, Modifier::None, ""});  // RGB -> Palette
    s.key(KeyChord{Key::Tab, Modifier::None, ""});   // its value, selected on arrival
    s.type("22");
    // Only the preview has changed so far: the desktop keeps its colour.
    CK_CHECK(s.displayed(kDesktopCell) == classic);
    s.key(KeyChord{Key::Enter, Modifier::None, ""});

    CK_CHECK(!s.app.is_modal());
    CK_CHECK(s.app.theme().resolve(desktop_background).bg == ckv::Color::indexed(22));
    // The terminal shows it: the accepted theme repainted the frame.
    CK_CHECK(s.displayed(kDesktopCell).bg == ckv::Color::indexed(22));
    const std::optional<ckv::FileReadResult> saved = s.files.read_file(kThemePath);
    CK_CHECK(saved.has_value());
    if (saved) CK_CHECK(saved->contents == ckv::ui::serialize_theme(s.app.theme()));

    // The next session starts with the saved theme.
    ThemeScript next(saved ? std::optional<std::string>{saved->contents} : std::nullopt);
    CK_CHECK(next.app.theme().resolve(desktop_background).bg == ckv::Color::indexed(22));
    CK_CHECK(next.displayed(kDesktopCell).bg == ckv::Color::indexed(22));
    CK_CHECK(!next.app.is_modal());
}

CK_TEST(workbench_cancelled_theme_editor_changes_and_saves_nothing) {
    ThemeScript s;
    const ckv::ui::Theme before = s.app.theme();
    s.key(KeyChord{Key::Char, Modifier::Alt, "v"});
    s.key(KeyChord{Key::Char, Modifier::None, "t"});
    for (int tab = 0; tab < 5; ++tab) s.key(KeyChord{Key::Tab, Modifier::None, ""});  // the attributes
    s.type(" ");
    s.key(KeyChord{Key::Escape, Modifier::None, ""});
    CK_CHECK(!s.app.is_modal());
    CK_CHECK(ckv::ui::serialize_theme(s.app.theme()) == ckv::ui::serialize_theme(before));
    CK_CHECK(!s.files.exists(kThemePath));
}

CK_TEST(workbench_reports_an_unreadable_theme_file_and_keeps_the_classic_theme) {
    ThemeScript s(std::string("ckvision-theme 1\nckv.desktop.background fg @1 bg @2 attrs blink\n"));
    CK_CHECK(s.app.is_modal());
    bool reported = false;
    for (int y = 0; y < 24 && !reported; ++y) {
        std::string row;
        for (int x = 0; x < 80; ++x) row += s.term.display().frame().at(Point{x, y}).grapheme();
        reported = row.find("not be read") != std::string::npos;
    }
    CK_CHECK(reported);
    const ckv::ui::RoleId desktop_background = s.app.roles().find("ckv.desktop.background");
    ckv::ui::RoleRegistry registry;
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    CK_CHECK(s.app.theme().resolve(desktop_background) ==
             ckv::ui::make_classic_theme(registry, roles).resolve(roles.desktop_background));
}

CK_TEST(workbench_tab_presentation_commands_keep_every_page_control_inside_the_page) {
    Fixture f;
    auto* tabs = f.workbench.tabs();
    for (const char* key : {"workbench.tabs-underlined", "workbench.tabs-framed", "workbench.tabs-compact"}) {
        const auto command = f.app.commands().id_for(key);
        CK_CHECK(command.has_value());
        if (!command) continue;
        CK_CHECK(f.app.execute_command(*command));
        for (std::size_t i = 0; i < tabs->tab_count(); ++i) {
            tabs->set_active_index(i);
            f.app.step(0);
            const auto* page = tabs->active_page();
            for (const auto& child : page->children()) {
                const auto rect = child->bounds();
                CK_CHECK(rect.x >= 0 && rect.right() <= page->bounds().width);
                CK_CHECK(rect.y >= 0 && rect.bottom() <= page->bounds().height);
            }
        }
    }
}

CK_TEST(workbench_toolbar_presentations_fit_all_tab_presentations_without_overlapping_the_editor) {
    Fixture f;
    for (const char* tab : {"workbench.tabs-underlined", "workbench.tabs-framed", "workbench.tabs-compact"}) {
        CK_CHECK(f.app.execute_command(*f.app.commands().id_for(tab)));
        for (const char* toolbar : {"workbench.toolbar-compact", "workbench.toolbar-padded", "workbench.toolbar-framed"}) {
            CK_CHECK(f.app.execute_command(*f.app.commands().id_for(toolbar)));
            f.workbench.tabs()->set_active_index(0);
            const auto* page = f.workbench.tabs()->active_page();
            const auto bar = f.workbench.tool_bar()->bounds();
            CK_CHECK(bar.bottom() <= page->bounds().height);
            CK_CHECK(bar.y >= f.workbench.command_input()->bounds().bottom());
            CK_CHECK(f.workbench.memo()->bounds().bottom() <= f.workbench.command_input()->bounds().y);
            CK_CHECK(bar.height == f.workbench.tool_bar()->vertical_size_hint().preferred);
        }
    }
}
