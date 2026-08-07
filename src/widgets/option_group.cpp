// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/option_group.hpp"

#include <algorithm>
#include <cctype>

#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

namespace {

bool ascii_ci_equal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

}  // namespace

// --- CheckGroup ------------------------------------------------------

CheckGroup::CheckGroup(std::vector<std::string> labels)
    : labels_(std::move(labels)), checked_(labels_.size(), false) {
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void CheckGroup::on_attached() {
    if (normal_role_ == ui::kInvalidRole)
        normal_role_ = context().roles->find("ckv.option.normal");
    if (focused_role_ == ui::kInvalidRole)
        focused_role_ = context().roles->find("ckv.option.focused");
}

bool CheckGroup::checked(std::size_t index) const {
    CKV_ASSERT(index < checked_.size());
    return checked_[index];
}

void CheckGroup::set_checked(std::size_t index, bool value) {
    CKV_ASSERT(index < checked_.size());
    if (checked_[index] == value) return;
    checked_[index] = value;
    invalidate();
    if (on_changed) on_changed(index, value);
}

void CheckGroup::toggle(std::size_t index) { set_checked(index, !checked_[index]); }

void CheckGroup::move_cursor(int direction) {
    if (labels_.empty()) return;
    const int n = static_cast<int>(labels_.size());
    cursor_ = static_cast<std::size_t>((static_cast<int>(cursor_) + direction + n) % n);
    invalidate();
}

SizeHint CheckGroup::horizontal_size_hint() const {
    int max_width = 0;
    for (const auto& label : labels_)
        max_width = std::max(max_width, text::text_width(parse_mnemonic(label).display) + 4);
    return SizeHint{max_width, max_width, max_width};
}
SizeHint CheckGroup::vertical_size_hint() const {
    const int h = static_cast<int>(labels_.size());
    return SizeHint{h, h, h};
}

bool CheckGroup::on_key(const KeyEvent& event) {
    switch (event.chord.key) {
        case Key::Up:
        case Key::Left:
            move_cursor(-1);
            return true;
        case Key::Down:
        case Key::Right:
            move_cursor(+1);
            return true;
        case Key::Enter:
            if (!labels_.empty()) toggle(cursor_);
            return true;
        case Key::Char:
            if (event.chord.text == " ") {
                if (!labels_.empty()) toggle(cursor_);
                return true;
            }
            for (std::size_t i = 0; i < labels_.size(); ++i) {
                const auto parsed = parse_mnemonic(labels_[i]);
                if (!parsed.mnemonic.empty() && ascii_ci_equal(parsed.mnemonic, event.chord.text)) {
                    cursor_ = i;
                    toggle(i);
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

bool CheckGroup::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down) return false;
    const Rect abs = absolute_bounds();
    const int row = event.cell.y - abs.y;
    if (row < 0 || static_cast<std::size_t>(row) >= labels_.size()) return false;
    cursor_ = static_cast<std::size_t>(row);
    toggle(cursor_);
    return true;
}

void CheckGroup::on_focus(const FocusEvent& event) {
    has_focus_ = event.gained;
    invalidate();
}

void CheckGroup::draw(scene::Painter& painter) {
    const ui::Theme& theme = *context().theme;
    for (std::size_t i = 0; i < labels_.size(); ++i) {
        const Style style = (has_focus_ && i == cursor_) ? theme.resolve(focused_role_)
                                                          : theme.resolve(normal_role_);
        painter.fill(Rect{0, static_cast<int>(i), bounds().width, 1}, Cell::from_grapheme(" ", style));
        const std::string marker = checked_[i] ? "[x] " : "[ ] ";
        painter.draw_text(Point{0, static_cast<int>(i)}, marker, style);
        painter.draw_text(Point{4, static_cast<int>(i)}, parse_mnemonic(labels_[i]).display, style);
    }
}

// --- RadioGroup --------------------------------------------------------

RadioGroup::RadioGroup(std::vector<std::string> labels) : labels_(std::move(labels)) {
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void RadioGroup::on_attached() {
    if (normal_role_ == ui::kInvalidRole)
        normal_role_ = context().roles->find("ckv.option.normal");
    if (focused_role_ == ui::kInvalidRole)
        focused_role_ = context().roles->find("ckv.option.focused");
}

void RadioGroup::set_selected(int index) {
    if (index != -1 && (index < 0 || static_cast<std::size_t>(index) >= labels_.size())) return;  // out of range
    if (index == selected_) return;
    selected_ = index;
    invalidate();
    if (on_changed) on_changed(selected_);
}

void RadioGroup::move_cursor(int direction) {
    if (labels_.empty()) return;
    const int n = static_cast<int>(labels_.size());
    cursor_ = static_cast<std::size_t>((static_cast<int>(cursor_) + direction + n) % n);
    invalidate();
}

SizeHint RadioGroup::horizontal_size_hint() const {
    int max_width = 0;
    for (const auto& label : labels_)
        max_width = std::max(max_width, text::text_width(parse_mnemonic(label).display) + 4);
    return SizeHint{max_width, max_width, max_width};
}
SizeHint RadioGroup::vertical_size_hint() const {
    const int h = static_cast<int>(labels_.size());
    return SizeHint{h, h, h};
}

bool RadioGroup::on_key(const KeyEvent& event) {
    switch (event.chord.key) {
        case Key::Up:
        case Key::Left:
            move_cursor(-1);
            set_selected(static_cast<int>(cursor_));  // arrow navigation also selects, matching classic radio groups
            return true;
        case Key::Down:
        case Key::Right:
            move_cursor(+1);
            set_selected(static_cast<int>(cursor_));
            return true;
        case Key::Enter:
            if (!labels_.empty()) set_selected(static_cast<int>(cursor_));
            return true;
        case Key::Char:
            if (event.chord.text == " ") {
                if (!labels_.empty()) set_selected(static_cast<int>(cursor_));
                return true;
            }
            for (std::size_t i = 0; i < labels_.size(); ++i) {
                const auto parsed = parse_mnemonic(labels_[i]);
                if (!parsed.mnemonic.empty() && ascii_ci_equal(parsed.mnemonic, event.chord.text)) {
                    cursor_ = i;
                    set_selected(static_cast<int>(i));
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

bool RadioGroup::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down) return false;
    const Rect abs = absolute_bounds();
    const int row = event.cell.y - abs.y;
    if (row < 0 || static_cast<std::size_t>(row) >= labels_.size()) return false;
    cursor_ = static_cast<std::size_t>(row);
    set_selected(row);
    return true;
}

void RadioGroup::on_focus(const FocusEvent& event) {
    has_focus_ = event.gained;
    invalidate();
}

void RadioGroup::draw(scene::Painter& painter) {
    const ui::Theme& theme = *context().theme;
    for (std::size_t i = 0; i < labels_.size(); ++i) {
        const Style style = (has_focus_ && i == cursor_) ? theme.resolve(focused_role_)
                                                          : theme.resolve(normal_role_);
        painter.fill(Rect{0, static_cast<int>(i), bounds().width, 1}, Cell::from_grapheme(" ", style));
        const std::string marker = (static_cast<int>(i) == selected_) ? "(x) " : "( ) ";
        painter.draw_text(Point{0, static_cast<int>(i)}, marker, style);
        painter.draw_text(Point{4, static_cast<int>(i)}, parse_mnemonic(labels_[i]).display, style);
    }
}

}  // namespace ckv::widgets
