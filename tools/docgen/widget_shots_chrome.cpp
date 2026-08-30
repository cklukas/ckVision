// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Application chrome: the bars an application wears, the window frame
// itself, and the transient surfaces that come and go over them.
#include "widget_shots.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cvision/core/key.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/minimized_window_stub.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/window_switcher_bar.hpp"
#include "widget_stage.hpp"

namespace ckv::docgen {
namespace {

// The handful of commands the chrome figures present. Declared exactly
// as an application declares its own: one descriptor each, with the
// title, category and chord the menu, status line, toolbar and palette
// all read back out of the registry rather than restating.
struct DemoCommands {
    ui::CommandId open = ui::kInvalidCommand;
    ui::CommandId save = ui::kInvalidCommand;
    ui::CommandId print = ui::kInvalidCommand;
    ui::CommandId find = ui::kInvalidCommand;
    ui::CommandId replace_all = ui::kInvalidCommand;
    ui::CommandId tile = ui::kInvalidCommand;
};

DemoCommands declare_demo_commands(ui::Application& app) {
    ui::CommandRegistry& registry = app.commands();
    DemoCommands ids;
    ids.open = registry.declare({"doc.open", "&Open...", "File", "", "Ctrl+O",
                                 ui::CommandVisibility::Palette, [] {}});
    ids.save = registry.declare({"doc.save", "&Save", "File", "", "Ctrl+S",
                                 ui::CommandVisibility::Palette, [] {}});
    ids.print = registry.declare({"doc.print", "&Print", "File", "", "Ctrl+P",
                                  ui::CommandVisibility::Palette, [] {}});
    ids.find = registry.declare({"doc.find", "&Find...", "Search", "", "Ctrl+F",
                                 ui::CommandVisibility::Palette, [] {}});
    ids.replace_all = registry.declare({"doc.replace_all", "Replace &All", "Search", "", "",
                                        ui::CommandVisibility::Palette, [] {}});
    ids.tile = registry.declare({"win.tile", "&Tile", "Window", "", "",
                                 ui::CommandVisibility::Palette, [] {}});
    // Availability belongs to the command, not to any one surface that
    // shows it: this predicate is why Print greys on the menu AND on the
    // toolbar AND in the palette, without any of them being told twice.
    registry.set_enabled_predicate(ids.print, [] { return false; });
    return ids;
}

std::vector<widgets::MenuBarItem> demo_menus(const DemoCommands& ids) {
    return {
        widgets::MenuBarItem{"&File",
                             {widgets::MenuItem::command(ids.open),
                              widgets::MenuItem::command(ids.save),
                              widgets::MenuItem::separator(),
                              widgets::MenuItem::command(ids.print),  // greyed by its own enabled predicate
                              widgets::MenuItem::submenu(
                                  "Recen&t", {widgets::MenuItem::action("notes.md", [] {}),
                                              widgets::MenuItem::action("release.md", [] {})})}},
        widgets::MenuBarItem{"&Search",
                             {widgets::MenuItem::command(ids.find),
                              widgets::MenuItem::command(ids.replace_all)}},
        widgets::MenuBarItem{"&Window", {widgets::MenuItem::command(ids.tile)}},
    };
}

void shot_menu_bar_and_dropdown(const std::filesystem::path& dir) {
    WidgetStage stage;
    const DemoCommands ids = declare_demo_commands(stage.app());

    // ckvision-doc: menubar
    auto* bar = stage.desktop().dock_top(std::make_unique<widgets::MenuBar>(demo_menus(ids)));
    bar->on_highlight_changed = [](const widgets::MenuHighlight& highlight) {
        (void)highlight;  // e.g. mirror the help context into a status line
    };
    // ckvision-doc-end: menubar

    stage.window("Document", Rect{4, 3, 40, 8});
    stage.step();
    stage.save(dir, "widget-menubar", Rect{0, 0, 46, 4});

    // ckvision-doc: dropdownmenu
    bar->activate();  // F10 does this for the reader; Down drops the menu
    stage.app().dispatch(KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});
    // ckvision-doc-end: dropdownmenu

    stage.step();
    stage.save(dir, "widget-dropdownmenu", Rect{0, 0, 40, 12});
}

void shot_status_line(const std::filesystem::path& dir) {
    WidgetStage stage;
    const DemoCommands ids = declare_demo_commands(stage.app());

    // ckvision-doc: statusline
    auto* status = stage.desktop().dock_bottom(std::make_unique<widgets::StatusLine>());
    status->set_items({
        widgets::StatusLineItem{"~F1~ Help"},
        widgets::StatusLineItem{"~Ctrl+S~ Save", ids.save},
        widgets::StatusLineItem{"~Ctrl+P~ Print", ids.print},
        widgets::StatusLineItem{"~Alt+X~ Quit", stage.app().commands().standard().quit},
    });
    status->set_transient_hint("Saved package.json (1 284 bytes)");
    // ckvision-doc-end: statusline

    stage.window("Document", Rect{4, 2, 40, 8});
    stage.step();
    stage.save(dir, "widget-statusline", Rect{0, 20, 80, 4});
}

void shot_tool_bar(const std::filesystem::path& dir) {
    WidgetStage stage;
    const DemoCommands ids = declare_demo_commands(stage.app());
    ui::View& content = stage.dialog_window("Report", Rect{16, 6, 46, 8});

    // ckvision-doc: toolbar
    auto* tools = content.make<widgets::ToolBar>();
    tools->set_bounds(Rect{0, 0, 44, 1});
    tools->set_commands({ids.open, ids.save, ids.print, ids.find});
    // ckvision-doc-end: toolbar

    auto* body = content.make<widgets::StaticText>(
        "A ToolBar presents registered commands: it reads their titles and "
        "enablement from the registry and runs them through it.");
    body->set_bounds(Rect{0, 2, 44, 3});
    stage.step();
    stage.save_window(dir, "widget-toolbar");
}

void shot_command_palette(const std::filesystem::path& dir) {
    WidgetStage stage;
    declare_demo_commands(stage.app());
    ui::View& content = stage.dialog_window("Commands", Rect{18, 4, 44, 13});

    // ckvision-doc: commandpalette
    auto* palette = content.make<widgets::CommandPalette>();
    palette->set_bounds(Rect{1, 1, 40, 9});
    // An empty query offers everything the registry holds that is not
    // framework-only; typing narrows it, matching from the start of a
    // word rather than anywhere in the string.
    palette->set_query("");
    // ckvision-doc-end: commandpalette

    stage.focus(palette);
    stage.step();
    stage.save_window(dir, "widget-commandpalette");
}

void shot_notification_center(const std::filesystem::path& dir) {
    WidgetStage stage;
    stage.window("Deploy", Rect{4, 3, 50, 14});

    // ckvision-doc: notificationcenter
    auto* centre = stage.desktop().make<widgets::NotificationCenter>();
    centre->set_bounds(Rect{40, 2, 36, 6});
    centre->set_auto_dismiss(4'000'000'000);  // 4s for everything not persistent
    centre->add(widgets::Notification{widgets::NotificationSeverity::Info,
                                      "Build finished in 42 s", false});
    centre->add(widgets::Notification{widgets::NotificationSeverity::Warning,
                                      "2 tests were skipped", false});
    centre->add(widgets::Notification{widgets::NotificationSeverity::Error,
                                      "Upload refused: no credentials",
                                      /*persistent=*/true});
    centre->on_changed = [] { /* a post, a dismissal or an expiry */ };
    // ckvision-doc-end: notificationcenter

    stage.step();
    stage.save(dir, "widget-notificationcenter", Rect{38, 1, 40, 8});
}

void shot_tooltip(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Export", Rect{16, 5, 44, 9});
    auto* button = content.make<widgets::Button>("&Export");
    button->set_bounds(Rect{2, 2, 12, 2});

    // ckvision-doc: tooltip
    auto* tip = stage.desktop().make<widgets::Tooltip>("Writes report.pdf beside the source");
    tip->show_at(Point{20, 10});
    // ckvision-doc-end: tooltip

    stage.step();
    stage.save(dir, "widget-tooltip", Rect{15, 4, 48, 10});
}

void shot_window_chrome(const std::filesystem::path& dir) {
    WidgetStage stage;

    // ckvision-doc: window
    auto frame = std::make_unique<widgets::Window>("Report");
    frame->set_bounds(Rect{6, 3, 44, 9});
    frame->set_footer("2 of 7");
    frame->set_min_size(Size{20, 5});
    frame->set_minimizable(true);
    frame->set_resizable(true);
    frame->close_request = [] {
        return false;  // veto: something is unsaved
    };
    frame->set_content(std::make_unique<ui::View>());
    widgets::Window* window = stage.desktop().add_window(std::move(frame));
    // ckvision-doc-end: window

    auto* body = window->content()->make<widgets::StaticText>(
        "The frame draws the title, the close control, the minimize and "
        "maximize controls, the footer, and the resize grip.");
    body->set_bounds(Rect{1, 1, 40, 4});
    stage.step();
    stage.save(dir, "widget-window", Rect{5, 2, 48, 12});
}

void shot_desktop(const std::filesystem::path& dir) {
    WidgetStage stage;

    // ckvision-doc: desktop
    for (const char* title : {"Sources", "Build log", "Terminal"}) {
        auto frame = std::make_unique<widgets::Window>(title);
        frame->set_content(std::make_unique<ui::View>());
        stage.desktop().add_window(std::move(frame));
    }
    stage.desktop().tile();  // or cascade(); both are desktop-wide commands
    // ckvision-doc-end: desktop

    stage.step();
    stage.save(dir, "widget-desktop");
}

void shot_minimized_window_stub(const std::filesystem::path& dir) {
    WidgetStage stage;
    for (const char* title : {"config.yaml", "Build log"}) {
        auto frame = std::make_unique<widgets::Window>(title);
        frame->set_bounds(Rect{6, 3, 44, 9});
        frame->set_content(std::make_unique<ui::View>());
        stage.desktop().add_window(std::move(frame));
    }

    // ckvision-doc: minimizedwindowstub
    // An application never constructs one: a Desktop whose placement is
    // Parked puts a stub up when a window is minimized, and takes it down
    // again when the window comes back.
    stage.desktop().set_minimized_window_placement(
        widgets::MinimizedWindowPlacement::Parked);
    stage.desktop().windows().front()->set_minimized(true);
    stage.desktop().finish_minimize_animation();
    // ckvision-doc-end: minimizedwindowstub

    stage.step();
    stage.save(dir, "widget-minimizedwindowstub", Rect{0, 20, 36, 4});
}

void shot_window_switcher_bar(const std::filesystem::path& dir) {
    WidgetStage stage;
    for (const char* title : {"Sources", "Build log", "Terminal"}) {
        auto frame = std::make_unique<widgets::Window>(title);
        frame->set_bounds(Rect{4, 2, 40, 8});
        frame->set_content(std::make_unique<ui::View>());
        stage.desktop().add_window(std::move(frame));
    }
    // This figure is about the BAR, so the desktop must not also park a stub
    // for the window that is put away — which its default would (D-064), one
    // row above, saying the same thing twice. `HostListed` is what every
    // application with its own listing sets, ckmux included.
    stage.desktop().set_minimized_window_placement(
        widgets::MinimizedWindowPlacement::HostListed);
    stage.desktop().windows().front()->set_minimized(true);
    stage.desktop().finish_minimize_animation();

    // ckvision-doc: windowswitcherbar
    auto* switcher =
        stage.desktop().dock_bottom(std::make_unique<widgets::WindowSwitcherBar>(stage.desktop()));
    switcher->refresh();
    // ckvision-doc-end: windowswitcherbar

    stage.step();
    stage.save(dir, "widget-windowswitcherbar", Rect{0, 20, 80, 4});
}

void shot_paged_strip(const std::filesystem::path& dir) {
    WidgetStage stage;

    // ckvision-doc: pagedstrip
    auto* strip = stage.desktop().dock_bottom(std::make_unique<widgets::PagedStrip>());
    // The strip pulls its items rather than being handed them: a host
    // whose model moved calls refresh_items() and the source is asked
    // again.
    strip->set_item_source([] {
        std::vector<widgets::PagedStrip::Item> items;
        for (const auto& [text, selected] : std::initializer_list<std::pair<const char*, bool>>{
                 {"editor", true}, {"shell", false}, {"monitor", false},
                 {"release notes", false}, {"changelog", false}}) {
            widgets::PagedStrip::Item item;
            item.text = text;
            // The provider's own answer, never a measurement the strip takes
            // of `text`: an item carrying a leading glyph says so here.
            item.width = static_cast<int>(item.text.size());
            item.selected = selected;
            items.push_back(std::move(item));
        }
        return items;
    });
    strip->on_item_activated = [](std::size_t index) { (void)index; };
    strip->on_collapse_changed = [](bool collapsed) { (void)collapsed; };
    // ckvision-doc-end: pagedstrip

    stage.step();
    stage.save(dir, "widget-pagedstrip", Rect{0, 20, 80, 4});
}

}  // namespace

void capture_chrome_shots(const std::filesystem::path& dir) {
    shot_menu_bar_and_dropdown(dir);
    shot_status_line(dir);
    shot_tool_bar(dir);
    shot_command_palette(dir);
    shot_notification_center(dir);
    shot_tooltip(dir);
    shot_window_chrome(dir);
    shot_desktop(dir);
    shot_minimized_window_stub(dir);
    shot_window_switcher_bar(dir);
    shot_paged_strip(dir);
}

}  // namespace ckv::docgen
