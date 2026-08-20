// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/tab_control.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

namespace {
bool mnemonic_matches(const std::optional<std::string>& mnemonic, const std::string& text) {
    if (!mnemonic || mnemonic->empty() || text.empty()) return false;
    const unsigned char a = static_cast<unsigned char>((*mnemonic)[0]);
    const unsigned char b = static_cast<unsigned char>(text[0]);
    return static_cast<char>(std::tolower(a)) == static_cast<char>(std::tolower(b));
}
}  // namespace

TabControl::TabControl() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{20, 6});
}

void TabControl::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.menu.bar.normal");
    if (active_role_ == ui::kInvalidRole) active_role_ = context().roles->find("ckv.menu.bar.active");
    if (page_role_ == ui::kInvalidRole) page_role_ = context().roles->find("ckv.dialog.background");
}

ui::View* TabControl::add_tab(std::string label, std::unique_ptr<ui::View> page) {
    MnemonicText parsed = parse_mnemonic(label);
    ui::View* raw = add_child(std::move(page));
    if (raw == nullptr) return nullptr;
    tabs_.push_back(Tab{parsed.display, raw, parsed.mnemonic});
    if (tabs_.size() == 1) {
        active_index_ = 0;
    } else {
        raw->set_visible(false);
    }
    on_resized();
    invalidate();
    size_hint_changed();
    return raw;
}

ui::View* TabControl::active_page() const noexcept {
    if (tabs_.empty()) return nullptr;
    return tabs_[active_index_].page;
}

void TabControl::set_active_index(std::size_t index) {
    if (index >= tabs_.size() || index == active_index_) return;
    if (ui::View* old = active_page()) old->set_visible(false);
    active_index_ = index;
    if (ui::View* page = active_page()) {
        page->set_visible(true);
        page->set_bounds(Rect{0, 1, bounds().width, std::max(0, bounds().height - 1)});
    }
    invalidate();
}

void TabControl::on_resized() {
    if (ui::View* page = active_page()) page->set_bounds(Rect{0, 1, bounds().width, std::max(0, bounds().height - 1)});
}

int TabControl::tab_start_x(std::size_t index) const {
    int x = 0;
    for (std::size_t i = 0; i < index && i < tabs_.size(); ++i) x += text::text_width(tabs_[i].label) + 3;
    return x;
}

int TabControl::tab_at_x(int local_x) const {
    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        const int x = tab_start_x(i);
        const int w = text::text_width(tabs_[i].label) + 3;
        if (local_x >= x && local_x < x + w) return static_cast<int>(i);
    }
    return -1;
}

void TabControl::activate_delta(int delta) {
    if (tabs_.empty()) return;
    const int count = static_cast<int>(tabs_.size());
    const int next = (static_cast<int>(active_index_) + delta + count) % count;
    set_active_index(static_cast<std::size_t>(next));
}

ui::SizeHint TabControl::horizontal_size_hint() const {
    int width = 0;
    for (const auto& tab : tabs_) width += text::text_width(tab.label) + 3;
    return ui::SizeHint{4, std::max(20, width), ui::kUnboundedExtent};
}

ui::SizeHint TabControl::vertical_size_hint() const { return ui::SizeHint{1, 6, ui::kUnboundedExtent}; }

bool TabControl::on_key(const KeyEvent& event) {
    if (event.action == KeyAction::Release) return false;
    if (event.chord.key == Key::Left) {
        activate_delta(-1);
        return true;
    }
    if (event.chord.key == Key::Right || event.chord.key == Key::Tab) {
        activate_delta(1);
        return true;
    }
    if (event.chord.key == Key::Char && has_modifier(event.chord.modifiers, Modifier::Alt) &&
        !has_modifier(event.chord.modifiers, Modifier::Ctrl) &&
        !has_modifier(event.chord.modifiers, Modifier::Super)) {
        for (std::size_t i = 0; i < tabs_.size(); ++i) {
            if (mnemonic_matches(tabs_[i].mnemonic, event.chord.text)) {
                set_active_index(i);
                return true;
            }
        }
    }
    return false;
}

bool TabControl::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    const int local_y = event.cell.y - abs.y;
    if (local_y != 0) return false;
    const int index = tab_at_x(event.cell.x - abs.x);
    if (index < 0) return false;
    set_active_index(static_cast<std::size_t>(index));
    return true;
}

void TabControl::on_focus(const FocusEvent& event) {
    has_focus_ = event.gained;
    invalidate();
}

void TabControl::draw(scene::Painter& painter) {
    if (bounds().height <= 0 || bounds().width <= 0) return;
    const Style normal = context().theme->resolve(normal_role_);
    const Style active = context().theme->resolve(active_role_);
    const Style page = context().theme->resolve(page_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", normal));
    if (bounds().height > 1) painter.fill(Rect{0, 1, bounds().width, bounds().height - 1}, Cell::from_grapheme(" ", page));

    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        const int x = tab_start_x(i);
        if (x >= bounds().width) break;
        const Style style = i == active_index_ ? active : normal;
        const std::string label = " " + tabs_[i].label + " ";
        painter.draw_text(Point{x, 0}, text::clip_to_width(label, bounds().width - x), style);
    }
    if (has_focus_) {
        Style focus = active;
        focus.attrs |= Attr::Underline;
        painter.draw_text(Point{std::min(bounds().width - 1, tab_start_x(active_index_)), 0}, " ", focus);
    }
}

}  // namespace ckv::widgets
