// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Scrollbar: proportional thumb, arrows, paging, drag (plans/
// widgets.md M6a baseline). Orientation-agnostic math lives in one
// place (thumb_length_cells/thumb_start_cell) so horizontal and
// vertical behave identically apart from which axis they read.
#pragma once

#include <functional>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

enum class Orientation { Horizontal, Vertical };

class Scrollbar : public ui::View {
public:
    Scrollbar(Orientation orientation, const ui::Theme& theme, ui::RoleId track_role, ui::RoleId thumb_role);

    // `content_size` is the total scrollable extent; `viewport_size` is
    // how much of it is visible at once. Both clamped to >= 0 / >= 1
    // respectively; position is re-clamped to the new max_position().
    void set_range(int content_size, int viewport_size);
    int content_size() const noexcept { return content_size_; }
    int viewport_size() const noexcept { return viewport_size_; }

    void set_position(int position);
    int position() const noexcept { return position_; }
    int max_position() const noexcept;

    // Fired whenever position changes for ANY reason (arrows, paging,
    // drag, or a direct set_position call) — one place for a scroll
    // viewport to observe, rather than instrumenting every input path.
    std::function<void(int)> on_position_changed;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;

private:
    int track_length() const noexcept;  // cells available for the thumb, excluding the two arrow cells
    int thumb_length_cells() const noexcept;
    int thumb_start_cell() const noexcept;
    int main_axis_extent(Point local) const noexcept;  // x for Horizontal, y for Vertical

    Orientation orientation_;
    int content_size_ = 0;
    int viewport_size_ = 1;
    int position_ = 0;

    bool dragging_ = false;
    int drag_start_cell_ = 0;
    int drag_start_position_ = 0;

    const ui::Theme& theme_;
    ui::RoleId track_role_;
    ui::RoleId thumb_role_;
};

}  // namespace ckv::widgets
