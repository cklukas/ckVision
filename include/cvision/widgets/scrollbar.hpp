// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Scrollbar: proportional thumb, arrows, paging, drag (the internal plans
// widgets.md M6a baseline). Orientation-agnostic math lives in one
// place (thumb_length_cells/thumb_start_cell) so horizontal and
// vertical behave identically apart from which axis they read.
#pragma once

#include <functional>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/orientation.hpp"

namespace ckv::widgets {

// When a scrollbar is on screen. This is about whether the content fits, and
// nothing else — not focus, not the pointer being nearby. A bar that comes
// and goes for any other reason moves the content under the reader's eyes
// for a reason they cannot see.
enum class ScrollbarPolicy {
    // Shown exactly while the content is larger than the viewport. The
    // ordinary choice, and the one a reader can reason about: a bar present
    // means there is more, a bar absent means there is not.
    Auto,
    // Always on screen, reserving its column or row whether or not it can
    // scroll. For a surface whose content changes constantly, where a bar
    // appearing and disappearing would reflow everything around it.
    Always,
    // Never on screen. For a view whose scrolling is driven from outside —
    // a containing viewport that owns the visible bars itself.
    Hidden,
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.scrollbar.track"/"ckv.scrollbar.thumb".
class Scrollbar : public ui::View {
public:
    explicit Scrollbar(Orientation orientation);

    void set_role_override(ui::RoleId track_role, ui::RoleId thumb_role) noexcept {
        track_role_ = track_role;
        thumb_role_ = thumb_role;
    }

    // `content_size` is the total scrollable extent; `viewport_size` is
    // how much of it is visible at once. Both clamped to >= 0 / >= 1
    // respectively; position is re-clamped to the new max_position().
    void set_range(int content_size, int viewport_size);
    int content_size() const noexcept { return content_size_; }
    int viewport_size() const noexcept { return viewport_size_; }

    void set_position(int position);
    int position() const noexcept { return position_; }
    int max_position() const noexcept;

    // How many cells the thumb occupies, which is the share of the content
    // currently visible. Exposed because it is the bar's answer to "how much
    // of this am I looking at", and a caller pinning that behaviour should
    // not have to read it off painted cells.
    int thumb_length() const noexcept { return thumb_length_cells(); }

    void set_policy(ScrollbarPolicy policy);
    ScrollbarPolicy policy() const noexcept { return policy_; }
    // Whether this bar would be on screen for the range it currently holds.
    // Asking the bar keeps the rule in one place, so a container working out
    // how much room its content has cannot disagree with what gets drawn.
    bool should_show() const noexcept;

    // Fired whenever position changes for ANY reason (arrows, paging,
    // drag, or a direct set_position call) — one place for a scroll
    // viewport to observe, rather than instrumenting every input path.
    std::function<void(int)> on_position_changed;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    // Where the thumb sits and how long it is, in HALF cells — the block
    // glyphs can fill half a cell, so position and length both carry twice
    // the resolution the cell grid alone would allow.
    struct ThumbSpan {
        int start = 0;
        int length = 0;
    };
    ThumbSpan thumb_span_halves() const noexcept;

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
    ScrollbarPolicy policy_ = ScrollbarPolicy::Always;

    ui::RoleId track_role_ = ui::kInvalidRole;
    ui::RoleId thumb_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
