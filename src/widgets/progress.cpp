// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/progress.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <charconv>

#include "cvision/core/text.hpp"
#include "cvision/widgets/progress_paint_internal.hpp"

namespace ckv::widgets {

Progress::Progress() { set_preferred_size(Size{20, 1}); }

void Progress::on_attached() {
    attached_ = true;
    if (track_role_ == ui::kInvalidRole) track_role_ = context().roles->find("ckv.progress.track");
    if (fill_role_ == ui::kInvalidRole) fill_role_ = context().roles->find("ckv.progress.fill");
    label_role_ = context().roles->find("ckv.progress.label");
    disabled_role_ = context().roles->find("ckv.progress.disabled");
    if (on_animation_changed) on_animation_changed();
}

void Progress::on_detaching() { attached_ = false; if (on_animation_changed) on_animation_changed(); }
void Progress::on_effective_visibility_changed() { if (on_animation_changed) on_animation_changed(); }
void Progress::set_motion(ProgressMotion motion) {
    motion_ = motion; invalidate(); if (on_animation_changed) on_animation_changed();
}
bool Progress::needs_animation() const noexcept {
    return attached_ && visible_in_tree() && indeterminate_ && motion_ == ProgressMotion::Animated;
}

void Progress::set_presentation(ProgressPresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    invalidate();
}

void Progress::set_activity_style(ProgressActivityStyle style) {
    if (activity_style_ == style) return;
    activity_style_ = style;
    invalidate();
}

void Progress::set_glyphs(ProgressGlyphs glyphs) {
    if (glyphs_ == glyphs) return;
    glyphs_ = glyphs;
    invalidate();
}

void Progress::set_show_percentage(bool show) {
    if (show_percentage_ == show) return;
    show_percentage_ = show;
    size_hint_changed();
    invalidate();
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
    if (on_animation_changed) on_animation_changed();
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
    const int suffix = show_percentage_ ? 5 : 0;
    return ui::SizeHint{show_percentage_ ? 6 : 4, std::max(20 + suffix, text::text_width(label_) + 4 + suffix), ui::kUnboundedExtent};
}

ui::SizeHint Progress::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }

void Progress::draw(scene::Painter& painter) {
    const int width = bounds().width;
    if (width <= 0 || bounds().height <= 0) return;
    auto clipped = painter.clipped(Rect{0, 0, width, 1});
    const Style inert = context().theme->resolve(disabled_role_);
    const auto shown = [&](ui::RoleId role) {
        Style style = context().theme->resolve(role);
        if (!enabled_in_tree()) { style.fg = inert.fg; style.attrs |= inert.attrs; }
        return style;
    };
    const Style track = shown(track_role_);
    const Style fill = shown(fill_role_);
    const bool percentage = show_percentage_ && !indeterminate_ && width >= 6;
    const int meter_width = width - (percentage ? 5 : 0);
    const auto phase = motion_ == ProgressMotion::Static ? 0 : pulse_;
    const auto activity = motion_ == ProgressMotion::Static ? ProgressActivityStyle::Bounce : activity_style_;
    const auto [unit, lit_begin, lit_end] = detail::progress_span(meter_width, fraction_, indeterminate_, phase, presentation_, activity);
    const auto lit = [&](int x) {
        if (!indeterminate_ && presentation_ == ProgressPresentation::Smooth && glyphs_ == ProgressGlyphs::Unicode)
            return x * 8LL < static_cast<std::int64_t>(std::floor(fraction_ * meter_width * 8.0));
        return x / unit >= lit_begin && x / unit < lit_end;
    };
    detail::paint_progress(clipped, Rect{0, 0, meter_width, 1}, fraction_, indeterminate_,
                           motion_ == ProgressMotion::Static ? 0 : pulse_, presentation_,
                           motion_ == ProgressMotion::Static ? ProgressActivityStyle::Bounce : activity_style_, glyphs_, track,
                           !enabled_in_tree() && presentation_ != ProgressPresentation::Solid ? Style{fill.fg, inert.fg, fill.attrs} : fill);
    const std::string_view label = text::clip_to_width_view(label_, meter_width);
    int x = std::max(0, (meter_width - text::text_width(label)) / 2);
    for (std::size_t byte = 0; byte < label.size();) {
        const std::size_t end = text::grapheme_end(label, byte);
        const std::string_view cluster = label.substr(byte, end - byte);
        clipped.draw_text(Point{x, 0}, cluster, lit(x) ? fill : track);
        x += text::grapheme_width(cluster);
        byte = end;
    }
    if (percentage) {
        const Style style = shown(label_role_);
        clipped.fill(Rect{meter_width, 0, 5, 1}, Cell::from_grapheme(" ", style));
        std::array<char, 4> text{};
        const int value = static_cast<int>(std::llround(fraction_ * 100));
        const auto result = std::to_chars(text.data(), text.data() + 3, value);
        *result.ptr = '%';
        const int length = static_cast<int>(result.ptr - text.data()) + 1;
        clipped.draw_text(Point{width - length, 0}, std::string_view{text.data(), static_cast<std::size_t>(length)}, style);
    }
}

}  // namespace ckv::widgets
