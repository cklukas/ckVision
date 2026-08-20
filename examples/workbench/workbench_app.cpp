// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/message_box.hpp"
#include "workbench_app.hpp"

#include <memory>
#include <vector>

#include "cvision/core/style.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/flow_view.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/memo.hpp"
#include "cvision/widgets/progress.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/tab_control.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/text_view.hpp"
#include "cvision/widgets/tree_view.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::workbench {
WorkbenchApp::WorkbenchApp(ui::Application& app) : app_(app), roles_(ui::intern_standard_roles(app.roles())) {
    app_.theme() = ui::make_classic_theme(app_.roles(), roles_);

    auto desktop = std::make_unique<widgets::Desktop>(app_.root().bounds());
    desktop_ = desktop.get();
    app_.root().add_child(std::move(desktop));

    build_chrome();
    build_window();

    // CommandPalette presents the commands that declare themselves
    // browsable, which is the application's own vocabulary rather than
    // the framework's navigation plumbing. Give the workbench's Help tab
    // a genuine app-level action so the example demonstrates that
    // contract directly.
    app_.commands().declare({.key = "workbench.build-project",
                             .title = "&Build project",
                             .category = "Build",
                             .chord = "F7",
                             .handler = [] {}});
    app_.commands().set_handler(app_.commands().standard().quit, [this] { app_.request_quit(); });
    app_.set_focus(memo_);

    // F1 answers with something. Silence is the one response a reader
    // cannot tell apart from a key that never arrived.
    widgets::install_about_help(app_, *desktop_, roles_,
                                "ckVision Workbench example",
                                "An application template with text, data and utility tabs.");
}

void WorkbenchApp::build_chrome() {
    widgets::MenuBarItem file_menu{"&File", {}};
    file_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().quit}));
    widgets::MenuBarItem window_menu{"&Window", {}};
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().next_window}));
    window_menu.items.push_back(widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().previous_window}));

    desktop_->dock_top(std::make_unique<widgets::MenuBar>(
        std::vector<widgets::MenuBarItem>{std::move(file_menu), std::move(window_menu)}));

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
    memo->set_bounds(Rect{1, 1, 36, 10});
    memo->set_wrap_mode(widgets::WrapMode::Word);
    memo->set_text("ckVision memo\nclipboard, undo, and wrapping live here.");
    memo_ = memo.get();
    page->add_child(std::move(memo));

    auto command_label = std::make_unique<widgets::Label>("&Command:");
    command_label->set_bounds(Rect{1, 12, 10, 1});
    page->add_child(std::move(command_label));

    auto command = std::make_unique<widgets::InputLine>();
    command->set_bounds(Rect{12, 12, 24, 1});
    command->set_history(&history_, "workbench.command");
    command->set_text("build");
    command->commit_to_history();
    command->set_text("test");
    command->commit_to_history();
    command_input_ = command.get();
    page->add_child(std::move(command));

    auto toolbar = std::make_unique<widgets::ToolBar>();
    toolbar->set_bounds(Rect{1, 14, 34, 1});
    toolbar->set_commands({app_.commands().standard().menu, app_.commands().standard().quit});
    tool_bar_ = toolbar.get();
    page->add_child(std::move(toolbar));

    auto text = std::make_unique<widgets::TextView>();
    text->set_bounds(Rect{39, 1, 30, 12});
    text->set_spans({widgets::TextSpan{"TextView links export as ", static_cast<Attr>(0), std::nullopt},
                     widgets::TextSpan{"OSC 8", Attr::Underline, std::string{"https://example.invalid/osc8"}},
                     widgets::TextSpan{" and activate deterministically.", static_cast<Attr>(0), std::nullopt}});
    text->set_current_link(0);
    text->on_link_activate = [this](const std::string& target) { last_link_ = target; };
    text_view_ = text.get();
    page->add_child(std::move(text));

    auto flow = std::make_unique<widgets::FlowView>();
    flow->set_bounds(Rect{39, 14, 30, 3});
    auto chart = std::make_shared<Image>(4, 1);
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
    tree->set_bounds(Rect{1, 1, 22, 10});
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
    list->set_bounds(Rect{25, 1, 18, 10});
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
    combo->set_bounds(Rect{1, 12, 18, 4});
    combo->set_items({"debug", "release", "asan"});
    combo->set_selected_index(1);
    combo_ = combo.get();
    page->add_child(std::move(combo));

    auto progress = std::make_unique<widgets::Progress>();
    progress->set_bounds(Rect{25, 13, 32, 1});
    progress->set_fraction(0.625);
    progress->set_label("62%");
    progress_ = progress.get();
    page->add_child(std::move(progress));

    auto search = std::make_unique<widgets::SearchBox>();
    search->set_bounds(Rect{1, 15, 22, 1});
    search->set_query("alpha");
    search_box_ = search.get();
    page->add_child(std::move(search));

    auto breadcrumb = std::make_unique<widgets::BreadcrumbBar>();
    breadcrumb->set_bounds(Rect{25, 15, 30, 1});
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
    inspector->set_items({widgets::PropertyItem{"Theme", "Classic", true},
                          widgets::PropertyItem{"Mode", "Demo", true}});
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

}  // namespace ckv::workbench
