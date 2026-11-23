// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/group_box.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/ui/layout.hpp"

namespace ckv::widgets {

GroupBox::GroupBox(std::string title, scene::LineStyle line_style)
    : title_(std::move(title)), line_style_(line_style) {}

void GroupBox::set_title(std::string title) {
    if (title_ == title) return;
    title_ = std::move(title);
    size_hint_changed();
    invalidate();
}

ui::View* GroupBox::set_content(std::unique_ptr<ui::View> content) {
    if (content_ != nullptr) {
        remove_child(content_);
        content_ = nullptr;
    }
    if (content == nullptr) {
        size_hint_changed();
        invalidate();
        return nullptr;
    }
    content_ = add_child(std::move(content));
    relayout();
    size_hint_changed();
    invalidate();
    return content_;
}

void GroupBox::on_attached() {
    frame_role_ = context().roles->find("ckv.window.frame.active");
    title_role_ = context().roles->find("ckv.label.text");
}

void GroupBox::on_resized() { relayout(); }

void GroupBox::on_child_size_hint_changed(ui::View&) {
    relayout();
    size_hint_changed();
}

void GroupBox::relayout() {
    if (content_ == nullptr) return;
    const int inner_w = std::max(0, bounds().width - 2);
    const int inner_h = std::max(0, bounds().height - 2);
    content_->set_bounds(Rect{1, 1, inner_w, inner_h});
}

void GroupBox::draw(scene::Painter& painter) {
    const int w = bounds().width;
    const int h = bounds().height;
    if (w < 2 || h < 2) return;
    const Style frame_style = context().theme->resolve(frame_role_);
    painter.draw_box(Rect{0, 0, w, h}, line_style_, frame_style);
    if (!title_.empty() && w > 4) {
        const Style title_style = context().theme->resolve(title_role_);
        const std::string caption = " " + title_ + " ";
        auto clipped = painter.clipped(Rect{2, 0, w - 4, 1});
        clipped.draw_text(Point{2, 0}, caption, title_style);
    }
}

ui::SizeHint GroupBox::horizontal_size_hint() const {
    const int title_w = title_.empty() ? 2 : text::text_width(title_) + 6;
    if (content_ == nullptr) return ui::SizeHint{title_w, title_w, ui::kUnboundedExtent};
    const ui::SizeHint ch = content_->horizontal_size_hint();
    return ui::SizeHint{std::max(title_w, ch.min + 2), std::max(title_w, ch.preferred + 2),
                        ui::kUnboundedExtent};
}

ui::SizeHint GroupBox::vertical_size_hint() const {
    if (content_ == nullptr) return ui::SizeHint{2, 2, ui::kUnboundedExtent};
    const ui::SizeHint ch = content_->vertical_size_hint();
    return ui::SizeHint{ch.min + 2, ch.preferred + 2, ui::kUnboundedExtent};
}

int GroupBox::height_for_width(int width) const {
    if (content_ == nullptr) return 2;
    return content_->height_for_width(std::max(0, width - 2)) + 2;
}

}  // namespace ckv::widgets
