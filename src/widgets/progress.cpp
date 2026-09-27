// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/progress.hpp"

#include <algorithm>
#include <cmath>

#include "cvision/core/text.hpp"

namespace ckv::widgets {

Progress::Progress() { set_preferred_size(Size{20, 1}); }

void Progress::on_attached() {
    if (track_role_ == ui::kInvalidRole) track_role_ = context().roles->find("ckv.list.normal");
    if (fill_role_ == ui::kInvalidRole) fill_role_ = context().roles->find("ckv.menu.bar.active");
}

void Progress::set_fraction(double fraction) {
    if (!std::isfinite(fraction)) fraction = 0.0;
    fraction = std::clamp(fraction, 0.0, 1.0);
    if (fraction_ == fraction) return;
    fraction_ = fraction;
    invalidate();
}

void Progress::set_indeterminate(bool indeterminate) {
    if (indeterminate_ == indeterminate) return;
    indeterminate_ = indeterminate;
    invalidate();
}

void Progress::set_pulse(int offset) {
    if (pulse_ == offset) return;
    pulse_ = offset;
    invalidate();
}

void Progress::set_label(std::string label) {
    if (label_ == label) return;
    label_ = std::move(label);
    invalidate();
    size_hint_changed();
}

ui::SizeHint Progress::horizontal_size_hint() const {
    return ui::SizeHint{4, std::max(20, text::text_width(label_) + 4), ui::kUnboundedExtent};
}

ui::SizeHint Progress::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }

void Progress::draw(scene::Painter& painter) {
    const int w = bounds().width;
    if (w <= 0 || bounds().height <= 0) return;
    const Style track = context().theme->resolve(track_role_);
    const Style fill = context().theme->resolve(fill_role_);
    // The lit span: the filled share, or the indeterminate pulse.
    int lit_begin = 0;
    int lit_end = 0;
    if (indeterminate_) {
        const int block_width = std::max(1, w / 4);
        const int span = std::max(1, w + block_width);
        lit_begin = ((pulse_ % span) + span) % span - block_width;
        lit_end = lit_begin + block_width;
    } else {
        lit_end = static_cast<int>(std::llround(fraction_ * static_cast<double>(w)));
    }
    lit_begin = std::clamp(lit_begin, 0, w);
    lit_end = std::clamp(lit_end, lit_begin, w);
    painter.fill(Rect{0, 0, w, 1}, Cell::from_grapheme(" ", track));
    if (lit_end > lit_begin) painter.fill(Rect{lit_begin, 0, lit_end - lit_begin, 1}, Cell::from_grapheme(" ", fill));

    // The label lies over the bar, each cluster on whichever surface is under
    // it. Drawn in the track's style throughout, it erased the fill beneath
    // it, and a bar two-thirds done read as barely begun.
    if (!label_.empty()) {
        const std::string shown = text::clip_to_width(label_, w);
        int x = std::max(0, (w - text::text_width(shown)) / 2);
        for (std::string_view grapheme : text::split_graphemes(shown)) {
            painter.draw_text(Point{x, 0}, grapheme, x >= lit_begin && x < lit_end ? fill : track);
            x += text::grapheme_width(grapheme);
        }
    }
}

}  // namespace ckv::widgets
