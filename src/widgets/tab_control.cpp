// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/tab_control.hpp"

#include <algorithm>
#include <string_view>

#include "cvision/core/ascii.hpp"
#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic.hpp"
#include "cvision/widgets/mnemonic_internal.hpp"

namespace ckv::widgets {

namespace {
bool mnemonic_matches(const std::optional<std::string>& mnemonic, const std::string& text) {
    if (!mnemonic || mnemonic->empty() || text.empty()) return false;
    return ascii_lower((*mnemonic)[0]) == ascii_lower(text[0]);
}

// The marks that say captions are hidden off either end of the strip.
constexpr std::string_view kHiddenLeftMark = "◂";   // U+25C2
constexpr std::string_view kHiddenRightMark = "▸";  // U+25B8
}  // namespace

TabControl::TabControl() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{20, 6});
}

void TabControl::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.menu.bar.normal");
    if (active_role_ == ui::kInvalidRole) active_role_ = context().roles->find("ckv.menu.bar.active");
    if (page_role_ == ui::kInvalidRole) page_role_ = context().roles->find("ckv.dialog.background");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.menu.dropdown.disabled");
    if (hotkey_role_ == ui::kInvalidRole) hotkey_role_ = context().roles->find("ckv.hotkey");
}

ui::View* TabControl::add_tab(std::string label, std::unique_ptr<ui::View> page) {
    MnemonicText parsed = parse_mnemonic(label);
    ui::View* raw = add_child(std::move(page));
    if (raw == nullptr) return nullptr;
    tabs_.push_back(Tab{parsed.display, raw, parsed.mnemonic});
    captions_.push_back(std::move(parsed));
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
    keep_active_visible();
    invalidate();
}

void TabControl::on_resized() {
    if (ui::View* page = active_page()) page->set_bounds(Rect{0, 1, bounds().width, std::max(0, bounds().height - 1)});
    settle_strip();
}

int TabControl::caption_width(std::size_t index) const { return text::text_width(tabs_[index].label) + 2; }

TabControl::StripLayout TabControl::layout_from(std::size_t first) const {
    StripLayout strip;
    strip.first = first;
    strip.left_mark = first > 0;
    const int width = bounds().width;
    const int start = strip.left_mark ? 1 : 0;
    // Places captions from `first` while they end by `limit`. The first one
    // is placed whatever its width: a strip always shows a caption, and when
    // that caption alone is wider than the strip it is shown clipped.
    const auto place = [&](int limit) {
        strip.x.clear();
        strip.limit = limit;
        int x = start;
        std::size_t index = first;
        for (; index < tabs_.size(); ++index) {
            const int w = caption_width(index);
            if (index > first && x + w > limit) break;
            strip.x.push_back(x);
            x += w + 1;
        }
        return index;
    };
    strip.end = place(width);
    // Something is left over on the right: its mark takes the last column,
    // and the captions are placed again in what remains.
    if (strip.end < tabs_.size()) {
        strip.right_mark = true;
        strip.end = place(width - 1);
    }
    return strip;
}

int TabControl::tab_at_x(const StripLayout& strip, int local_x) const {
    for (std::size_t i = strip.first; i < strip.end; ++i) {
        const int x = strip.x[i - strip.first];
        // A caption answers over its own cells and the gap after it, never
        // over the scroll mark it may run up against.
        const int right = std::min(x + caption_width(i) + 1, strip.limit);
        if (local_x >= x && local_x < right) return static_cast<int>(i);
    }
    return -1;
}

void TabControl::keep_active_visible() {
    if (tabs_.empty()) {
        first_visible_ = 0;
        return;
    }
    first_visible_ = std::min(first_visible_, active_index_);
    while (first_visible_ < active_index_ && layout_from(first_visible_).end <= active_index_) ++first_visible_;
}

void TabControl::settle_strip() {
    keep_active_visible();
    // Room gained on the right is given back to the captions hidden on the
    // left, one at a time, for as long as that hides none on the right.
    while (first_visible_ > 0 && layout_from(first_visible_ - 1).end >= layout_from(first_visible_).end)
        --first_visible_;
}

void TabControl::scroll_strip(int delta) {
    const StripLayout before = layout_from(first_visible_);
    if (delta < 0 && before.left_mark) {
        --first_visible_;
    } else if (delta > 0 && before.right_mark) {
        ++first_visible_;
    } else {
        return;
    }
    // The active caption never leaves the strip: scrolled past, the
    // selection moves to the nearest caption still shown.
    const StripLayout after = layout_from(first_visible_);
    if (active_index_ < after.first) set_active_index(after.first);
    else if (active_index_ >= after.end) set_active_index(after.end - 1);
    invalidate();
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
    // Tab and Shift+Tab are not the strip's: they walk the focus through the
    // form, and a strip that took them could never be left from the keyboard.
    if (event.chord.key == Key::Right) {
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
    const int local_x = event.cell.x - abs.x;
    const StripLayout strip = layout_from(first_visible_);
    if (strip.left_mark && local_x == 0) {
        scroll_strip(-1);
        return true;
    }
    if (strip.right_mark && local_x == bounds().width - 1) {
        scroll_strip(1);
        return true;
    }
    const int index = tab_at_x(strip, local_x);
    if (index < 0) return false;
    set_active_index(static_cast<std::size_t>(index));
    return true;
}

void TabControl::on_focus(const FocusEvent&) { invalidate(); }

void TabControl::draw(scene::Painter& painter) {
    if (bounds().height <= 0 || bounds().width <= 0) return;
    // Disabled (D-076), the strip wears its menu family's disabled
    // foreground on its own surfaces, the active tab still marked by its
    // background.
    const bool enabled = enabled_in_tree();
    const Style inert = context().theme->resolve(disabled_role_);
    const auto shown = [&](Style style) { return enabled ? style : accent_style(style, inert); };
    const Style normal = shown(context().theme->resolve(normal_role_));
    const Style active = shown(context().theme->resolve(active_role_));
    const Style page = context().theme->resolve(page_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", normal));
    if (bounds().height > 1) painter.fill(Rect{0, 1, bounds().width, bounds().height - 1}, Cell::from_grapheme(" ", page));

    const StripLayout strip = layout_from(first_visible_);
    for (std::size_t i = strip.first; i < strip.end; ++i) {
        const int x = strip.x[i - strip.first];
        const int available = strip.limit - x;
        if (available <= 0) break;
        const Style style = i == active_index_ ? active : normal;
        // The strip is the menu family, so its mnemonics take the menu's
        // hotkey accent — and none while it cannot be used.
        const Style accent = enabled ? accent_style(style, context().theme->resolve(hotkey_role_)) : style;
        painter.draw_text(Point{x, 0}, text::clip_to_width(" ", available), style);
        draw_mnemonic(painter, Point{x + 1, 0}, captions_[i], available - 1, style, accent);
        const int end = x + 1 + text::text_width(captions_[i].display);
        if (end < strip.limit) painter.draw_text(Point{end, 0}, " ", style);
    }
    if (strip.left_mark) painter.draw_text(Point{0, 0}, kHiddenLeftMark, normal);
    if (strip.right_mark) painter.draw_text(Point{bounds().width - 1, 0}, kHiddenRightMark, normal);
    // A disabled tab strip (D-076) draws its unfocused state. The active
    // caption is always on the strip, so its leading cell is always there to
    // carry the focus mark.
    if (has_focus() && enabled_in_tree() && active_index_ >= strip.first && active_index_ < strip.end) {
        Style focus = active;
        focus.attrs |= Attr::Underline;
        painter.draw_text(Point{std::min(strip.limit - 1, strip.x[active_index_ - strip.first]), 0}, " ", focus);
    }
}

}  // namespace ckv::widgets
