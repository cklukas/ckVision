// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// ListView: scrolling, single/multi select, keyboard search, double-
// click activate (plans/widgets.md M6a baseline). Owns its own
// vertical Scrollbar directly rather than wrapping ScrollViewport —
// its content is a flat, row-indexed string list, not an arbitrary
// View, so ScrollViewport's generic content-positioning model doesn't
// fit; scrolling here is "which row is first visible," not "content
// view offset."
//
// Scope note: "double-click" is approximated without real timing —
// clicking on the row that is ALREADY the cursor (i.e. a second click
// on the same row, whenever it happens) activates it. Genuine time-
// windowed double-click detection needs the injected Clock threaded
// into widget event handlers, which no widget currently receives;
// documented gap, not an oversight. Virtualized-model support for
// millions of rows is explicitly beyond-baseline.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

using ui::SizeHint;

class ListView : public ui::View {
public:
    ListView(const ui::Theme& theme, ui::RoleId normal_role, ui::RoleId selected_role,
             ui::RoleId track_role, ui::RoleId thumb_role, bool multi_select = false);

    void set_items(std::vector<std::string> items);
    const std::vector<std::string>& items() const noexcept { return items_; }

    bool is_selected(std::size_t index) const;
    // Single-select: selecting an item deselects every other one.
    // Multi-select: toggles independently.
    void set_selected(std::size_t index, bool selected);
    std::vector<std::size_t> selected_indices() const;

    int cursor() const noexcept { return cursor_; }  // -1 if the list is empty

    std::function<void(std::size_t)> on_activate;  // Enter, or a second click on the cursor row
    std::function<void()> on_selection_changed;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;

private:
    void move_cursor(int new_cursor, bool extend_or_replace_selection);
    void ensure_cursor_visible();
    void select_only(std::size_t index);
    void toggle_selected(std::size_t index);

    std::vector<std::string> items_;
    std::vector<bool> selected_;
    int cursor_ = -1;
    bool multi_select_;

    Scrollbar* scrollbar_ = nullptr;

    const ui::Theme& theme_;
    ui::RoleId normal_role_;
    ui::RoleId selected_role_;
};

}  // namespace ckv::widgets
