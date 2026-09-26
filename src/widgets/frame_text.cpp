// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/frame_text.hpp"

#include <algorithm>
#include <utility>

#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/widgets/window.hpp"

namespace ckv::widgets {

namespace {

// The cells the text takes on the border: itself and a space either side.
int text_run(const std::string& text) {
    const int width = text::text_width(text);
    return width > 0 ? width + 2 : 0;
}

}  // namespace

FrameText::FrameText(std::string text) : text_(std::move(text)) {}

void FrameText::set_text(std::string text) {
    if (text == text_) return;
    const bool resized = reserved_width_ == 0 && text_run(text) != text_run(text_);
    text_ = std::move(text);
    if (resized) size_hint_changed();
    invalidate();
}

void FrameText::set_reserved_width(int cells) {
    const int width = std::max(0, cells);
    if (width == reserved_width_) return;
    reserved_width_ = width;
    size_hint_changed();
    invalidate();
}

void FrameText::set_alignment(ui::Alignment alignment) {
    if (alignment == alignment_) return;
    alignment_ = alignment;
    invalidate();
}

ui::SizeHint FrameText::horizontal_size_hint() const {
    const int width = reserved_width_ > 0 ? reserved_width_ : text_run(text_);
    return ui::SizeHint{width, width, width};
}

ui::SizeHint FrameText::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }

void FrameText::draw(scene::Painter& painter) {
    const auto* const window = dynamic_cast<const Window*>(parent());
    const int run = std::min(text_run(text_), bounds().width);
    if (window == nullptr || context().theme == nullptr || run <= 0) return;
    const Style style = window->frame_style();
    const int free = bounds().width - run;
    const int x = alignment_ == ui::Alignment::End ? free : alignment_ == ui::Alignment::Center ? free / 2 : 0;
    painter.fill(Rect{x, 0, run, 1}, Cell::from_grapheme(" ", style));
    // Given less room than it asked for, the text is elided inside its
    // padding, as a window title is, rather than cut off at the edge.
    painter.draw_text(Point{x + 1, 0}, text::elide_to_width(text_, std::max(0, run - 2)), style);
}

}  // namespace ckv::widgets
