// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Appearance specimens: chrome — the desktop, the window frame and what is
// set into it, the bars an application docks against the screen edges, the
// transient surfaces that come and go over them, the standard dialogs, and
// the application's own refusal to draw on a terminal too small to hold it.
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "appearance_matrix.hpp"
#include "cvision/core/filesystem.hpp"
#include "cvision/core/text.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/date_time_dialog.hpp"
#include "cvision/widgets/dialog.hpp"
#include "cvision/widgets/directory_picker.hpp"
#include "cvision/widgets/editor_window.hpp"
#include "cvision/widgets/file_dialog.hpp"
#include "cvision/widgets/frame_text.hpp"
#include "cvision/widgets/help_viewer.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/minimized_window_stub.hpp"
#include "cvision/widgets/paged_strip.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/terminal_report_dialog.hpp"
#include "cvision/widgets/theme_editor.hpp"
#include "cvision/widgets/window_list_dialog.hpp"
#include "cvision/widgets/window_switcher_bar.hpp"

namespace ckv::docgen::appearance {

namespace {

// Most chrome specimens stand on one small desktop; a standard dialog needs
// the room its own layout asks for.
constexpr Size kScreen{40, 12};
constexpr Size kDialogScreen{64, 20};

MouseEvent press(Point cell, MouseButton button = MouseButton::Left) {
    return MouseEvent{MouseAction::Down, button, cell, std::nullopt};
}

MouseEvent drag(Point cell) { return MouseEvent{MouseAction::Move, MouseButton::Left, cell, std::nullopt}; }

KeyEvent key(Key which) { return KeyEvent{KeyChord{which, Modifier::None, ""}}; }

KeyEvent typed(std::string text) { return KeyEvent{KeyChord{Key::Char, Modifier::None, std::move(text)}}; }

// Alt with a letter: a label's mnemonic, which reaches the control it names.
KeyEvent alt(std::string letter) { return KeyEvent{KeyChord{Key::Char, Modifier::Alt, std::move(letter)}}; }

// A plain document window with nothing in it, returned as the frame rather
// than its content: chrome specimens are about the frame.
widgets::Window& frame(Stage& stage, Rect bounds, std::string title) {
    return stage.window(bounds, std::move(title), std::make_unique<ui::View>(), false);
}

// A second window, put over part of the specimen and activated, so the
// specimen is seen the way every window but one on a desktop is seen.
void cover(Stage& stage, Rect bounds) { stage.desktop().activate(&frame(stage, bounds, "Build log")); }

widgets::MessageBoxDescriptor message(widgets::MessageBoxKind kind, std::string title, std::string text,
                                      widgets::MessageBoxButtons buttons = widgets::MessageBoxButtons::Ok) {
    return widgets::MessageBoxDescriptor{kind, std::move(title), std::move(text), buttons};
}

// Presents modally. The presentation is dropped at once: a specimen has no
// use for the answer, and dropping it declines only the completion.
void present(Stage& stage, const widgets::MessageBoxDescriptor& descriptor) {
    static_cast<void>(widgets::present_modal_message_box(stage.app(), stage.desktop(), stage.roles(), descriptor));
}

widgets::MessageBoxDescriptor saved() { return message(widgets::MessageBoxKind::Info, "Saved", "notes.md was saved."); }

// --- Commands --------------------------------------------------------------

// The commands the menu and status-line specimens present, declared as an
// application declares its own. Print is unavailable through its own
// predicate, which is the only way a command row greys.
struct Commands {
    ui::CommandId open = ui::kInvalidCommand;
    ui::CommandId save = ui::kInvalidCommand;
    ui::CommandId print = ui::kInvalidCommand;
};

Commands declare_commands(ui::Application& app) {
    ui::CommandRegistry& registry = app.commands();
    Commands ids;
    ids.open = registry.declare(
        {.key = "doc.open", .title = "&Open...", .category = "File", .chord = "Ctrl+O", .handler = [] {}});
    ids.save = registry.declare(
        {.key = "doc.save", .title = "&Save", .category = "File", .chord = "Ctrl+S", .handler = [] {}});
    ids.print = registry.declare(
        {.key = "doc.print", .title = "&Print", .category = "File", .chord = "Ctrl+P", .handler = [] {}});
    registry.set_enabled_predicate(ids.print, [] { return false; });
    return ids;
}

// Every kind of menu row: commands with chords, a separator, an unavailable
// command, a submenu, check and radio marks, and an unavailable action.
std::vector<widgets::MenuBarItem> menus(const Commands& ids, std::string file_title = "&File") {
    using widgets::MenuItem;
    using widgets::MenuMark;
    return {
        widgets::MenuBarItem{std::move(file_title),
                             {MenuItem::command(ids.open), MenuItem::command(ids.save), MenuItem::separator(),
                              MenuItem::command(ids.print),
                              MenuItem::submenu("Recen&t", {MenuItem::action("notes.md", [] {}),
                                                            MenuItem::action("release.md", [] {})})}},
        widgets::MenuBarItem{"&View",
                             {MenuItem::action("&Word wrap", [] {}).with_mark(MenuMark::Checked),
                              MenuItem::action("&Line numbers", [] {}).with_mark(MenuMark::Unchecked),
                              MenuItem::separator(), MenuItem::action("&Compact", [] {}).with_mark(MenuMark::RadioOn),
                              MenuItem::action("&Spacious", [] {}).with_mark(MenuMark::RadioOff)}},
        widgets::MenuBarItem{"&Window",
                             {MenuItem::action("&Tile", [] {}),
                              MenuItem::action("&Cascade", [] {}).with_enabled(false)}},
    };
}

widgets::MenuBar& menu_bar(Stage& stage, std::vector<widgets::MenuBarItem> items) {
    widgets::MenuBar& bar = *stage.desktop().dock_top(std::make_unique<widgets::MenuBar>(std::move(items)));
    stage.document(Rect{2, 2, 34, 8}, "Notes");
    return bar;
}

widgets::MenuBar& menu_bar(Stage& stage) { return menu_bar(stage, menus(declare_commands(stage.app()))); }

// A status-line item presenting a registered command: the registry supplies
// its chord and title, and the chord wears the shared hotkey accent.
widgets::StatusLineItem command_item(ui::CommandId command, int priority = 0) {
    return widgets::StatusLineItem{std::string{}, command, priority};
}

// The legend an application docks. Help and Quit outrank the document's own
// commands, so a line too narrow for all of them drops the document's first.
std::vector<widgets::StatusLineItem> legend(ui::Application& app, const Commands& ids, bool with_unavailable = false) {
    const ui::StandardCommands& standard = app.commands().standard();
    std::vector<widgets::StatusLineItem> items{command_item(standard.help, 2), command_item(ids.save)};
    if (with_unavailable) items.push_back(command_item(ids.print));
    items.push_back(command_item(standard.quit, 1));
    return items;
}

// --- Desktop ---------------------------------------------------------------

void add_desktop(Catalog& catalog) {
    Element& e = catalog.element("Desktop", "include/cvision/widgets/desktop.hpp", Traits{});
    state(e, "normal", kScreen, [](Stage&) {});
    state(e, "windows", kScreen, [](Stage& s) {
        frame(s, Rect{1, 1, 24, 7}, "Sources");
        frame(s, Rect{12, 4, 26, 7}, "Build log");
    });
    state(e, "tiled", kScreen, [](Stage& s) {
        for (const char* title : {"Sources", "Build log", "Terminal"}) frame(s, Rect{0, 0, 20, 6}, title);
        s.desktop().tile();
    });
    state(e, "cascaded", kScreen, [](Stage& s) {
        for (const char* title : {"Sources", "Build log", "Terminal"}) frame(s, Rect{0, 0, 24, 7}, title);
        s.desktop().cascade();
    });
    state(e, "docked", kScreen, [](Stage& s) {
        const Commands ids = declare_commands(s.app());
        s.desktop().dock_top(std::make_unique<widgets::MenuBar>(menus(ids)));
        s.desktop().dock_bottom(std::make_unique<widgets::StatusLine>())->set_items(legend(s.app(), ids));
        frame(s, Rect{0, 0, 30, 8}, "Notes").toggle_zoom(s.desktop().content_area());
    });
    state(e, "parked", kScreen, [](Stage& s) {
        frame(s, Rect{1, 1, 26, 7}, "config.yaml").set_minimized(true);
        frame(s, Rect{4, 2, 30, 7}, "Build log").set_minimized(true);
        frame(s, Rect{6, 1, 28, 8}, "Terminal");
        s.desktop().finish_minimize_animation();
    });
    // An application's own background over the pattern: a rounded plate with
    // a name on it, centred in the area between the docks, and a window over
    // part of it.
    state(e, "painted", kScreen, [](Stage& s) {
        s.desktop().set_background_painter([&s](scene::Painter& painter, Rect area) {
            const Style plate = s.app().theme().resolve(s.roles().dialog_frame);
            const Rect box{area.x + (area.width - 22) / 2, area.y + (area.height - 5) / 2, 22, 5};
            painter.fill(box, Cell::from_grapheme(" ", plate));
            painter.draw_box(box, scene::LineStyle::Rounded, plate);
            painter.draw_text(Point{box.x + 7, box.y + 2}, "ckVision", plate);
        });
        frame(s, Rect{1, 6, 18, 5}, "Notes");
    });
}

// --- Window ----------------------------------------------------------------

constexpr Rect kWindow{2, 1, 34, 9};

void add_window(Catalog& catalog) {
    Element& e = catalog.element("Window", "include/cvision/widgets/window.hpp", Traits{.text = true, .window = true});
    state(e, "normal", kScreen, [](Stage& s) { frame(s, kWindow, "Report"); });
    state(e, "active", kScreen, [](Stage& s) {
        frame(s, Rect{6, 3, 32, 8}, "Build log");
        frame(s, kWindow, "Report");
    });
    state(e, "inactive", kScreen, [](Stage& s) {
        frame(s, kWindow, "Report");
        cover(s, Rect{12, 5, 26, 6});
    });
    state(e, "footer", kScreen, [](Stage& s) { frame(s, kWindow, "Report").set_footer("2 of 7"); });
    state(e, "zoomed", kScreen, [](Stage& s) { frame(s, kWindow, "Report").toggle_zoom(s.desktop().content_area()); });
    state(e, "resizing", kScreen, [](Stage& s) {
        frame(s, kWindow, "Report");
        s.step();
        const Point corner{kWindow.x + kWindow.width - 1, kWindow.y + kWindow.height - 1};
        s.app().dispatch(press(corner));
        s.app().dispatch(drag(Point{corner.x + 2, corner.y + 1}));
    });
    state(e, "pressed", kScreen, [](Stage& s) {
        frame(s, kWindow, "Report");
        s.step();
        s.app().dispatch(press(Point{kWindow.x + 3, kWindow.y}));
    });
    state(e, "fixed", kScreen, [](Stage& s) { frame(s, kWindow, "Report").set_resizable(false); });
    // A press on the left edge, dragged outward: the edge follows and the
    // right edge stays, with every grip showing while the resize lasts.
    state(e, "edge-resizing", kScreen, [](Stage& s) {
        frame(s, kWindow, "Report");
        s.step();
        const Point edge{kWindow.x, kWindow.y + 4};
        s.app().dispatch(press(edge));
        s.app().dispatch(drag(Point{edge.x - 2, edge.y}));
    });
    // The keyboard move/size mode (Ctrl+F5), part way through: moved one cell
    // right and grown one row, the border in "ckv.window.frame.moving".
    state(e, "moving", kScreen, [](Stage& s) {
        frame(s, kWindow, "Report");
        s.step();
        s.app().dispatch(KeyEvent{KeyChord{Key::F5, Modifier::Ctrl, ""}});
        s.app().dispatch(key(Key::Right));
        s.app().dispatch(KeyEvent{KeyChord{Key::Down, Modifier::Shift, ""}});
    });
    // The three line sets a window may choose instead of the default double-
    // while-active: rounded and single on the active window, double on an
    // inactive one, where the default would have been single.
    state(e, "rounded", kScreen,
          [](Stage& s) { frame(s, kWindow, "Report").set_frame_lines(widgets::FrameLines::Rounded); });
    state(e, "single", kScreen,
          [](Stage& s) { frame(s, kWindow, "Report").set_frame_lines(widgets::FrameLines::Single); });
    state(e, "double", kScreen, [](Stage& s) {
        frame(s, kWindow, "Report").set_frame_lines(widgets::FrameLines::Double);
        cover(s, Rect{12, 5, 26, 6});
    });
    state(e, "wide", kScreen, [](Stage& s) { frame(s, kWindow, std::string(kWideText)); });
    state(e, "narrow", kScreen, [](Stage& s) { frame(s, Rect{2, 1, 24, 7}, "Quarterly report, second draft"); });
}

// --- EditorWindow ----------------------------------------------------------

// The editor's file lives in memory, owned by the specimen's build closure so
// that it outlives every window built from it.
std::shared_ptr<MemoryFileSystem> notes_filesystem() {
    auto fs = std::make_shared<MemoryFileSystem>();
    fs->add_directory("/notes");
    fs->add_file("/notes/release.md", "# Release 0.4\n\nParked windows\nand frame text.\n");
    return fs;
}

constexpr Rect kEditor{1, 1, 36, 9};

widgets::EditorWindow& editor_window(Stage& stage, MemoryFileSystem& fs, Rect bounds = kEditor,
                                     std::string title = "release.md", bool open = true) {
    auto window =
        std::make_unique<widgets::EditorWindow>(std::move(title), std::make_shared<widgets::EditorDocument>(), fs);
    window->set_bounds(bounds);
    if (open) window->open("/notes/release.md");
    widgets::EditorWindow& placed = *window;
    stage.add_window(std::move(window));
    return placed;
}

void add_editor_window(Catalog& catalog) {
    Element& e = catalog.element("EditorWindow", "include/cvision/widgets/editor_window.hpp",
                                 Traits{.text = true, .window = true});
    state(e, "normal", kScreen, [fs = notes_filesystem()](Stage& s) { editor_window(s, *fs); });
    state(e, "active", kScreen, [fs = notes_filesystem()](Stage& s) {
        frame(s, Rect{8, 3, 30, 8}, "Build log");
        s.focus(editor_window(s, *fs).editor());
    });
    state(e, "inactive", kScreen, [fs = notes_filesystem()](Stage& s) {
        editor_window(s, *fs);
        cover(s, Rect{14, 5, 24, 6});
    });
    state(e, "modified", kScreen, [fs = notes_filesystem()](Stage& s) {
        s.focus(editor_window(s, *fs).editor());
        s.app().dispatch(typed("x"));
    });
    state(e, "overwrite", kScreen, [fs = notes_filesystem()](Stage& s) {
        s.focus(editor_window(s, *fs).editor());
        s.app().dispatch(key(Key::Insert));
    });
    state(e, "wide", kScreen,
          [fs = notes_filesystem()](Stage& s) { editor_window(s, *fs, kEditor, std::string(kWideText), false); });
    state(e, "narrow", kScreen, [fs = notes_filesystem()](Stage& s) {
        editor_window(s, *fs, Rect{1, 1, 22, 9}, "quarterly-release-notes.md", false);
    });
}

// --- FrameText -------------------------------------------------------------

widgets::FrameText& frame_text(widgets::Window& window, std::string text,
                               widgets::FrameSlot slot = widgets::FrameSlot{}) {
    return *window.add_frame_overlay(std::make_unique<widgets::FrameText>(std::move(text)), slot);
}

void add_frame_text(Catalog& catalog) {
    Element& e = catalog.element("FrameText", "include/cvision/widgets/frame_text.hpp", Traits{.text = true});
    state(e, "normal", kScreen, [](Stage& s) { frame_text(frame(s, kWindow, "Report"), "Ln 4, Col 12"); });
    state(e, "start", kScreen, [](Stage& s) {
        frame_text(frame(s, kWindow, "Report"), "/home/notes",
                   widgets::FrameSlot{widgets::Edge::Bottom, ui::Alignment::Start, 0});
    });
    state(e, "reserved", kScreen, [](Stage& s) {
        widgets::FrameText& text = frame_text(frame(s, kWindow, "Report"), "2/7");
        text.set_reserved_width(12);
        text.set_alignment(ui::Alignment::Center);
    });
    state(e, "inactive", kScreen, [](Stage& s) {
        frame_text(frame(s, kWindow, "Report"), "Ln 4, Col 12");
        cover(s, Rect{16, 2, 22, 5});
    });
    state(e, "dialog", kScreen, [](Stage& s) {
        frame_text(s.window(kWindow, "Report", std::make_unique<ui::View>(), true), "Ln 4, Col 12");
    });
    state(e, "wide", kScreen, [](Stage& s) { frame_text(frame(s, kWindow, "Report"), std::string(kWideText)); });
    state(e, "narrow", kScreen,
          [](Stage& s) { frame_text(frame(s, Rect{2, 1, 14, 6}, "Log"), "Page 12 of 340, line 9"); });
}

// --- MinimizedWindowStub ---------------------------------------------------

widgets::MinimizedWindowStub& parked(Stage& stage, std::string title) {
    frame(stage, kWindow, std::move(title)).set_minimized(true);
    stage.desktop().finish_minimize_animation();
    return *stage.desktop().parked_windows().back();
}

void add_minimized_window_stub(Catalog& catalog) {
    Element& e = catalog.element("MinimizedWindowStub", "include/cvision/widgets/minimized_window_stub.hpp",
                                 Traits{.focusable = true, .text = true});
    state(e, "normal", kScreen, [](Stage& s) { parked(s, "config.yaml"); });
    state(e, "focused", kScreen, [](Stage& s) { s.focus(parked(s, "config.yaml")); });
    state(e, "several", kScreen, [](Stage& s) {
        parked(s, "config.yaml");
        parked(s, "Build log");
        frame(s, Rect{4, 1, 30, 7}, "Terminal");
    });
    state(e, "close-pressed", kScreen, [](Stage& s) {
        widgets::MinimizedWindowStub& stub = parked(s, "config.yaml");
        s.step();
        const Rect at = stub.absolute_bounds();
        s.app().dispatch(press(Point{at.x + 3, at.y}));
    });
    state(e, "restore-pressed", kScreen, [](Stage& s) {
        widgets::MinimizedWindowStub& stub = parked(s, "config.yaml");
        s.step();
        const Rect at = stub.absolute_bounds();
        s.app().dispatch(press(Point{at.x + at.width - 4, at.y}));
    });
    state(e, "wide", kScreen, [](Stage& s) { parked(s, std::string(kWideText)); });
    state(e, "narrow", Size{20, 6}, [](Stage& s) { parked(s, "quarterly-release-notes.md"); });
}

// --- MenuBar ---------------------------------------------------------------

void add_menu_bar(Catalog& catalog) {
    Element& e =
        catalog.element("MenuBar", "include/cvision/widgets/menu.hpp", Traits{.focusable = true, .text = true});
    state(e, "normal", kScreen, [](Stage& s) { menu_bar(s); });
    state(e, "focused", kScreen, [](Stage& s) { menu_bar(s).activate(); });
    state(e, "walked", kScreen, [](Stage& s) {
        menu_bar(s).activate();
        s.app().dispatch(key(Key::Right));
    });
    state(e, "open", kScreen, [](Stage& s) {
        menu_bar(s).activate();
        s.app().dispatch(key(Key::Down));
    });
    state(e, "trailing", kScreen,
          [](Stage& s) { menu_bar(s).set_trailing_view(std::make_unique<widgets::StaticText>("12:00")); });
    state(e, "wide", kScreen,
          [](Stage& s) { menu_bar(s, menus(declare_commands(s.app()), std::string(kWideText))); });
    // Too narrow for Window: the titles that fit are drawn whole, and the
    // overflow title stands for the rest.
    state(e, "narrow", Size{20, 8},
          [](Stage& s) { menu_bar(s, menus(declare_commands(s.app()), "&Document")); });
    state(e, "overflow-walked", Size{20, 8}, [](Stage& s) {
        menu_bar(s, menus(declare_commands(s.app()), "&Document")).activate();
        s.app().dispatch(key(Key::End));
    });
    // A hidden menu's mnemonic: the overflow list, with that menu entered.
    state(e, "overflow-open", Size{20, 8}, [](Stage& s) {
        menu_bar(s, menus(declare_commands(s.app()), "&Document")).activate();
        s.app().dispatch(typed("w"));
    });
}

// --- DropdownMenu ----------------------------------------------------------

// Opens the bar's `menu`th title from the keyboard, as F10 and Down do.
void open_menu(Stage& stage, int menu) {
    menu_bar(stage).activate();
    for (int i = 0; i < menu; ++i) stage.app().dispatch(key(Key::Right));
    stage.app().dispatch(key(Key::Down));
}

void context_menu(Stage& stage, std::vector<widgets::MenuItem> items, Point at) {
    widgets::show_context_menu(std::move(items), at, stage.app(), stage.desktop());
}

void add_dropdown_menu(Catalog& catalog) {
    using widgets::MenuItem;
    Element& e = catalog.element("DropdownMenu", "include/cvision/widgets/menu.hpp", Traits{.text = true});
    state(e, "normal", kScreen, [](Stage& s) { open_menu(s, 0); });
    state(e, "unavailable", kScreen, [](Stage& s) {
        open_menu(s, 0);
        s.app().dispatch(key(Key::Down));
        s.app().dispatch(key(Key::Down));
    });
    state(e, "submenu", kScreen, [](Stage& s) {
        open_menu(s, 0);
        s.app().dispatch(key(Key::End));
        s.app().dispatch(key(Key::Right));
    });
    state(e, "marks", kScreen, [](Stage& s) { open_menu(s, 1); });
    state(e, "pointer", kScreen, [](Stage& s) {
        menu_bar(s);
        s.step();
        s.app().dispatch(press(Point{2, 0}));
    });
    state(e, "context", kScreen, [](Stage& s) {
        s.document(Rect{2, 1, 34, 9}, "Notes");
        context_menu(s,
                     {MenuItem::action("Cu&t", [] {}), MenuItem::action("&Copy", [] {}),
                      MenuItem::action("&Paste", [] {}).with_enabled(false), MenuItem::separator(),
                      MenuItem::action("Select &all", [] {})},
                     Point{10, 3});
    });
    state(e, "wide", kScreen, [](Stage& s) {
        s.document(Rect{2, 1, 34, 9}, "Notes");
        context_menu(s, {MenuItem::action(std::string(kWideText), [] {}), MenuItem::action("&Copy", [] {})},
                     Point{10, 3});
    });
    state(e, "narrow", Size{20, 8}, [](Stage& s) {
        context_menu(s, {MenuItem::action("Reveal in the file browser", [] {}), MenuItem::action("&Copy", [] {})},
                     Point{6, 2});
    });
}

// --- PagedStrip ------------------------------------------------------------

widgets::PagedStrip& paged_strip(Stage& stage, std::vector<std::string> labels, std::size_t selected = 0) {
    stage.document(Rect{2, 1, 34, 7}, "Notes");
    widgets::PagedStrip& strip = *stage.desktop().dock_bottom(std::make_unique<widgets::PagedStrip>());
    strip.set_item_source([names = std::move(labels), selected] {
        std::vector<widgets::PagedStrip::Item> items;
        for (std::size_t i = 0; i < names.size(); ++i) {
            widgets::PagedStrip::Item item;
            item.text = names[i];
            item.width = text::text_width(names[i]);
            item.selected = i == selected;
            items.push_back(std::move(item));
        }
        return items;
    });
    return strip;
}

std::vector<std::string> many_items() {
    return {"editor", "shell", "monitor", "release notes", "changelog", "scratch", "build"};
}

void add_paged_strip(Catalog& catalog) {
    Element& e = catalog.element("PagedStrip", "include/cvision/widgets/paged_strip.hpp", Traits{.text = true});
    state(e, "normal", kScreen, [](Stage& s) { paged_strip(s, {"editor", "shell", "monitor"}); });
    state(e, "paged", kScreen, [](Stage& s) { paged_strip(s, many_items()); });
    state(e, "last-page", kScreen, [](Stage& s) {
        widgets::PagedStrip& strip = paged_strip(s, many_items());
        strip.set_page(strip.page_count() - 1);
    });
    state(e, "collapsible", kScreen, [](Stage& s) { paged_strip(s, many_items()).set_collapsible(true); });
    state(e, "collapsed", kScreen, [](Stage& s) {
        widgets::PagedStrip& strip = paged_strip(s, many_items());
        strip.set_collapsible(true);
        strip.set_collapsed(true);
    });
    state(e, "pressed", kScreen, [](Stage& s) {
        widgets::PagedStrip& strip = paged_strip(s, {"editor", "shell", "monitor"});
        s.step();
        const Rect at = strip.absolute_bounds();
        s.app().dispatch(press(Point{at.x + strip.placed_items().at(1).x + 1, at.y}));
    });
    state(e, "wide", kScreen, [](Stage& s) { paged_strip(s, {"editor", std::string(kWideText), "shell"}, 1); });
    state(e, "narrow", Size{20, 6}, [](Stage& s) { paged_strip(s, {"quarterly-release-notes.md"}); });
}

// --- WindowSwitcherBar -----------------------------------------------------

widgets::WindowSwitcherBar& switcher(Stage& stage, const std::vector<std::string>& titles) {
    // The bar is the listing, so the desktop parks no second one beside it.
    stage.desktop().set_minimized_window_placement(widgets::MinimizedWindowPlacement::HostListed);
    for (const std::string& title : titles) frame(stage, Rect{2, 1, 30, 7}, title);
    return *stage.desktop().dock_bottom(std::make_unique<widgets::WindowSwitcherBar>(stage.desktop()));
}

std::vector<std::string> three_windows() { return {"Sources", "Build log", "Terminal"}; }

Point first_entry(Stage& stage, widgets::WindowSwitcherBar& bar) {
    stage.step();
    const Rect at = bar.absolute_bounds();
    return Point{at.x + bar.drawn_entries().front().x + 1, at.y};
}

void add_window_switcher_bar(Catalog& catalog) {
    Element& e = catalog.element("WindowSwitcherBar", "include/cvision/widgets/window_switcher_bar.hpp",
                                 Traits{.text = true});
    state(e, "normal", kScreen, [](Stage& s) { switcher(s, three_windows()); });
    state(e, "minimized", kScreen, [](Stage& s) {
        switcher(s, three_windows());
        s.desktop().windows().front()->set_minimized(true);
        s.desktop().finish_minimize_animation();
    });
    state(e, "paged", Size{28, 8},
          [](Stage& s) { switcher(s, {"Sources", "Build log", "Terminal", "Release notes", "Scratch"}); });
    state(e, "pressed", kScreen, [](Stage& s) {
        widgets::WindowSwitcherBar& bar = switcher(s, three_windows());
        s.app().dispatch(press(first_entry(s, bar)));
    });
    state(e, "context", kScreen, [](Stage& s) {
        widgets::WindowSwitcherBar& bar = switcher(s, three_windows());
        bar.set_context_menu_provider([](const widgets::WindowSwitcherTarget& target) {
            return std::vector<widgets::MenuItem>{
                widgets::MenuItem::action("&Close", target.bind([](widgets::Window& window) { window.close(); })),
                widgets::MenuItem::action("&Minimize",
                                          target.bind([](widgets::Window& window) { window.set_minimized(true); }))};
        });
        s.app().dispatch(press(first_entry(s, bar), MouseButton::Right));
    });
    state(e, "wide", kScreen, [](Stage& s) { switcher(s, {"Sources", std::string(kWideText)}); });
    state(e, "narrow", Size{20, 6}, [](Stage& s) { switcher(s, {"quarterly-release-notes.md"}); });
}

// --- StatusLine ------------------------------------------------------------

widgets::StatusLine& status_line(Stage& stage, bool with_unavailable = false) {
    const Commands ids = declare_commands(stage.app());
    stage.document(Rect{2, 1, 34, 8}, "Notes");
    widgets::StatusLine& status = *stage.desktop().dock_bottom(std::make_unique<widgets::StatusLine>());
    status.set_items(legend(stage.app(), ids, with_unavailable));
    return status;
}

void add_status_line(Catalog& catalog) {
    Element& e = catalog.element("StatusLine", "include/cvision/widgets/status_line.hpp", Traits{.text = true});
    for (const std::string variant : {"normal", "hint", "pressed", "unavailable", "wide", "narrow"}) {
        state(e, "grouped-" + variant, variant == "narrow" ? Size{20, 6} : Size{48, 12}, [variant](Stage& s) {
            auto& status = status_line(s, variant == "unavailable");
            auto items = status.items();
            if (items.size() > 1) items[1].group_break_before = true;
            if (variant == "wide") items[1].presentation.label = std::string(kWideText);
            status.set_items(std::move(items));
            status.set_presentation(widgets::StatusLinePresentation::Grouped);
            if (variant == "hint") status.set_transient_hint("Saved notes.md");
            if (variant == "pressed") {
                s.step();
                const Rect at = status.absolute_bounds();
                s.app().dispatch(press(Point{at.x + 2, at.y}));
            }
        });
    }
    state(e, "normal", kScreen, [](Stage& s) { status_line(s); });
    state(e, "unavailable", Size{48, 12}, [](Stage& s) { status_line(s, true); });
    state(e, "hint", kScreen, [](Stage& s) { status_line(s).set_transient_hint("Saved notes.md"); });
    state(e, "pressed", kScreen, [](Stage& s) {
        widgets::StatusLine& status = status_line(s);
        s.step();
        const Rect at = status.absolute_bounds();
        s.app().dispatch(press(Point{at.x + 2, at.y}));
    });
    state(e, "wide", kScreen, [](Stage& s) {
        status_line(s).set_items(
            {command_item(s.app().commands().standard().help), widgets::StatusLineItem{std::string(kWideText)}});
    });
    state(e, "narrow", Size{20, 6}, [](Stage& s) { status_line(s, true); });
}

// --- Tooltip ---------------------------------------------------------------

// A tooltip is a popup: it stands above every window, as it must to explain
// something inside one.
widgets::Tooltip& tooltip(Stage& stage, std::string text, Point at = Point{6, 5}) {
    ui::View& body = stage.dialog(Rect{2, 1, 34, 8}, "Export");
    stage.place(body, Rect{2, 2, 12, 1}, std::make_unique<widgets::StaticText>("report.pdf"));
    widgets::Tooltip& tip = *stage.desktop().add_popup(std::make_unique<widgets::Tooltip>(std::move(text)));
    tip.show_at(at);
    return tip;
}

void add_tooltip(Catalog& catalog) {
    Element& e = catalog.element("Tooltip", "include/cvision/widgets/common_components.hpp", Traits{.text = true});
    state(e, "normal", kScreen, [](Stage& s) { tooltip(s, "Writes report.pdf"); });
    state(e, "over-desktop", kScreen, [](Stage& s) { tooltip(s, "Drop a file to open it", Point{10, 10}); });
    state(e, "wide", kScreen, [](Stage& s) { tooltip(s, std::string(kWideText)); });
    state(e, "narrow", kScreen, [](Stage& s) {
        tooltip(s, "Writes report.pdf beside the source").set_bounds(Rect{6, 5, 14, 1});
    });
    // Placed beside an anchor at the bottom right: no row below, so the row
    // above, and moved left until it ends inside the desktop.
    state(e, "above-anchor", kScreen, [](Stage& s) {
        tooltip(s, "Writes report.pdf").show_near(Rect{30, 10, 8, 2}, Rect{0, 0, 40, 12});
    });
}

// --- NotificationCenter ----------------------------------------------------

// Toasts laid over the work, so the centre is a popup above the windows.
widgets::NotificationCenter& notifications(Stage& stage, std::vector<widgets::Notification> posted,
                                           Rect bounds = Rect{8, 1, 30, 3}) {
    stage.document(Rect{1, 2, 34, 9}, "Deploy");
    widgets::NotificationCenter& centre = *stage.desktop().add_popup(std::make_unique<widgets::NotificationCenter>());
    centre.set_bounds(bounds);
    for (widgets::Notification& notification : posted) centre.add(std::move(notification));
    return centre;
}

widgets::Notification toast(widgets::NotificationSeverity severity, std::string text) {
    return widgets::Notification{severity, std::move(text), false};
}

void add_notification_center(Catalog& catalog) {
    using widgets::NotificationSeverity;
    Element& e = catalog.element("NotificationCenter", "include/cvision/widgets/common_components.hpp",
                                 Traits{.focusable = true, .text = true});
    for (auto presentation : {widgets::NotificationPresentation::Banners, widgets::NotificationPresentation::Framed}) {
        const std::string prefix = presentation == widgets::NotificationPresentation::Banners ? "banners" : "framed";
        for (auto variant : {"normal", "focused", "disabled", "warning", "error", "stacked", "unicode", "narrow", "tiny"}) {
            state(e, prefix + "-" + variant, kScreen, [presentation, variant](Stage& s) {
                const std::string_view name(variant);
                std::vector<widgets::Notification> posted{toast(NotificationSeverity::Info, name == "unicode" ? std::string(kWideText) : "Build finished")};
                if (name == "warning") posted[0].severity = NotificationSeverity::Warning;
                if (name == "error") posted[0].severity = NotificationSeverity::Error;
                if (name == "stacked") {
                    posted.push_back(toast(NotificationSeverity::Warning, "Tests skipped"));
                    posted.push_back(widgets::Notification{NotificationSeverity::Error, "Upload refused", true});
                }
                auto& center = notifications(s, std::move(posted), Rect{8, 1, name == "narrow" ? 12 : name == "tiny" ? 1 : 30, name == "tiny" ? 1 : 11});
                center.set_presentation(presentation);
                if (name == "focused") s.focus(center);
                if (name == "disabled") center.set_enabled(false);
            });
        }
    }
    state(e, "normal", kScreen,
          [](Stage& s) { notifications(s, {toast(NotificationSeverity::Info, "Build finished")}); });
    state(e, "focused", kScreen,
          [](Stage& s) { s.focus(notifications(s, {toast(NotificationSeverity::Info, "Build finished")})); });
    state(e, "warning", kScreen,
          [](Stage& s) { notifications(s, {toast(NotificationSeverity::Warning, "2 tests were skipped")}); });
    state(e, "error", kScreen,
          [](Stage& s) { notifications(s, {toast(NotificationSeverity::Error, "Upload refused")}); });
    state(e, "stacked", kScreen, [](Stage& s) {
        notifications(s, {toast(NotificationSeverity::Info, "Build finished"),
                          toast(NotificationSeverity::Warning, "2 tests were skipped"),
                          widgets::Notification{NotificationSeverity::Error, "Upload refused", true}});
    });
    state(e, "wide", kScreen,
          [](Stage& s) { notifications(s, {toast(NotificationSeverity::Info, std::string(kWideText))}); });
    state(e, "narrow", kScreen, [](Stage& s) {
        notifications(s, {toast(NotificationSeverity::Warning, "2 tests were skipped")}, Rect{8, 1, 12, 1});
    });
}

// --- Standard dialogs: shared staging --------------------------------------

// Where a standard dialog is put when a specimen places it rather than
// leaving that to the desktop, and the window that holds the activation when
// the dialog is shown inactive.
constexpr Rect kPlacedDialog{6, 2, 52, 15};
constexpr Rect kActivationHolder{34, 11, 28, 8};

// A standard dialog presented modally at `bounds`, over a document window
// that it therefore leaves inactive behind it.
void present_over_document(Stage& stage, widgets::WindowHandle handle, Rect bounds = kPlacedDialog) {
    frame(stage, Rect{1, 1, 40, 12}, "Notes");
    handle.window->set_bounds(bounds);
    stage.desktop().present_modal(std::move(handle), stage.app());
}

// A standard dialog presented modelessly at `bounds`, with the activation
// handed to another window afterwards.
void present_inactive(Stage& stage, widgets::WindowHandle handle, Rect bounds = kPlacedDialog) {
    handle.window->set_bounds(bounds);
    stage.desktop().present_modeless(std::move(handle), stage.app());
    cover(stage, kActivationHolder);
}

// --- MessageBox ------------------------------------------------------------

void add_message_box(Catalog& catalog) {
    using widgets::MessageBoxKind;
    Element& e =
        catalog.element("MessageBox", "include/cvision/widgets/message_box.hpp", Traits{.text = true, .window = true});
    state(e, "normal", kScreen, [](Stage& s) { present(s, saved()); });
    state(e, "active", kScreen, [](Stage& s) {
        frame(s, Rect{1, 1, 30, 8}, "Notes");
        present(s, saved());
    });
    state(e, "inactive", kScreen, [](Stage& s) {
        s.desktop().present_modeless(widgets::make_message_box(saved(), s.roles(), s.app(), nullptr, nullptr), s.app());
        cover(s, Rect{16, 6, 22, 5});
    });
    state(e, "warning", kScreen, [](Stage& s) {
        present(s, message(MessageBoxKind::Warning, "Low space", "Less than 1 MB is left.",
                           widgets::MessageBoxButtons::OkCancel));
    });
    state(e, "error", kScreen,
          [](Stage& s) { present(s, message(MessageBoxKind::Error, "Upload failed", "No credentials.")); });
    state(e, "confirm", kScreen, [](Stage& s) {
        present(s, message(MessageBoxKind::Confirm, "Unsaved changes", "Close notes.md anyway?",
                           widgets::MessageBoxButtons::YesNoCancel));
    });
    state(e, "wide", kScreen,
          [](Stage& s) { present(s, message(MessageBoxKind::Info, std::string(kWideText), std::string(kWideText))); });
    state(e, "narrow", Size{24, 12}, [](Stage& s) {
        present(s, message(MessageBoxKind::Info, "Synchronization finished",
                           "Every file in the workspace is up to date with the server."));
    });
}

// --- FileDialog and DirectoryPicker ----------------------------------------

// A small tree the file dialogs browse: injected, so a specimen never depends
// on the machine that renders it, and owned by the build closures, which
// outlive every dialog built from it.
std::shared_ptr<const MemoryFileSystem> project_filesystem() {
    auto fs = std::make_shared<MemoryFileSystem>();
    fs->add_directory("/project");
    fs->add_directory("/project/docs");
    fs->add_directory("/project/src");
    fs->add_directory("/project/.cache");
    fs->add_file("/project/README.md", "# Project\n");
    fs->add_file("/project/release-notes.md", "0.4.0\n");
    fs->add_file("/project/CMakeLists.txt", "project(demo)\n");
    fs->add_file("/project/docs/guide.md", "");
    fs->add_file("/project/src/main.cpp", "int main() {}\n");
    return fs;
}

widgets::FileDialogOptions file_filters() {
    widgets::FileDialogOptions options;
    options.filters = {widgets::FileDialogFilter{"All files", {}}, widgets::FileDialogFilter{"Markdown", {".md"}}};
    return options;
}

void present_modal_file_dialog(Stage& stage, const FileSystem& fs, widgets::FileDialogOptions options = file_filters(),
                               widgets::FileDialogMode mode = widgets::FileDialogMode::Open) {
    static_cast<void>(widgets::present_modal_file_dialog(mode, "/project", fs, std::move(options), stage.app(),
                                                         stage.desktop(), stage.roles()));
}

// The file dialog's button row needs the dialog screen's whole width.
constexpr Rect kFileDialog{0, 3, 64, 15};

widgets::WindowHandle file_dialog(Stage& stage, const FileSystem& fs) {
    return widgets::make_file_dialog(widgets::FileDialogMode::Open, "/project", fs, file_filters(), stage.roles(),
                                     stage.app(), nullptr, nullptr);
}

void add_file_dialog(Catalog& catalog) {
    Element& e = catalog.element("FileDialog", "include/cvision/widgets/file_dialog.hpp", Traits{.window = true});
    state(e, "normal", kDialogScreen, [fs = project_filesystem()](Stage& s) { present_modal_file_dialog(s, *fs); });
    state(e, "active", kDialogScreen,
          [fs = project_filesystem()](Stage& s) { present_over_document(s, file_dialog(s, *fs), kFileDialog); });
    state(e, "inactive", kDialogScreen,
          [fs = project_filesystem()](Stage& s) { present_inactive(s, file_dialog(s, *fs), kFileDialog); });
    state(e, "save", kDialogScreen, [fs = project_filesystem()](Stage& s) {
        widgets::FileDialogOptions options = file_filters();
        options.suggested_name = "summary.md";
        present_modal_file_dialog(s, *fs, std::move(options), widgets::FileDialogMode::Save);
    });
    state(e, "filtered", kDialogScreen, [fs = project_filesystem()](Stage& s) {
        widgets::FileDialogOptions options = file_filters();
        options.active_filter = 1;
        present_modal_file_dialog(s, *fs, std::move(options));
    });
    state(e, "hidden-shown", kDialogScreen, [fs = project_filesystem()](Stage& s) {
        widgets::FileDialogOptions options = file_filters();
        options.show_hidden = true;
        present_modal_file_dialog(s, *fs, std::move(options));
    });
    state(e, "moved", kDialogScreen, [fs = project_filesystem()](Stage& s) {
        present_modal_file_dialog(s, *fs);
        s.app().dispatch(key(Key::Down));
        s.app().dispatch(key(Key::Down));
    });
    // On a terminal narrower than the dialog asks for.
    state(e, "cramped", Size{48, 20}, [fs = project_filesystem()](Stage& s) { present_modal_file_dialog(s, *fs); });
}

widgets::WindowHandle directory_picker(Stage& stage, const FileSystem& fs) {
    return widgets::make_directory_picker(fs, "/project", stage.roles(), stage.app(), nullptr, nullptr);
}

void add_directory_picker(Catalog& catalog) {
    Element& e =
        catalog.element("DirectoryPicker", "include/cvision/widgets/directory_picker.hpp", Traits{.window = true});
    // As the desktop places it on its own.
    state(e, "normal", kDialogScreen, [fs = project_filesystem()](Stage& s) {
        static_cast<void>(widgets::present_modal_directory_picker(*fs, "/project", s.app(), s.desktop(), s.roles()));
    });
    state(e, "active", kDialogScreen,
          [fs = project_filesystem()](Stage& s) { present_over_document(s, directory_picker(s, *fs)); });
    state(e, "inactive", kDialogScreen,
          [fs = project_filesystem()](Stage& s) { present_inactive(s, directory_picker(s, *fs)); });
    // The root opened, and the selection moved into it.
    state(e, "moved", kDialogScreen, [fs = project_filesystem()](Stage& s) {
        present_over_document(s, directory_picker(s, *fs));
        s.app().dispatch(key(Key::Right));
        s.app().dispatch(key(Key::Down));
        s.app().dispatch(key(Key::Down));
    });
}

// --- WindowListDialog ------------------------------------------------------

void list_windows(Stage& stage, std::vector<std::string> titles = {"release-notes.md", "guide.md", "Terminal"}) {
    for (std::string& title : titles) frame(stage, Rect{2, 1, 30, 8}, std::move(title));
}

void present_window_list(Stage& stage) {
    static_cast<void>(widgets::present_modal_window_list_dialog(stage.desktop(), stage.app(), stage.roles()));
}

void add_window_list_dialog(Catalog& catalog) {
    Element& e =
        catalog.element("WindowListDialog", "include/cvision/widgets/window_list_dialog.hpp", Traits{.window = true});
    state(e, "normal", kDialogScreen, [](Stage& s) {
        list_windows(s, {"Notes"});
        present_window_list(s);
    });
    state(e, "active", kDialogScreen, [](Stage& s) {
        list_windows(s);
        s.desktop().cascade();
        present_window_list(s);
    });
    state(e, "inactive", kDialogScreen, [](Stage& s) {
        list_windows(s);
        present_inactive(s, widgets::make_window_list_dialog(s.desktop(), s.roles(), s.app(), nullptr),
                         Rect{4, 1, 46, 10});
    });
    state(e, "moved", kDialogScreen, [](Stage& s) {
        list_windows(s);
        present_window_list(s);
        s.app().dispatch(key(Key::Down));
    });
    state(e, "minimized", kDialogScreen, [](Stage& s) {
        list_windows(s);
        s.desktop().windows().front()->set_minimized(true);
        s.desktop().finish_minimize_animation();
        present_window_list(s);
    });
    // Typing over the list filters it: "no" leaves the one title containing
    // it, and the filter line above says what was typed.
    state(e, "filtered", kDialogScreen, [](Stage& s) {
        list_windows(s);
        present_window_list(s);
        s.app().dispatch(typed("n"));
        s.app().dispatch(typed("o"));
    });
    // A filter nothing matches: an empty list, and the two buttons that act
    // on a listed window disabled.
    state(e, "unmatched", kDialogScreen, [](Stage& s) {
        list_windows(s);
        present_window_list(s);
        s.app().dispatch(typed("x"));
    });
}

// --- ThemeEditor -----------------------------------------------------------

// The editor asks for 78x22 cells, the most an 80x24 terminal holds with the
// window's shadow; its placed states put it exactly there.
constexpr Size kThemeEditorScreen{80, 24};
constexpr Rect kPlacedThemeEditor{1, 1, 78, 22};

void present_modal_theme_editor(Stage& stage) {
    static_cast<void>(
        widgets::present_modal_theme_editor(stage.app().theme(), stage.app(), stage.desktop(), stage.roles()));
}

widgets::WindowHandle theme_editor(Stage& stage) {
    return widgets::make_theme_editor(stage.app().theme(), stage.roles(), stage.app(), nullptr, nullptr);
}

void add_theme_editor(Catalog& catalog) {
    Element& e =
        catalog.element("ThemeEditor", "include/cvision/widgets/theme_editor.hpp", Traits{.window = true});
    state(e, "normal", kThemeEditorScreen, present_modal_theme_editor);
    state(e, "active", kThemeEditorScreen,
          [](Stage& s) { present_over_document(s, theme_editor(s), kPlacedThemeEditor); });
    state(e, "inactive", kThemeEditorScreen,
          [](Stage& s) { present_inactive(s, theme_editor(s), kPlacedThemeEditor); });
    // A role chosen and its foreground changed to a palette entry: the table
    // row and the live preview both show the edit.
    state(e, "edited", kThemeEditorScreen, [](Stage& s) {
        present_modal_theme_editor(s);
        for (int row = 0; row < 3; ++row) s.app().dispatch(key(Key::Down));
        s.app().dispatch(key(Key::Tab));
        s.app().dispatch(key(Key::Left));
        s.app().dispatch(key(Key::Tab));
        s.app().dispatch(typed("1"));
        s.app().dispatch(typed("9"));
        s.app().dispatch(typed("6"));
    });
    // The first role underlined, its rule made curly and coloured palette
    // entry 9, each editor reached by its label's mnemonic: the underline's
    // editors enabled, and the table and the preview's sample underlined.
    // The matrix's terminal does not claim underline_styles, so the frame
    // shows the plain rule every host can draw.
    state(e, "underlined", kThemeEditorScreen, [](Stage& s) {
        present_modal_theme_editor(s);
        s.app().dispatch(alt("t"));
        s.app().dispatch(key(Key::Down));
        s.app().dispatch(typed(" "));
        s.app().dispatch(alt("u"));
        s.app().dispatch(key(Key::Right));
        s.app().dispatch(key(Key::Right));
        s.app().dispatch(alt("l"));
        s.app().dispatch(key(Key::Right));
        s.app().dispatch(key(Key::Tab));
        s.app().dispatch(typed("9"));
    });
    // On a terminal smaller than the dialog asks for: the role table gives up
    // its rows first.
    state(e, "cramped", Size{64, 20}, present_modal_theme_editor);
}

// --- HelpViewer ------------------------------------------------------------

// The topics the viewer shows, owned by the build closures for the reason
// the file systems are.
std::shared_ptr<const widgets::HelpProvider> help_topics(
    std::string title = "Window chrome",
    std::string body = "Every window frame carries a close control, a title, and a resize grip.") {
    auto provider = std::make_shared<widgets::MemoryHelpProvider>();
    provider->add_topic("chrome", widgets::HelpTopic{std::move(title),
                                                     {{std::move(body) + " The "},
                                                      {"menu bar", "menus"},
                                                      {" opens with F10."}},
                                                     {{"menus", "Menus"}, {"status", "Status line"}}});
    provider->add_topic("menus", widgets::HelpTopic{"Menus", {{"F10 walks the menu bar."}}, {{"chrome", "Window chrome"}}});
    provider->add_topic("status", widgets::HelpTopic{"Status line", {{"Hints follow the focus."}}, {}});
    return provider;
}

void present_help(Stage& stage, const widgets::HelpProvider& provider) {
    static_cast<void>(
        widgets::present_modeless_help_viewer(provider, "chrome", stage.app(), stage.desktop(), stage.roles()));
}

widgets::WindowHandle help_viewer(Stage& stage, const widgets::HelpProvider& provider) {
    return widgets::make_help_viewer(provider, "chrome", stage.roles(), stage.app(), nullptr);
}

void add_help_viewer(Catalog& catalog) {
    Element& e =
        catalog.element("HelpViewer", "include/cvision/widgets/help_viewer.hpp", Traits{.text = true, .window = true});
    state(e, "normal", kDialogScreen, [topics = help_topics()](Stage& s) { present_help(s, *topics); });
    state(e, "active", kDialogScreen,
          [topics = help_topics()](Stage& s) { present_over_document(s, help_viewer(s, *topics)); });
    state(e, "inactive", kDialogScreen,
          [topics = help_topics()](Stage& s) { present_inactive(s, help_viewer(s, *topics)); });
    // The index moved to another topic, which the prose pane follows.
    state(e, "topic", kDialogScreen, [topics = help_topics()](Stage& s) {
        present_help(s, *topics);
        s.app().dispatch(key(Key::Up));
    });
    // A cross-link holding the keyboard: Tab from the index reaches the prose
    // on its first link, and the next Tab steps to the see-also list.
    state(e, "link", kDialogScreen, [topics = help_topics()](Stage& s) {
        present_help(s, *topics);
        s.app().dispatch(key(Key::Tab));
        s.app().dispatch(key(Key::Tab));
    });
    // A query in the search box narrows the index.
    state(e, "searched", kDialogScreen, [topics = help_topics()](Stage& s) {
        present_help(s, *topics);
        s.app().dispatch(KeyEvent{KeyChord{Key::Tab, Modifier::Shift, ""}});
        s.app().dispatch(typed("m"));
        s.app().dispatch(typed("e"));
    });
    state(e, "wide", kDialogScreen, [topics = help_topics(std::string(kWideText), std::string(kWideText))](Stage& s) {
        present_help(s, *topics);
    });
    state(e, "narrow", Size{30, 14}, [topics = help_topics()](Stage& s) { present_help(s, *topics); });
}

// --- TerminalReportDialog --------------------------------------------------

widgets::TerminalReportDialogOptions counted_reports() {
    widgets::TerminalReportDialogOptions options;
    options.mouse_reports_decoded = [] { return std::size_t{42}; };
    return options;
}

widgets::WindowHandle terminal_report(Stage& stage) {
    return widgets::make_terminal_report_dialog(stage.desktop(), stage.roles(), stage.app(), nullptr,
                                                counted_reports());
}

void present_terminal_report(Stage& stage, widgets::TerminalReportDialogOptions options = counted_reports()) {
    static_cast<void>(
        widgets::present_modal_terminal_report_dialog(stage.desktop(), stage.app(), stage.roles(), std::move(options)));
}

void add_terminal_report_dialog(Catalog& catalog) {
    Element& e = catalog.element("TerminalReportDialog", "include/cvision/widgets/terminal_report_dialog.hpp",
                                 Traits{.window = true});
    state(e, "normal", kDialogScreen, [](Stage& s) { present_terminal_report(s); });
    state(e, "active", kDialogScreen, [](Stage& s) { present_over_document(s, terminal_report(s)); });
    state(e, "inactive", kDialogScreen, [](Stage& s) { present_inactive(s, terminal_report(s)); });
    // A host that does not count decoded mouse reports: the row is left out.
    state(e, "uncounted", kDialogScreen, [](Stage& s) { present_terminal_report(s, {}); });
}

// --- DateDialog and TimeDialog ----------------------------------------------

constexpr Size kPickerScreen{40, 24};

widgets::DateDialogOptions date_options() {
    widgets::DateDialogOptions options;
    options.initial = widgets::DateValue{2026, 8, 19};
    options.today = widgets::DateValue{2026, 8, 9};
    return options;
}

void present_date(Stage& stage, widgets::DateDialogOptions options = date_options(),
                  const widgets::StandardStrings& strings = widgets::english_standard_strings()) {
    static_cast<void>(
        widgets::present_modal_date_dialog(stage.app(), stage.desktop(), stage.roles(), std::move(options), strings));
}

widgets::DateTimeLabels german_date_labels() {
    widgets::DateTimeLabels labels;
    labels.month_names = {"Januar", "Februar", "März",      "April",   "Mai",      "Juni",
                          "Juli",   "August",  "September", "Oktober", "November", "Dezember"};
    labels.weekday_names = {"Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};
    labels.am = "vorm.";
    labels.pm = "nachm.";
    return labels;
}

widgets::StandardStrings german_strings() {
    widgets::StandardStrings strings;
    strings.select_date_title = "Datum wählen";
    strings.select_time_title = "Zeit wählen";
    strings.cancel = "Abbrechen";
    return strings;
}

void add_date_dialog(Catalog& catalog) {
    Element& e = catalog.element("DateDialog", "include/cvision/widgets/date_time_dialog.hpp",
                                 Traits{.text = true, .window = true});
    state(e, "normal", kPickerScreen, [](Stage& s) { present_date(s); });
    state(e, "active", kPickerScreen, [](Stage& s) {
        frame(s, Rect{1, 1, 30, 10}, "Notes");
        present_date(s);
    });
    state(e, "inactive", kPickerScreen, [](Stage& s) {
        present_date(s);
        cover(s, Rect{20, 15, 19, 8});
    });
    // Only days up to the 24th may be chosen; the ISO week column is shown.
    state(e, "range", kPickerScreen, [](Stage& s) {
        widgets::DateDialogOptions options = date_options();
        options.maximum = widgets::DateValue{2026, 8, 24};
        options.show_iso_week_numbers = true;
        present_date(s, std::move(options));
    });
    // A year the calendar cannot draw, typed and refused: the dialog says why.
    state(e, "refused", kPickerScreen, [](Stage& s) {
        present_date(s);
        for (int i = 0; i < 4; ++i) s.app().dispatch(key(Key::Tab));
        for (const char c : std::string("1200")) s.app().dispatch(typed(std::string(1, c)));
        s.app().dispatch(key(Key::Enter));
    });
    // Every word from the host's tables.
    state(e, "labels", kPickerScreen, [](Stage& s) {
        widgets::DateDialogOptions options = date_options();
        options.labels = german_date_labels();
        present_date(s, std::move(options), german_strings());
    });
    state(e, "wide", kPickerScreen, [](Stage& s) {
        widgets::DateDialogOptions options = date_options();
        options.labels.month_names = std::vector<std::string>(12, std::string(kWideText));
        options.labels.weekday_names = {"月", "火", "水", "木", "金", "土", "日"};
        present_date(s, std::move(options));
    });
    // Less room than the dialog asks for: it is clamped to the desktop.
    state(e, "narrow", Size{22, 24}, [](Stage& s) { present_date(s); });
}

widgets::TimeDialogOptions time_options() {
    widgets::TimeDialogOptions options;
    options.initial = widgets::TimeValue{21, 41, 7};
    return options;
}

void present_time(Stage& stage, widgets::TimeDialogOptions options = time_options(),
                  const widgets::StandardStrings& strings = widgets::english_standard_strings()) {
    static_cast<void>(
        widgets::present_modal_time_dialog(stage.app(), stage.desktop(), stage.roles(), std::move(options), strings));
}

void add_time_dialog(Catalog& catalog) {
    Element& e = catalog.element("TimeDialog", "include/cvision/widgets/date_time_dialog.hpp",
                                 Traits{.window = true});
    state(e, "normal", kScreen, [](Stage& s) { present_time(s); });
    state(e, "active", kScreen, [](Stage& s) {
        frame(s, Rect{1, 1, 30, 8}, "Notes");
        present_time(s);
    });
    state(e, "inactive", kScreen, [](Stage& s) {
        present_time(s);
        cover(s, Rect{22, 5, 17, 6});
    });
    // The twelve-hour face without seconds, the meridiem and title in the
    // host's words.
    state(e, "labels", kScreen, [](Stage& s) {
        widgets::TimeDialogOptions options = time_options();
        options.show_seconds = false;
        options.hour_format = widgets::HourFormat::TwelveHour;
        options.labels = german_date_labels();
        present_time(s, std::move(options), german_strings());
    });
}

// --- DescriptorDialog ------------------------------------------------------

constexpr Size kFormScreen{44, 16};

widgets::DialogDescriptor export_form(std::string title = "Export report", std::string name = "release-notes") {
    widgets::DialogDescriptor descriptor;
    descriptor.title = std::move(title);
    widgets::FieldDescriptor field;
    field.label = "&Name";
    field.initial_text = std::move(name);
    field.validate = [](const std::string& text) { return !text.empty(); };
    field.description = "Written beside the source.";
    descriptor.fields.push_back(std::move(field));
    widgets::FieldDescriptor format;
    format.label = "&Format";
    format.kind = widgets::FieldKind::Combo;
    format.options = {"PDF", "HTML", "Plain text"};
    format.initial_selection = 0;
    descriptor.fields.push_back(std::move(format));
    widgets::FieldDescriptor overwrite;
    overwrite.label = "&Overwrite an existing file";
    overwrite.kind = widgets::FieldKind::Check;
    overwrite.initial_checked = true;
    descriptor.fields.push_back(std::move(overwrite));
    descriptor.buttons = {widgets::ButtonDescriptor{"E&xport", widgets::ButtonRole::Accept, [] {}},
                          widgets::ButtonDescriptor{"Cancel", widgets::ButtonRole::Dismiss, [] {}}};
    return descriptor;
}

void present_form(Stage& stage, widgets::DialogDescriptor descriptor) {
    static_cast<void>(
        widgets::present_modal_dialog(std::move(descriptor), stage.app(), stage.desktop(), stage.roles()));
}

void add_descriptor_dialog(Catalog& catalog) {
    Element& e =
        catalog.element("DescriptorDialog", "include/cvision/widgets/dialog.hpp", Traits{.text = true, .window = true});
    state(e, "normal", kFormScreen, [](Stage& s) { present_form(s, export_form()); });
    state(e, "active", kFormScreen, [](Stage& s) {
        frame(s, Rect{1, 1, 30, 8}, "Notes");
        present_form(s, export_form());
    });
    // The form stays up behind the alert it raised, which holds the
    // activation until it is answered.
    state(e, "inactive", kFormScreen, [](Stage& s) {
        present_form(s, export_form());
        present(s, message(widgets::MessageBoxKind::Confirm, "Replace", "Replace the existing file?",
                           widgets::MessageBoxButtons::YesNo));
    });
    state(e, "described", kFormScreen, [](Stage& s) {
        widgets::DialogDescriptor descriptor = export_form();
        descriptor.field_description_rows = 1;
        present_form(s, std::move(descriptor));
    });
    // A field's own validator refused the accept: the field is marked.
    state(e, "invalid", kFormScreen, [](Stage& s) {
        present_form(s, export_form("Export report", ""));
        s.app().dispatch(key(Key::Enter));
    });
    // Every field passed, and the form's own check vetoed the answer.
    state(e, "vetoed", kFormScreen, [](Stage& s) {
        widgets::DialogDescriptor descriptor = export_form();
        descriptor.check = [](const widgets::DialogResult&) {
            return std::optional<widgets::DialogVeto>{widgets::DialogVeto{"That name is taken.", 0}};
        };
        present_form(s, std::move(descriptor));
        s.app().dispatch(key(Key::Enter));
    });
    state(e, "wide", kFormScreen,
          [](Stage& s) { present_form(s, export_form(std::string(kWideText), std::string(kWideText))); });
    state(e, "narrow", Size{24, 12}, [](Stage& s) { present_form(s, export_form("Export the quarterly report")); });
}

// --- Application -----------------------------------------------------------

// What an application puts up: a menu bar, a status line, and a window laid
// out in the room the terminal gives it.
void application(Stage& stage) {
    const Commands ids = declare_commands(stage.app());
    stage.desktop().dock_top(std::make_unique<widgets::MenuBar>(menus(ids)));
    stage.desktop().dock_bottom(std::make_unique<widgets::StatusLine>())->set_items(legend(stage.app(), ids));
    frame(stage, Rect{2, 2, 30, 7}, "Notes");
    stage.desktop().tile();
}

void add_application(Catalog& catalog) {
    Element& e = catalog.element("Application", "include/cvision/ui/application.hpp", Traits{});
    state(e, "normal", kScreen, application);
    // The smallest terminal an application still draws on: below the full
    // chrome size, so what it shows is the windows clamped to fit.
    state(e, "hard-floor", ui::kHardFloorSize, application);
    fixed_state(e, "too-small", Size{ui::kHardFloorSize.width - 2, ui::kHardFloorSize.height - 1}, application,
                "the last-resort screen draws in its own colours, since it must work before any theme is configured");
}

}  // namespace

void add_chrome_specimens(Catalog& catalog) {
    add_desktop(catalog);
    add_window(catalog);
    add_editor_window(catalog);
    add_frame_text(catalog);
    add_minimized_window_stub(catalog);
    add_menu_bar(catalog);
    add_dropdown_menu(catalog);
    add_paged_strip(catalog);
    add_window_switcher_bar(catalog);
    add_status_line(catalog);
    add_tooltip(catalog);
    add_notification_center(catalog);
    add_message_box(catalog);
    add_file_dialog(catalog);
    add_directory_picker(catalog);
    add_window_list_dialog(catalog);
    add_theme_editor(catalog);
    add_help_viewer(catalog);
    add_terminal_report_dialog(catalog);
    add_date_dialog(catalog);
    add_time_dialog(catalog);
    add_descriptor_dialog(catalog);
    add_application(catalog);
}

}  // namespace ckv::docgen::appearance
