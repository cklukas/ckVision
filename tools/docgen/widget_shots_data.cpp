// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Views that show a collection: lists, trees, tables, calendars, and
// the scrolling machinery they share.
#include "widget_shots.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cvision/widgets/button.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/widgets/scrollbar.hpp"
#include "cvision/widgets/splitter.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/tab_control.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/tree_view.hpp"
#include "widget_stage.hpp"

namespace ckv::docgen {
namespace {

void shot_list_view(const std::filesystem::path& dir) {
    WidgetStage stage;
    auto list = std::make_unique<widgets::ListView>(/*multi_select=*/true);

    // ckvision-doc: listview
    list->set_items({"applied-physics.md", "boot-sequence.md", "capabilities.md",
                     "dialogs.md", "editor.md", "fuzzing.md", "graphics.md",
                     "input-decoder.md", "layout.md", "themes.md"});
    list->set_cursor(2);
    list->set_selected(2, true);
    list->set_selected(4, true);
    list->set_scrollbar_policy(widgets::ScrollbarPolicy::Auto);
    list->on_activate = [](std::size_t index) { (void)index; /* open the row */ };
    list->on_selection_changed = [](std::size_t index) { (void)index; };
    // ckvision-doc-end: listview

    widgets::ListView* view = list.get();
    stage.window_with_content("Documents", Rect{22, 4, 34, 12}, std::move(list));
    stage.focus(view);
    stage.step();
    stage.save_window(dir, "widget-listview");
}

void shot_tree_view(const std::filesystem::path& dir) {
    WidgetStage stage;
    auto tree = std::make_unique<widgets::TreeView>();

    // ckvision-doc: treeview
    widgets::TreeNode core;
    core.label = "core";
    widgets::TreeNode widgets_dir;
    widgets_dir.label = "widgets";

    widgets::TreeNode cvision;
    cvision.label = "cvision";
    cvision.expanded = true;
    cvision.children = {std::move(core), std::move(widgets_dir)};

    widgets::TreeNode include;
    include.label = "include";
    include.expanded = true;
    include.children.push_back(std::move(cvision));

    widgets::TreeNode src;
    src.label = "src";
    src.children_known = false;  // an expander, with the listing not yet done

    widgets::TreeNode readme;
    readme.label = "README.md";

    tree->set_roots({std::move(include), std::move(src), std::move(readme)});
    tree->set_connector_style(widgets::TreeConnectorStyle::Outline);
    tree->on_expand_request = [](widgets::TreeNode& node) {
        (void)node;  // fill node.children in place; the tree redraws with them
    };
    tree->on_activate = [](widgets::TreeNode& node) { (void)node; };
    // ckvision-doc-end: treeview

    widgets::TreeView* view = tree.get();
    stage.window_with_content("Project", Rect{22, 5, 34, 11}, std::move(tree));
    stage.focus(view);
    stage.step();
    stage.save_window(dir, "widget-treeview");
}

void shot_table(const std::filesystem::path& dir) {
    WidgetStage stage;
    auto table = std::make_unique<widgets::Table>();

    // ckvision-doc: table
    table->set_columns({
        widgets::TableColumn{"Suite", 22, 8, widgets::TableCellType::Text, false},
        widgets::TableColumn{"Cases", 7, 4, widgets::TableCellType::Integer, false},
        widgets::TableColumn{"Owner", 12, 5, widgets::TableCellType::Text, true},
    });
    table->set_rows({
        {"test_application", "148", "core"},
        {"test_editor", "96", "editor"},
        {"test_frame_svg", "12", "docgen"},
        {"test_table", "54", "widgets"},
    });
    // With set_rows() and no TableModel the built-in order is a plain
    // text comparison of that column; a model decides its own order in
    // request_sort() instead.
    table->sort_by(0, /*ascending=*/true);
    table->set_selected_cell(widgets::TableCellRef{0, 2});
    table->on_edit_committed = [](widgets::TableCellRef cell,
                                  const widgets::TableEditResult& result) {
        (void)cell;
        (void)result;
    };
    // ckvision-doc-end: table

    widgets::Table* view = table.get();
    stage.window_with_content("Test inventory", Rect{16, 5, 48, 10}, std::move(table));
    stage.focus(view);
    stage.step();
    stage.save_window(dir, "widget-table");

    // The in-place cell editor is a state worth its own figure.
    view->begin_edit();
    stage.step();
    stage.save_window(dir, "widget-table-editing");
}

void shot_property_inspector(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Properties", Rect{22, 6, 36, 9});

    // ckvision-doc: propertyinspector
    auto* inspector = content.make<widgets::PropertyInspector>();
    inspector->set_bounds(Rect{1, 1, 32, 5});
    inspector->set_items({
        widgets::PropertyItem{"Title", "Release notes", true},
        widgets::PropertyItem{"Encoding", "UTF-8", false},
        widgets::PropertyItem{"Read only", "no", true},
        widgets::PropertyItem{"Lines", "1 284", false},
    });
    inspector->on_change = [](std::size_t index, std::string value) { (void)index; (void)value; };
    // ckvision-doc-end: propertyinspector

    stage.focus(inspector);
    stage.step();
    stage.save_window(dir, "widget-propertyinspector");
}

void shot_calendar_view(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Schedule", Rect{24, 4, 32, 13});

    // ckvision-doc: calendarview
    auto* calendar = content.make<widgets::CalendarView>();
    calendar->set_bounds(Rect{1, 1, 28, 9});
    calendar->set_month(widgets::DateValue{2026, 8, 1});
    calendar->set_selected(widgets::DateValue{2026, 8, 19});
    calendar->set_today(widgets::DateValue{2026, 8, 9});
    calendar->set_marked_span(widgets::DateValue{2026, 8, 24}, widgets::DateValue{2026, 8, 28});
    calendar->set_first_weekday(widgets::Weekday::Monday);
    calendar->set_show_iso_week_numbers(true);
    calendar->on_select = [](widgets::DateValue day) { (void)day; };
    // ckvision-doc-end: calendarview

    stage.focus(calendar);
    stage.step();
    stage.save_window(dir, "widget-calendarview");
}

void shot_calendar_dropdown(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Journal", Rect{18, 3, 44, 6});
    auto* anchor = content.make<widgets::Button>("Date...");
    anchor->set_bounds(Rect{26, 1, 12, 2});

    // ckvision-doc: calendardropdown
    widgets::CalendarDropdown* month =
        widgets::show_calendar_dropdown(*anchor, stage.app(), stage.desktop());
    month->show_month(widgets::DateValue{2026, 8, 1});
    month->calendar().set_selected(widgets::DateValue{2026, 8, 19});
    month->calendar().on_select = [](widgets::DateValue day) { (void)day; };
    // ckvision-doc-end: calendardropdown

    stage.step();
    stage.save(dir, "widget-calendardropdown", Rect{17, 2, 47, 19});
}

void shot_clock_view(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Status", Rect{24, 8, 32, 6});

    // ckvision-doc: clockview
    auto* clock = content.make<widgets::ClockView>();
    clock->set_bounds(Rect{2, 1, 14, 1});
    clock->set_time_provider([] { return widgets::TimeValue{9, 41, 7}; });
    clock->set_show_seconds(true);
    clock->set_hour_format(widgets::HourFormat::TwelveHour);
    clock->set_meridiem_labels("AM", "PM");
    clock->on_click = [] { /* drop a calendar under it */ };
    // ckvision-doc-end: clockview

    auto* plain = content.make<widgets::ClockView>();
    plain->set_bounds(Rect{18, 1, 10, 1});
    plain->set_time_provider([] { return widgets::TimeValue{21, 41, 7}; });

    stage.step();
    stage.save_window(dir, "widget-clockview");
}

void shot_scrollbar(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.window("Log", Rect{26, 5, 28, 12});

    auto* text = content.make<widgets::StaticText>(
        "A Scrollbar is a control, not a viewport: it reports a position and the "
        "view it belongs to does the scrolling.");
    text->set_bounds(Rect{0, 0, 23, 8});

    // ckvision-doc: scrollbar
    auto* bar = content.make<widgets::Scrollbar>(widgets::Orientation::Vertical);
    bar->set_bounds(Rect{24, 0, 1, 8});
    bar->set_range(/*content_size=*/240, /*viewport_size=*/8);
    bar->set_position(96);
    bar->set_policy(widgets::ScrollbarPolicy::Auto);
    bar->on_position_changed = [](int position) { (void)position; };

    auto* ruler = content.make<widgets::Scrollbar>(widgets::Orientation::Horizontal);
    ruler->set_bounds(Rect{0, 9, 25, 1});
    ruler->set_range(200, 25);
    ruler->set_position(40);
    // ckvision-doc-end: scrollbar

    stage.step();
    stage.save_window(dir, "widget-scrollbar");
}

void shot_scroll_viewport(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.window("Release notes", Rect{20, 5, 40, 12});

    // ckvision-doc: scrollviewport
    auto* viewport = content.make<widgets::ScrollViewport>();
    viewport->set_bounds(Rect{0, 0, 38, 9});

    auto page = std::make_unique<ui::View>();
    page->set_preferred_size(Size{60, 30});  // the world, larger than the hole
    auto* body = page->make<widgets::StaticText>(
        "ScrollViewport clips a content view larger than itself and owns the two "
        "scrollbars that say where in it you are. Give the content a preferred "
        "size: the viewport reads that, not the child's bounds.");
    body->set_bounds(Rect{0, 0, 58, 12});
    viewport->set_content(std::move(page));
    viewport->set_scroll(0, 0);
    viewport->set_scrollbars_always_visible(true);
    // ckvision-doc-end: scrollviewport

    stage.step();
    stage.save_window(dir, "widget-scrollviewport");
}

void shot_splitter(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.window("Compare", Rect{18, 5, 44, 11});

    // ckvision-doc: splitter
    auto left = std::make_unique<widgets::ListView>();
    left->set_items({"alpha", "beta", "gamma", "delta"});
    auto right = std::make_unique<widgets::StaticText>(
        "The Splitter owns both panes and the bar between them. Drag the bar, or "
        "focus it and use the arrow keys.");

    auto* splitter = content.make<widgets::Splitter>(Rect{0, 0, 42, 8}, std::move(left),
                                                     std::move(right),
                                                     widgets::Orientation::Vertical);
    splitter->set_split_position(16);
    // ckvision-doc-end: splitter

    stage.focus(splitter);
    stage.step();
    stage.save_window(dir, "widget-splitter");
}

void shot_tab_control(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Preferences", Rect{18, 5, 44, 11});

    // ckvision-doc: tabcontrol
    auto* tabs = content.make<widgets::TabControl>();
    tabs->set_bounds(Rect{0, 0, 42, 8});

    auto general = std::make_unique<ui::View>();
    general->make<widgets::StaticText>("Settings that apply everywhere.")
        ->set_bounds(Rect{1, 1, 36, 2});
    tabs->add_tab("&General", std::move(general));

    auto editor = std::make_unique<ui::View>();
    editor->make<widgets::StaticText>("Editor-only settings.")->set_bounds(Rect{1, 1, 36, 2});
    tabs->add_tab("&Editor", std::move(editor));

    tabs->add_tab("&Keys", std::make_unique<ui::View>());
    tabs->set_active_index(0);
    // ckvision-doc-end: tabcontrol

    stage.focus(tabs);
    stage.step();
    stage.save_window(dir, "widget-tabcontrol");
}

void shot_breadcrumb_bar(const std::filesystem::path& dir) {
    WidgetStage stage;
    ui::View& content = stage.dialog_window("Browse", Rect{18, 8, 44, 6});

    // ckvision-doc: breadcrumbbar
    auto* trail = content.make<widgets::BreadcrumbBar>();
    trail->set_bounds(Rect{1, 1, 40, 1});
    trail->set_segments({"ckvision", "include", "cvision", "widgets"});
    trail->set_separator(" > ");
    trail->on_activate = [](std::size_t index) { (void)index; /* jump to that level */ };
    // ckvision-doc-end: breadcrumbbar

    stage.focus(trail);
    stage.step();
    stage.save_window(dir, "widget-breadcrumbbar");
}

}  // namespace

void capture_data_shots(const std::filesystem::path& dir) {
    shot_list_view(dir);
    shot_tree_view(dir);
    shot_table(dir);
    shot_property_inspector(dir);
    shot_calendar_view(dir);
    shot_calendar_dropdown(dir);
    shot_clock_view(dir);
    shot_scrollbar(dir);
    shot_scroll_viewport(dir);
    shot_splitter(dir);
    shot_tab_control(dir);
    shot_breadcrumb_bar(dir);
}

}  // namespace ckv::docgen
