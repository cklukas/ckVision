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
// Double-click activation uses Application's injected Clock when the list is
// attached under an Application context. D-041 keeps ListView row-materialized
// for 0.1; virtualized and columned list models are post-0.1 scope.
#pragma once

#include <functional>
#include <cstdint>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/scrollbar.hpp"

namespace ckv::widgets {

using ui::SizeHint;

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.list.normal"/"ckv.list.selected"; its embedded
// Scrollbar resolves its own roles the same way, independently.
class ListView : public ui::View {
public:
    explicit ListView(bool multi_select = false);

    void set_role_override(ui::RoleId normal_role, ui::RoleId selected_role) noexcept {
        normal_role_ = normal_role;
        selected_role_ = selected_role;
    }

    void set_items(std::vector<std::string> items);
    const std::vector<std::string>& items() const noexcept { return items_; }

    bool is_selected(std::size_t index) const;
    // Single-select: selecting an item deselects every other one.
    // Multi-select: toggles independently.
    void set_selected(std::size_t index, bool selected);
    std::vector<std::size_t> selected_indices() const;

    int cursor() const noexcept { return cursor_; }  // -1 if the list is empty

    std::function<void(std::size_t)> on_activate;  // Enter, or a second click on the cursor row
    // Fires with the index whose selection just changed (M10/WP-22 —
    // mirrors TreeView::on_selection_changed passing its own node).
    std::function<void(std::size_t)> on_selection_changed;

    void on_resized() override;
    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    static constexpr std::int64_t kDoubleClickIntervalNanos = 500'000'000;

    void move_cursor(int new_cursor, bool extend_or_replace_selection);
    void ensure_cursor_visible();
    void select_only(std::size_t index);
    void toggle_selected(std::size_t index);

    std::vector<std::string> items_;
    std::vector<bool> selected_;
    int cursor_ = -1;
    bool multi_select_;
    int last_click_index_ = -1;
    std::int64_t last_click_nanos_ = -1;

    Scrollbar* scrollbar_ = nullptr;

    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
