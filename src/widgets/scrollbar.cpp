// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/scrollbar.hpp"

#include <algorithm>

namespace ckv::widgets {

Scrollbar::Scrollbar(Orientation orientation) : orientation_(orientation) {}

void Scrollbar::on_attached() {
    if (track_role_ == ui::kInvalidRole) track_role_ = context().roles->find("ckv.scrollbar.track");
    if (thumb_role_ == ui::kInvalidRole) thumb_role_ = context().roles->find("ckv.scrollbar.thumb");
}

int Scrollbar::main_axis_extent(Point local) const noexcept {
    return orientation_ == Orientation::Horizontal ? local.x : local.y;
}

int Scrollbar::track_length() const noexcept {
    const int total = orientation_ == Orientation::Horizontal ? bounds().width : bounds().height;
    return std::max(0, total - 2);  // minus the two arrow cells
}

Scrollbar::ThumbSpan Scrollbar::thumb_span_halves() const noexcept {
    const int track = track_length();
    if (track <= 0) return ThumbSpan{0, 0};
    const int track_halves = track * 2;
    if (content_size_ <= viewport_size_) return ThumbSpan{0, track_halves};  // all of it is visible

    // Measured in HALF cells, because the block-drawing glyphs can fill half
    // a cell: that doubles the resolution of both the thumb's length and its
    // position, so a long document's bar moves smoothly instead of jumping a
    // whole cell at a time.
    //
    // The length is the share of the content currently on screen, so the
    // thumb's size answers "how much of this am I looking at" — the question
    // a fixed-size marker cannot answer at all. Rounded to nearest, so a
    // half-full view does not read as slightly less than half.
    const int proportional =
        (track_halves * viewport_size_ + content_size_ / 2) / std::max(1, content_size_);
    // At least one half cell, or there would be nothing to see or to drag...
    int length = std::max(1, proportional);
    // ...and at least one half cell of track left over, because a thumb
    // filling its track says "everything is visible", which is exactly what
    // is not true here. The gap is also what keeps every scroll position
    // reachable by dragging.
    length = std::min(length, track_halves - 1);

    const int max_pos = max_position();
    const int travel = track_halves - length;
    const int start = max_pos <= 0 ? 0 : travel * position_ / max_pos;
    return ThumbSpan{start, length};
}

int Scrollbar::thumb_length_cells() const noexcept {
    const ThumbSpan span = thumb_span_halves();
    if (span.length <= 0) return 0;
    // How many cells the thumb touches, which is what a caller reasoning
    // about layout wants; the halves are a rendering detail.
    const int first = span.start / 2;
    const int last = (span.start + span.length - 1) / 2;
    return last - first + 1;
}

int Scrollbar::thumb_start_cell() const noexcept { return thumb_span_halves().start / 2; }

int Scrollbar::max_position() const noexcept { return std::max(0, content_size_ - viewport_size_); }

void Scrollbar::set_range(int content_size, int viewport_size) {
    content_size_ = std::max(0, content_size);
    viewport_size_ = std::max(1, viewport_size);
    set_position(position_);  // re-clamp to the new max_position(); fires on_position_changed if it moved
    set_visible(should_show());
    invalidate();
}

void Scrollbar::set_position(int position) {
    const int clamped = std::clamp(position, 0, max_position());
    if (clamped == position_) return;
    position_ = clamped;
    invalidate();
    if (on_position_changed) on_position_changed(position_);
}

bool Scrollbar::should_show() const noexcept {
    switch (policy_) {
        case ScrollbarPolicy::Always:
            return true;
        case ScrollbarPolicy::Hidden:
            return false;
        case ScrollbarPolicy::Auto:
            break;
    }
    // There is something to scroll to, and therefore something a bar can say.
    return max_position() > 0;
}

void Scrollbar::set_policy(ScrollbarPolicy policy) {
    if (policy_ == policy) return;
    policy_ = policy;
    set_visible(should_show());
}

bool Scrollbar::on_key(const KeyEvent& event) {
    switch (event.chord.key) {
        case Key::Up:
            if (orientation_ != Orientation::Vertical) return false;
            set_position(position_ - 1);
            return true;
        case Key::Down:
            if (orientation_ != Orientation::Vertical) return false;
            set_position(position_ + 1);
            return true;
        case Key::Left:
            if (orientation_ != Orientation::Horizontal) return false;
            set_position(position_ - 1);
            return true;
        case Key::Right:
            if (orientation_ != Orientation::Horizontal) return false;
            set_position(position_ + 1);
            return true;
        case Key::PageUp:
            set_position(position_ - viewport_size_);
            return true;
        case Key::PageDown:
            set_position(position_ + viewport_size_);
            return true;
        case Key::Home:
            set_position(0);
            return true;
        case Key::End:
            set_position(max_position());
            return true;
        default:
            return false;
    }
}

bool Scrollbar::on_mouse(const MouseEvent& event) {
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    const int total = orientation_ == Orientation::Horizontal ? bounds().width : bounds().height;

    if (event.action == MouseAction::Down) {
        const int extent = main_axis_extent(local);
        if (extent < 0 || extent >= total) return false;
        if (extent == 0) {
            set_position(position_ - 1);
            return true;
        }
        if (extent == total - 1) {
            set_position(position_ + 1);
            return true;
        }
        const int track_pos = extent - 1;
        const int thumb_start = thumb_start_cell();
        const int thumb_end = thumb_start + thumb_length_cells();
        if (track_pos >= thumb_start && track_pos < thumb_end) {
            dragging_ = true;
            drag_start_cell_ = extent;
            drag_start_position_ = position_;
        } else if (track_pos < thumb_start) {
            set_position(position_ - viewport_size_);
        } else {
            set_position(position_ + viewport_size_);
        }
        return true;
    }

    if (!dragging_) return false;

    if (event.action == MouseAction::Move) {
        const int extent = main_axis_extent(local);
        const int available = track_length() - thumb_length_cells();
        int delta_position = 0;
        if (available > 0) delta_position = (extent - drag_start_cell_) * max_position() / available;
        set_position(drag_start_position_ + delta_position);
        return true;
    }
    if (event.action == MouseAction::Up) {
        dragging_ = false;
        return true;
    }
    return false;
}

void Scrollbar::draw(scene::Painter& painter) {
    const Style track_style = context().theme->resolve(track_role_);
    const Style thumb_style = context().theme->resolve(thumb_role_);
    const int total = orientation_ == Orientation::Horizontal ? bounds().width : bounds().height;
    if (total <= 0) return;

    const auto put = [&](int extent, std::string_view glyph, Style style) {
        const Point p = orientation_ == Orientation::Horizontal ? Point{extent, 0} : Point{0, extent};
        painter.draw_text(p, glyph, style);
    };

    // Triangle arrows at the ends and a trough marked by its own background
    // colour, with the thumb drawn in block glyphs at half-cell resolution.
    // A bar with nothing to scroll draws a thumb the full length of its
    // track, which is the honest statement that the whole content is on
    // screen — the same thing thumb_length() reports. A bar with nothing to scroll drops both the
    // page area and the indicator for one dark-shade run — the bar is
    // still drawn, but it states plainly that there is nowhere to go.
    put(0, orientation_ == Orientation::Horizontal ? "◄" : "▲", track_style);
    if (total > 1) put(total - 1, orientation_ == Orientation::Horizontal ? "►" : "▼", track_style);

    const int track = track_length();
    // The thumb is drawn from block glyphs so it reads as one continuous
    // handle rather than a column of beads, and so a half-covered cell can
    // show it: a full block where the whole cell is thumb, and the matching
    // half block where only one half is. A half-block cell keeps the track's
    // own background, so the uncovered half still looks like track.
    const ThumbSpan span = thumb_span_halves();
    const Style half_style{thumb_style.fg, track_style.bg, thumb_style.attrs};
    const bool horizontal = orientation_ == Orientation::Horizontal;
    for (int i = 0; i < track; ++i) {
        const auto covered = [&span](int half) {
            return half >= span.start && half < span.start + span.length;
        };
        const bool first = covered(2 * i);      // top, or left
        const bool second = covered(2 * i + 1); // bottom, or right
        if (first && second) {
            put(1 + i, "█", thumb_style);
        } else if (first) {
            put(1 + i, horizontal ? "▌" : "▀", half_style);
        } else if (second) {
            put(1 + i, horizontal ? "▐" : "▄", half_style);
        } else {
            // A blank cell in the trough's own colour, not a shaded glyph.
            // The empty half of a half-block cell shows plain background, so
            // a textured track would meet it at a visible seam: the reader
            // sees the pattern stop mid-cell for no reason they can name.
            // Colour alone marks the trough, and the two meet flush.
            put(1 + i, " ", track_style);
        }
    }
}

}  // namespace ckv::widgets
