// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/message_box.hpp"
#include "workbench_app.hpp"

#include "../example_about.hpp"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "cvision/core/style.hpp"
#include "cvision/ui/theme_format.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/flow_view.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/key_chord_capture.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/memo.hpp"
#include "cvision/widgets/progress.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/tab_control.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/text_view.hpp"
#include "cvision/widgets/theme_editor.hpp"
#include "cvision/widgets/tree_view.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::workbench {
WorkbenchApp::WorkbenchApp(ui::Application& app, WorkbenchThemeFile theme_file)
    : app_(app), roles_(ui::intern_standard_roles(app.roles())), theme_file_(std::move(theme_file)) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds());
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));

    // CommandPalette presents the commands that declare themselves
    // browsable, which is the application's own vocabulary rather than
    // the framework's navigation plumbing. Give the workbench's Help tab
    // a genuine app-level action so the example demonstrates that
    // contract directly.
    build_command_ = app_.commands().declare({.key = "workbench.build-project",
                                              .title = "&Build project",
                                              .category = "Build",
                                              .chord = "F7",
                                              .handler = [this] { run_build(); }});
    console_command_ = app_.commands().declare({.key = "workbench.console",
                                                .title = "C&onsole",
                                                .category = "Window",
                                                .handler = [this] { open_console(); }});
    keys_command_ = app_.commands().declare({.key = "workbench.keys",
                                             .title = "&Keys...",
                                             .category = "Options",
                                             .handler = [this] { present_keys_dialog(); }});
    theme_command_ = app_.commands().declare({.key = "workbench.edit-theme",
                                              .title = "Edit &theme...",
                                              .category = "View",
                                              .handler = [this] { edit_theme(); }});

    build_chrome();
    build_window();

    // Short explanations for the controls whose use is not plain at a
    // glance: shown when the pointer rests on one or the focus arrives, and
    // at once with the tooltip key.
    tips_.emplace(app_, *desktop_);
    tips_->set_tip(*command_palette_, "Type to filter the commands; Enter runs the highlighted one");
    tips_->set_tip(*property_inspector_, "Enter edits the value on the cursor row");

    app_.commands().set_handler(app_.commands().standard().quit, [this] { app_.request_quit(); });
    app_.set_focus(memo_);

    // F1 answers with something. Silence is the one response a reader
    // cannot tell apart from a key that never arrived.
    widgets::install_about_help(app_, *desktop_, roles_,
                                "ckVision Workbench example",
                                ckv::examples::about_text(
                                    "An application template with text, data and utility tabs."));
    // Last, so that a notice about an unreadable theme file opens over the
    // finished window and keeps the focus.
    load_theme();
}

void WorkbenchApp::build_chrome() {
    const ui::StandardCommands& standard = app_.commands().standard();
    widgets::MenuBarItem file_menu{"&File", {}};
    file_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{standard.help, "&About..."}));
    file_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{keys_command_}));
    file_menu.items.push_back(widgets::MenuItem::separator());
    file_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{standard.quit}));
    widgets::MenuBarItem view_menu{"&View", {}};
    view_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{theme_command_}));
    std::vector<widgets::MenuItem> tab_presentations;
    for (const auto& [key, title, presentation] : {
             std::tuple{"workbench.tabs-underlined", "&Underlined tabs", widgets::TabPresentation::Underlined},
             std::tuple{"workbench.tabs-framed", "&Framed tabs", widgets::TabPresentation::Framed},
             std::tuple{"workbench.tabs-compact", "&Compact tabs", widgets::TabPresentation::Compact}}) {
        const auto command = app_.commands().declare({
            .key = key, .title = title, .category = "View",
            .handler = [this, presentation] { tabs_->set_presentation(presentation); }});
        tab_presentations.push_back(widgets::MenuItem::command(widgets::CommandPresentation{command}));
    }
    view_menu.items.push_back(widgets::MenuItem::submenu("Tab &presentation", std::move(tab_presentations)));
    std::vector<widgets::MenuItem> toolbar_presentations;
    for (const auto& [key, title, presentation] : {
             std::tuple{"workbench.toolbar-compact", "&Compact buttons", widgets::ToolBarPresentation::Compact},
             std::tuple{"workbench.toolbar-padded", "&Padded buttons", widgets::ToolBarPresentation::Padded},
             std::tuple{"workbench.toolbar-framed", "&Framed buttons", widgets::ToolBarPresentation::Framed}}) {
        const auto command = app_.commands().declare({
            .key = key, .title = title, .category = "View",
            .handler = [this, presentation] {
                tool_bar_->set_presentation(presentation);
                tool_bar_->set_bounds(Rect{1, 10, 34, presentation == widgets::ToolBarPresentation::Framed ? 3 : 1});
            }});
        toolbar_presentations.push_back(widgets::MenuItem::command(widgets::CommandPresentation{command}));
    }
    view_menu.items.push_back(widgets::MenuItem::submenu("&Toolbar presentation", std::move(toolbar_presentations)));
    // Every way a desktop's windows are reached by keyboard: by cycling, by
    // number (Alt+1..Alt+9, the chord each numbered item shows), and by list.
    widgets::MenuBarItem window_menu{"&Window", {}};
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{console_command_}));
    window_menu.items.push_back(widgets::MenuItem::separator());
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{standard.next_window}));
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{standard.previous_window}));
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{standard.select_window[0]}));
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{standard.select_window[1]}));
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{standard.window_list}));
    window_menu.items.push_back(widgets::MenuItem::separator());
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{standard.zoom}));
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{standard.close}));
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{standard.tile}));
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{standard.cascade}));

    desktop_->dock_top(std::make_unique<widgets::MenuBar>(
        std::vector<widgets::MenuBarItem>{std::move(file_menu), std::move(view_menu), std::move(window_menu)}));

    auto status = std::make_unique<widgets::StatusLine>();
    status->set_items({widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().menu}},
                       widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().focus_next}},
                       widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().quit}}});
    desktop_->dock_bottom(std::move(status));
}

void WorkbenchApp::build_window() {
    auto window = std::make_unique<widgets::Window>("Workbench");
    window->set_bounds(Rect{2, 2, 74, 20});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);

    auto tabs = std::make_unique<widgets::TabControl>();
    tabs->set_bounds(Rect{0, 0, 72, 18});
    tabs_ = tabs.get();
    tabs->add_tab("&Text", build_text_page());
    tabs->add_tab("&Data", build_data_page());
    tabs->add_tab("&Help", build_help_page());
    window->set_content(std::move(tabs));
    window_ = desktop_->add_window(std::move(window));
}

std::unique_ptr<ui::View> WorkbenchApp::build_text_page() {
    auto page = std::make_unique<ui::View>();

    auto memo = std::make_unique<widgets::Memo>();
    memo->set_bounds(Rect{1, 1, 36, 6});
    memo->set_wrap_mode(widgets::WrapMode::Word);
    memo->set_text("ckVision memo\nclipboard, undo, and wrapping live here.");
    memo_ = memo.get();
    page->add_child(std::move(memo));

    auto command_label = std::make_unique<widgets::Label>("&Command:");
    command_label->set_bounds(Rect{1, 8, 10, 1});
    page->add_child(std::move(command_label));

    auto command = std::make_unique<widgets::InputLine>();
    command->set_bounds(Rect{12, 8, 24, 1});
    // The field recalls the application's own history list under this key,
    // seeded here with two earlier commands, newest last.
    app_.history().record("workbench.command", "build");
    app_.history().record("workbench.command", "test");
    command->set_history_key("workbench.command");
    command->set_text("test");
    command_input_ = command.get();
    page->add_child(std::move(command));

    auto toolbar = std::make_unique<widgets::ToolBar>();
    toolbar->set_bounds(Rect{1, 10, 34, 1});
    toolbar->set_groups({{widgets::CommandPresentation{build_command_}, widgets::CommandPresentation{console_command_}},
                         {widgets::CommandPresentation{app_.commands().standard().quit}}});
    tool_bar_ = toolbar.get();
    page->add_child(std::move(toolbar));

    auto text = std::make_unique<widgets::TextView>();
    text->set_bounds(Rect{39, 1, 30, 10});
    text->set_spans({widgets::TextSpan{"TextView links export as ", static_cast<Attr>(0), std::nullopt},
                     widgets::TextSpan{"OSC 8", Attr::Underline, std::string{"https://example.invalid/osc8"}},
                     widgets::TextSpan{" and activate deterministically.", static_cast<Attr>(0), std::nullopt}});
    text->set_current_link(0);
    text->on_link_activate = [this](const std::string& target) { last_link_ = target; };
    text_view_ = text.get();
    page->add_child(std::move(text));

    auto flow = std::make_unique<widgets::FlowView>();
    flow->set_bounds(Rect{39, 12, 30, 2});
    auto chart = std::make_shared<Image>(PixelSize{4, 1});
    for (int x = 0; x < chart->width(); ++x) chart->set_pixel(x, 0, Image::Rgba{0, 180, 120, 255});
    flow->set_document(widgets::FlowDocument{{widgets::FlowBlock{{
        widgets::FlowText{"Flow: ", static_cast<Attr>(0), std::nullopt},
        widgets::FlowText{"interactive link", Attr::Underline, std::string{"https://example.invalid/flow"}},
        widgets::FlowImage{std::move(chart), Size{7, 1}, "[chart]"},
    }}}});
    flow->on_link_activate = [this](const std::string& target) { last_link_ = target; };
    flow_view_ = flow.get();
    page->add_child(std::move(flow));

    return page;
}

std::unique_ptr<ui::View> WorkbenchApp::build_data_page() {
    auto page = std::make_unique<ui::View>();

    auto tree = std::make_unique<widgets::TreeView>();
    tree->set_bounds(Rect{1, 1, 22, 8});
    tree->set_connector_style(widgets::TreeConnectorStyle::BoxDrawing);
    widgets::TreeNode src;
    src.label = "src";
    widgets::TreeNode tests;
    tests.label = "tests";
    widgets::TreeNode project;
    project.label = "Project";
    project.children = {std::move(src), std::move(tests)};
    project.expanded = true;
    tree->set_roots({std::move(project)});
    tree_ = tree.get();
    page->add_child(std::move(tree));

    auto list = std::make_unique<widgets::ListView>(true);
    list->set_bounds(Rect{25, 1, 18, 8});
    list->set_items({"alpha", "beta", "gamma"});
    list->set_selected(0, true);
    list_ = list.get();
    page->add_child(std::move(list));

    auto table = std::make_unique<widgets::Table>();
    table->set_bounds(Rect{45, 1, 24, 8});
    table->set_columns({widgets::TableColumn{"Name", 10, 4}, widgets::TableColumn{"State", 10, 4}});
    table->set_rows({{"alpha", "ready"}, {"beta", "blocked"}, {"gamma", "done"}});
    table_ = table.get();
    page->add_child(std::move(table));

    auto combo = std::make_unique<widgets::ComboBox>(widgets::ComboBoxMode::PickOnly);
    combo->set_bounds(Rect{1, 10, 18, 1});
    combo->set_items({"debug", "release", "asan"});
    combo->set_selected_index(1);
    combo_ = combo.get();
    page->add_child(std::move(combo));

    auto progress = std::make_unique<widgets::Progress>();
    progress->set_bounds(Rect{25, 10, 32, 1});
    progress->set_fraction(0.625);
    progress->set_presentation(widgets::ProgressPresentation::Smooth);
    progress->set_show_percentage(true);
    progress_ = progress.get();
    page->add_child(std::move(progress));

    auto search = std::make_unique<widgets::SearchBox>();
    search->set_bounds(Rect{1, 12, 22, 1});
    search->set_query("alpha");
    search_box_ = search.get();
    page->add_child(std::move(search));

    auto breadcrumb = std::make_unique<widgets::BreadcrumbBar>();
    breadcrumb->set_bounds(Rect{25, 12, 30, 1});
    breadcrumb->set_segments({"workspace", "src", "widgets"});
    breadcrumb_ = breadcrumb.get();
    page->add_child(std::move(breadcrumb));

    return page;
}

std::unique_ptr<ui::View> WorkbenchApp::build_help_page() {
    auto page = std::make_unique<ui::View>();
    auto text = std::make_unique<widgets::TextView>();
    text->set_bounds(Rect{1, 1, 66, 12});
    text->set_text("Workbench combines ordinary app surfaces: text editing, data browsing, commands, tabs, and progress.");
    page->add_child(std::move(text));

    auto palette = std::make_unique<widgets::CommandPalette>();
    palette->set_bounds(Rect{1, 4, 28, 6});
    palette->set_query("build");
    command_palette_ = palette.get();
    page->add_child(std::move(palette));

    auto inspector = std::make_unique<widgets::PropertyInspector>();
    inspector->set_bounds(Rect{32, 4, 28, 5});
    widgets::PropertyItem theme{"Theme", "Classic", true, widgets::PropertyKind::Choice};
    theme.choices = {"Classic", "Dark", "Light", "Mono"};
    inspector->set_items({std::move(theme), widgets::PropertyItem{"Mode", "Demo", true}});
    property_inspector_ = inspector.get();
    page->add_child(std::move(inspector));

    auto notifications = std::make_unique<widgets::NotificationCenter>();
    notifications->set_bounds(Rect{1, 11, 32, 2});
    notifications->add(widgets::Notification{widgets::NotificationSeverity::Info, "Indexed commands are searchable", true});
    notifications_ = notifications.get();
    page->add_child(std::move(notifications));

    auto tooltip = std::make_unique<widgets::Tooltip>("Tooltip / popover text");
    tooltip->show_at(Point{36, 11});
    tooltip_ = tooltip.get();
    page->add_child(std::move(tooltip));
    return page;
}

// The build log, in a window of its own. A console reads as a console on a
// dark ground whatever the desktop around it looks like, so this one window
// keeps the dark scheme while the application stays classic: the override
// re-themes the window's whole subtree, frame included, and nothing else.
void WorkbenchApp::open_console() {
    if (console_window_ != nullptr) {
        desktop_->activate(console_window_);
        return;
    }
    auto window = std::make_unique<widgets::Window>("Console");
    window->set_bounds(Rect{30, 11, 46, 10});
    window->set_theme_override(ui::make_dark_theme(app_.roles(), roles_));
    auto text = std::make_unique<widgets::TextView>();
    text->set_bounds(window->content_rect());
    console_text_ = text.get();
    window->set_content(std::move(text));
    widgets::Window* const opened = window.get();
    window->on_closed = [this, opened] {
        console_window_ = nullptr;
        console_text_ = nullptr;
        widgets::schedule_self_detach(*opened, app_);
    };
    console_window_ = desktop_->add_window(std::move(window));
    if (console_lines_.empty()) console_lines_.emplace_back("workbench console ready");
    show_console_lines();
}

void WorkbenchApp::append_console_line(std::string line) {
    console_lines_.push_back(std::move(line));
    show_console_lines();
}

void WorkbenchApp::show_console_lines() {
    if (console_text_ == nullptr) return;
    std::string joined;
    for (const std::string& line : console_lines_) joined += line + "\n";
    console_text_->set_text(std::move(joined));
}

void WorkbenchApp::run_build() {
    ++builds_run_;
    append_console_line("build " + std::to_string(builds_run_) + ": ok");
}

// Rebinding at runtime: one row per command, each a KeyChordCapture showing
// the chord bound now. Enter starts a capture and the next key pressed
// becomes the binding; Backspace clears it. The registry is the only record,
// so the menus, the status line and the command palette show the new chord
// on the next frame, and the chord works as soon as the dialog closes.
void WorkbenchApp::present_keys_dialog() {
    if (keys_dialog_ != nullptr) return;
    const ui::StandardCommands& standard = app_.commands().standard();
    const std::vector<std::pair<ui::CommandId, std::string>> commands = {
        {build_command_, "&Build project"},
        {console_command_, "C&onsole"},
        {standard.next_window, "&Next window"},
        {standard.zoom, "&Zoom"},
    };
    auto window = std::make_unique<widgets::Window>("Keys");
    const int height = static_cast<int>(commands.size()) + 6;
    const Rect area = desktop_->content_area();
    window->set_bounds(Rect{area.x + (area.width - 44) / 2, area.y + (area.height - height) / 2, 44, height});
    window->set_role_override(roles_.dialog_frame, roles_.dialog_background, roles_.dialog_frame,
                              roles_.dialog_background);
    window->set_resizable(false);
    auto content = std::make_unique<ui::View>();
    key_rows_.clear();
    for (std::size_t index = 0; index < commands.size(); ++index) {
        const int row = static_cast<int>(index) + 1;
        auto label = std::make_unique<widgets::Label>(commands[index].second);
        label->set_bounds(Rect{2, row, 16, 1});
        widgets::Label* const label_view = label.get();
        content->add_child(std::move(label));
        auto capture = std::make_unique<widgets::KeyChordCapture>();
        capture->set_bounds(Rect{19, row, 20, 1});
        capture->set_chord(app_.commands().chord_for_command(commands[index].first));
        const ui::CommandId command = commands[index].first;
        capture->on_chord_changed = [this, command](const std::optional<KeyChord>& chord) { rebind(command, chord); };
        label_view->set_buddy(capture.get());
        key_rows_.push_back(KeyBindingRow{command, capture.get()});
        content->add_child(std::move(capture));
    }
    auto close = std::make_unique<widgets::Button>("&Close");
    close->set_bounds(Rect{16, static_cast<int>(commands.size()) + 2, 10, 2});
    widgets::Window* const dialog = window.get();
    close->on_press = [dialog] { dialog->close(); };
    content->add_child(std::move(close));
    window->set_content(std::move(content));
    window->cancel_request = [dialog] { dialog->close(); };
    window->on_closed = [this, dialog] {
        keys_dialog_ = nullptr;
        key_rows_.clear();
        widgets::schedule_self_detach(*dialog, app_);
    };
    ui::View* const first = key_rows_.front().capture;
    keys_dialog_ = desktop_->present_modal(widgets::WindowHandle{std::move(window), first}, app_);
}

void WorkbenchApp::rebind(ui::CommandId command, const std::optional<KeyChord>& chord) {
    ui::CommandRegistry& commands = app_.commands();
    while (const std::optional<KeyChord> bound = commands.chord_for_command(command)) commands.unbind_key(*bound);
    if (chord) commands.bind_key(*chord, command);
    for (const KeyBindingRow& row : key_rows_) row.capture->set_chord(commands.chord_for_command(row.command));
}

// ckvision-doc: workbench-edit-theme
// The theme editor edits a copy of the application's theme. Accepting it
// installs the edited theme -- set_theme repaints every surface -- and saves
// its text; cancelling leaves everything as it was.
void WorkbenchApp::edit_theme() {
    dialogs_.await(widgets::present_modal_theme_editor(app_.theme(), app_, *desktop_, roles_),
                   [this](widgets::ThemeEditorResult result) {
                       if (!result.theme) return;
                       app_.set_theme(*result.theme);
                       save_theme(*result.theme);
                   });
}

void WorkbenchApp::save_theme(const ui::Theme& theme) {
    if (theme_file_.files == nullptr) return;
    FileSystem& files = *theme_file_.files;
    files.create_directories(files.parent(theme_file_.path));
    const FileWriteResult written = files.write_file_atomic(theme_file_.path, ui::serialize_theme(theme));
    if (written.status != FileWriteStatus::Ok)
        report_theme_file("The theme could not be saved to " + theme_file_.path + ".");
}

void WorkbenchApp::load_theme() {
    if (theme_file_.files == nullptr) return;
    const std::optional<FileReadResult> saved = theme_file_.files->read_file(theme_file_.path);
    if (!saved) return;
    ui::ThemeParseResult parsed = ui::parse_theme(saved->contents, app_.theme());
    if (!parsed) {
        report_theme_file("The saved theme in " + theme_file_.path + " could not be read (line " +
                          std::to_string(parsed.error.line) + ": " + parsed.error.message +
                          "). The classic theme is used instead.");
        return;
    }
    app_.theme() = std::move(*parsed.theme);
}
// ckvision-doc-end: workbench-edit-theme

void WorkbenchApp::report_theme_file(std::string message) {
    dialogs_.await(widgets::present_modal_message_box(
        app_, *desktop_, roles_,
        widgets::MessageBoxDescriptor{widgets::MessageBoxKind::Error, "Theme", std::move(message),
                                      widgets::MessageBoxButtons::Ok}));
}

}  // namespace ckv::workbench
