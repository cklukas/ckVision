// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/label.hpp"

#include <cctype>

#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

namespace {

bool ascii_ci_equal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

bool is_descendant_of(const ui::View& view, const ui::View& ancestor) noexcept {
    for (const ui::View* p = &view; p != nullptr; p = p->parent())
        if (p == &ancestor) return true;
    return false;
}

bool is_label_mnemonic_request(const KeyEvent& event) noexcept {
    return event.action == KeyAction::Press && event.chord.key == Key::Char &&
           has_modifier(event.chord.modifiers, Modifier::Alt) &&
           !has_modifier(event.chord.modifiers, Modifier::Ctrl) &&
           !has_modifier(event.chord.modifiers, Modifier::Super) &&
           !event.chord.text.empty();
}

bool activate_label_mnemonic_recursive(ui::View& view, ui::View& scope, const std::string& query,
                                       ui::Application& app) {
    if (!view.visible() || !view.enabled()) return false;
    if (auto* label = dynamic_cast<Label*>(&view); label != nullptr &&
        !label->mnemonic().empty() && ascii_ci_equal(label->mnemonic(), query)) {
        ui::View* const buddy = label->buddy();
        if (buddy != nullptr && is_descendant_of(*buddy, scope) && buddy->focusable()) {
            app.set_focus(buddy);
            return true;
        }
    }
    for (const auto& child : view.children())
        if (activate_label_mnemonic_recursive(*child, scope, query, app)) return true;
    return false;
}

}  // namespace

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

void Label::set_buddy(View* buddy) noexcept {
    buddy_ = buddy;
    buddy_liveness_ = buddy != nullptr ? buddy->lifetime_token() : std::weak_ptr<void>{};
}

View* Label::buddy() const noexcept {
    if (buddy_ == nullptr) return nullptr;
    if (buddy_liveness_.expired()) return nullptr;
    return buddy_;
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

bool activate_label_mnemonic(View& scope, const KeyEvent& event, ui::Application& app) {
    if (!is_label_mnemonic_request(event)) return false;
    return activate_label_mnemonic_recursive(scope, scope, event.chord.text, app);
}

}  // namespace ckv::widgets
