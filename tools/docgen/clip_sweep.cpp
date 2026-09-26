// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "clip_sweep.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include "cvision/core/clock.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/list_view.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/scrollbar.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/tab_control.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/text_view.hpp"

namespace ckv::docgen::clip_sweep {

namespace {

// How one family is built, how tall it stands, and how far it is swept.
struct Family {
    std::string name;
    // The widest width swept: the widget's natural width for the text, at
    // which all of it shows.
    int max_width;
    // The widget's height at a given width.
    std::function<int(int width)> height;
    // The widget for a given width, unplaced, holding the mixed text.
    std::function<std::unique_ptr<ui::View>(int width)> build;
    // Whether the widget holds the keyboard while it is drawn.
    bool focused = false;
};

std::function<int(int)> rows(int count) {
    return [count](int) { return count; };
}

std::vector<Family> families() {
    std::vector<Family> list;
    list.push_back({"label", 9, rows(1), [](int) { return std::make_unique<widgets::Label>(std::string(kMnemonicText)); }});
    // Prose wraps rather than clips, so the width decides the height: every
    // wrapped line is part of the band.
    list.push_back({"static_text", 9,
                    [](int width) { return widgets::StaticText(std::string(kMixedText)).height_for_width(width); },
                    [](int) { return std::make_unique<widgets::StaticText>(std::string(kMixedText)); }});
    list.push_back({"button", 13, rows(2), [](int) { return std::make_unique<widgets::Button>(std::string(kMnemonicText)); }});
    // Focused, the field keeps its caret at the end of the text and scrolls
    // the start out of sight: its left edge is a clip boundary too.
    list.push_back({"input_line", 10, rows(1),
                    [](int) {
                        auto field = std::make_unique<widgets::InputLine>();
                        field->set_text(std::string(kMixedText));
                        return field;
                    },
                    true});
    list.push_back({"list_view", 10, rows(1), [](int) {
                        auto list_view = std::make_unique<widgets::ListView>();
                        list_view->set_items({std::string(kMixedText)});
                        return list_view;
                    }});
    // The column is as wide as the table. The header is cut by the column's
    // width; the cell also by the scrollbar the data rows keep in the last
    // column.
    list.push_back({"table", 10, rows(2), [](int width) {
                        auto table = std::make_unique<widgets::Table>();
                        table->set_columns({widgets::TableColumn{std::string(kMixedText), width}});
                        table->set_rows({{std::string(kMixedText)}});
                        return table;
                    }});
    list.push_back({"menu", 13, rows(3), [](int) {
                        std::vector<widgets::MenuItem> items;
                        items.push_back(widgets::MenuItem::action(std::string(kMnemonicText), [] {}));
                        return std::make_unique<widgets::DropdownMenu>(std::move(items));
                    }});
    list.push_back({"status_line", 10, rows(1), [](int) {
                        auto line = std::make_unique<widgets::StatusLine>();
                        line->set_items({widgets::StatusLineItem(std::string(kMixedText))});
                        return line;
                    }});
    list.push_back({"text_view", 9, rows(1), [](int) {
                        auto view = std::make_unique<widgets::TextView>();
                        view->set_vertical_scrollbar_policy(widgets::ScrollbarPolicy::Hidden);
                        view->set_horizontal_scrollbar_policy(widgets::ScrollbarPolicy::Hidden);
                        view->set_text(std::string(kMixedText));
                        return view;
                    }});
    list.push_back({"tab_control", 11, rows(1), [](int) {
                        auto tabs = std::make_unique<widgets::TabControl>();
                        tabs->add_tab(std::string(kMnemonicText), std::make_unique<ui::View>());
                        return tabs;
                    }});
    return list;
}

// Draws `family` at `width` alone on a fresh screen and copies its rows into
// `sweep` from row `top`, `columns` cells wide.
void paint_band(const Family& family, int width, int height, int columns, scene::Surface& sweep, int top) {
    const Size screen{std::max(ui::kHardFloorSize.width, columns), std::max(ui::kHardFloorSize.height, height)};
    term::HeadlessTerminal terminal(screen, term::headless_no_graphics_profile());
    ManualClock clock;
    ui::Application app(terminal, clock);
    const ui::StandardRoles roles = ui::intern_standard_roles(app.roles());
    app.theme() = ui::make_classic_theme(app.roles(), roles);
    std::unique_ptr<ui::View> widget = family.build(width);
    // Placed, not stretched: a root child fills the screen unless told not to.
    widget->set_fills_root(false);
    widget->set_bounds(Rect{0, 0, width, height});
    ui::View* const placed = app.root().add_child(std::move(widget));
    app.set_focus(family.focused ? placed : nullptr);
    app.step(0);
    const scene::Surface& frame = app.composed_surface();
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < columns; ++x) sweep.set_cell(Point{x, top + y}, frame.at(Point{x, y}), frame.link_target(Point{x, y}));
}

Sweep render(const Family& family) {
    const int columns = family.max_width + 1;
    std::vector<int> heights;
    int total = 0;
    for (int width = 0; width <= family.max_width; ++width) {
        heights.push_back(std::max(1, family.height(width)));
        total += heights.back();
    }
    Sweep sweep{family.name, scene::Surface(Size{columns, total}), {}};
    int top = 0;
    for (int width = 0; width <= family.max_width; ++width) {
        const int height = heights[static_cast<std::size_t>(width)];
        paint_band(family, width, height, columns, sweep.surface, top);
        sweep.widget_bounds.push_back(Rect{0, top, width, height});
        top += height;
    }
    return sweep;
}

}  // namespace

std::vector<Sweep> render_all() {
    std::vector<Sweep> sweeps;
    for (const Family& family : families()) sweeps.push_back(render(family));
    return sweeps;
}

}  // namespace ckv::docgen::clip_sweep
