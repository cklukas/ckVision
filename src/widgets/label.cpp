// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/label.hpp"

#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

Label::Label(std::string text) { set_text(std::move(text)); }

void Label::on_attached() {
    if (text_role_ == ui::kInvalidRole) text_role_ = context().roles->find("ckv.label.text");
    if (mnemonic_role_ == ui::kInvalidRole) mnemonic_role_ = context().roles->find("ckv.label.mnemonic");
}

void Label::set_text(std::string text) {
    raw_text_ = std::move(text);
    const MnemonicText parsed = parse_mnemonic(raw_text_);
    display_text_ = parsed.display;
    mnemonic_ = parsed.mnemonic;
    mnemonic_byte_offset_ = parsed.mnemonic_byte_offset;
    set_preferred_size(Size{text::text_width(display_text_), 1});
    invalidate();
    size_hint_changed();
}

void Label::draw(scene::Painter& painter) {
    const ui::Theme& theme = *context().theme;
    if (mnemonic_byte_offset_ == std::string::npos) {
        painter.draw_text(Point{0, 0}, display_text_, theme.resolve(text_role_));
        return;
    }
    const std::string_view before = std::string_view(display_text_).substr(0, mnemonic_byte_offset_);
    const std::string_view after =
        std::string_view(display_text_).substr(mnemonic_byte_offset_ + mnemonic_.size());
    int x = 0;
    painter.draw_text(Point{x, 0}, before, theme.resolve(text_role_));
    x += text::text_width(before);
    painter.draw_text(Point{x, 0}, mnemonic_, theme.resolve(mnemonic_role_));
    x += text::text_width(mnemonic_);
    painter.draw_text(Point{x, 0}, after, theme.resolve(text_role_));
}

SizeHint Label::horizontal_size_hint() const {
    const int width = text::text_width(display_text_);
    return SizeHint{width, width, width};  // a label never stretches or shrinks its text
}

}  // namespace ckv::widgets
