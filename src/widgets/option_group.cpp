// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/option_group.hpp"

#include <algorithm>
#include <cctype>

#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic.hpp"
#include "cvision/widgets/mnemonic_internal.hpp"

namespace ckv::widgets {

namespace {

bool ascii_ci_equal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

bool is_mnemonic_request(const KeyEvent& event) noexcept {
    return event.chord.key == Key::Char && !event.chord.text.empty() &&
           !has_modifier(event.chord.modifiers, Modifier::Ctrl) &&
           !has_modifier(event.chord.modifiers, Modifier::Super);
}

bool is_space_request(const KeyEvent& event) noexcept {
    return event.chord.key == Key::Char && event.chord.text == " " &&
           event.chord.modifiers == Modifier::None;
}

bool state_as_bool(CheckState state) noexcept { return state == CheckState::Checked; }

int group_label_rows(std::string_view label) noexcept { return label.empty() ? 0 : 1; }

Style group_label_style(const ui::Theme& theme, ui::RoleId label_role, ui::RoleId focused_option_role,
                        bool focused) {
    const Style label = theme.resolve(label_role);
    if (!focused) return label;
    // The caption must remain on the dialog/window surface; only its
    // foreground communicates that this option group owns keyboard focus.
    return Style{theme.resolve(focused_option_role).fg, label.bg, label.attrs};
}

CheckState toggled_state(CheckState state, bool tristate) noexcept {
    if (!tristate) return state == CheckState::Checked ? CheckState::Unchecked : CheckState::Checked;
    switch (state) {
        case CheckState::Unchecked:
            return CheckState::Checked;
        case CheckState::Checked:
            return CheckState::Mixed;
        case CheckState::Mixed:
            return CheckState::Unchecked;
    }
    CKV_ASSERT(false);
    return CheckState::Unchecked;
}

}  // namespace

// --- CheckGroup ------------------------------------------------------

CheckGroup::CheckGroup(std::vector<std::string> labels)
    : labels_(std::move(labels)), states_(labels_.size(), CheckState::Unchecked) {
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void CheckGroup::set_group_label(std::string label) {
    if (group_label_ == label) return;
    group_label_ = std::move(label);
    invalidate();
    size_hint_changed();
}

void CheckGroup::set_column_width(int columns) {
    CKV_ASSERT(columns >= 0);
    if (column_width_ == columns) return;
    column_width_ = columns;
    invalidate();
    size_hint_changed();
}

void CheckGroup::on_attached() {
    if (normal_role_ == ui::kInvalidRole)
        normal_role_ = context().roles->find("ckv.option.normal");
    if (focused_role_ == ui::kInvalidRole)
        focused_role_ = context().roles->find("ckv.option.focused");
    if (mnemonic_role_ == ui::kInvalidRole)
        mnemonic_role_ = context().roles->find("ckv.label.mnemonic");
    if (group_label_role_ == ui::kInvalidRole)
        group_label_role_ = context().roles->find("ckv.label.text");
}

bool CheckGroup::checked(std::size_t index) const {
    return state_as_bool(check_state(index));
}

void CheckGroup::set_checked(std::size_t index, bool value) {
    set_check_state(index, value ? CheckState::Checked : CheckState::Unchecked);
}

CheckState CheckGroup::check_state(std::size_t index) const {
    CKV_ASSERT(index < states_.size());
    return states_[index];
}

void CheckGroup::set_check_state(std::size_t index, CheckState state) {
    CKV_ASSERT(index < states_.size());
    if (states_[index] == state) return;
    states_[index] = state;
    invalidate();
    if (on_state_changed) on_state_changed(index, state);
    if (on_changed) on_changed(index, state_as_bool(state));
}

void CheckGroup::toggle(std::size_t index) { set_check_state(index, toggled_state(states_[index], tristate_)); }

void CheckGroup::move_cursor(int direction) {
    if (labels_.empty()) return;
    const int n = static_cast<int>(labels_.size());
    cursor_ = static_cast<std::size_t>((static_cast<int>(cursor_) + direction + n) % n);
    invalidate();
}

SizeHint CheckGroup::horizontal_size_hint() const {
    int max_width = text::text_width(group_label_);
    const int item_indent = group_label_.empty() ? 0 : 1;
    for (const auto& label : labels_)
        max_width = std::max(max_width, item_indent + text::text_width(parse_mnemonic(label).display) + 4);
    if (column_width_ != 0) max_width = column_width_;
    return SizeHint{max_width, max_width, max_width};
}
SizeHint CheckGroup::vertical_size_hint() const {
    const int h = group_label_rows(group_label_) + static_cast<int>(labels_.size());
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
        // Enter is deliberately not handled. Space is what ticks a box —
        // in this toolkit and in every other — and Enter belongs to the
        // form: it presses the dialog's default button. A group that
        // swallowed it left OK unreachable from the keyboard for as long as
        // any box had focus, which in a settings dialog is from the moment
        // it opens.
        case Key::Char:
            if (is_space_request(event)) {
                if (!labels_.empty()) toggle(cursor_);
                return true;
            }
            if (!is_mnemonic_request(event)) return false;
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
    const int row = event.cell.y - abs.y - group_label_rows(group_label_);
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
    const int label_rows = group_label_rows(group_label_);
    if (label_rows != 0) {
        const Style title_style = group_label_style(theme, group_label_role_, focused_role_, has_focus_);
        painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", title_style));
        painter.draw_text(Point{0, 0}, text::clip_to_width(group_label_, bounds().width), title_style);
    }
    for (std::size_t i = 0; i < labels_.size(); ++i) {
        const Style style = (has_focus_ && i == cursor_) ? theme.resolve(focused_role_)
                                                          : theme.resolve(normal_role_);
        const int row = label_rows + static_cast<int>(i);
        const int item_indent = label_rows == 0 ? 0 : 1;
        painter.fill(Rect{0, row, bounds().width, 1}, Cell::from_grapheme(" ", style));
        const std::string marker = states_[i] == CheckState::Checked ? "[X] "
                                 : states_[i] == CheckState::Mixed   ? "[~] "
                                                                     : "[ ] ";
        painter.draw_text(Point{item_indent, row}, marker, style);
        draw_mnemonic(painter, Point{item_indent + 4, row}, parse_mnemonic(labels_[i]),
                      bounds().width - item_indent - 4,
                      style, accent_style(style, theme.resolve(mnemonic_role_)));
    }
}

// --- RadioGroup --------------------------------------------------------

RadioGroup::RadioGroup(std::vector<std::string> labels) : labels_(std::move(labels)) {
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void RadioGroup::set_group_label(std::string label) {
    if (group_label_ == label) return;
    group_label_ = std::move(label);
    invalidate();
    size_hint_changed();
}

void RadioGroup::on_attached() {
    if (normal_role_ == ui::kInvalidRole)
        normal_role_ = context().roles->find("ckv.option.normal");
    if (focused_role_ == ui::kInvalidRole)
        focused_role_ = context().roles->find("ckv.option.focused");
    if (mnemonic_role_ == ui::kInvalidRole)
        mnemonic_role_ = context().roles->find("ckv.label.mnemonic");
    if (group_label_role_ == ui::kInvalidRole)
        group_label_role_ = context().roles->find("ckv.label.text");
}

void RadioGroup::set_selected(int index) {
    if (index != -1 && (index < 0 || static_cast<std::size_t>(index) >= labels_.size())) return;  // out of range
    // The keyboard cursor follows a programmatic selection, exactly as it
    // follows a mouse click (on_mouse sets cursor_ before toggling). Without
    // this, a dialog whose radio group opens with row N selected still has its
    // cursor on row 0 — and the reader's first Up, wrapping from 0 to the last
    // row, re-selects the very row they were trying to arrow away from.
    if (index >= 0) cursor_ = static_cast<std::size_t>(index);
    if (index == selected_) return;
    selected_ = index;
    invalidate();
    if (on_changed) on_changed(selected_);
}

void RadioGroup::set_column_width(int columns) {
    CKV_ASSERT(columns >= 0);
    if (column_width_ == columns) return;
    column_width_ = columns;
    invalidate();
    size_hint_changed();
}

void RadioGroup::move_cursor(int direction) {
    if (labels_.empty()) return;
    const int n = static_cast<int>(labels_.size());
    cursor_ = static_cast<std::size_t>((static_cast<int>(cursor_) + direction + n) % n);
    invalidate();
}

SizeHint RadioGroup::horizontal_size_hint() const {
    int max_width = text::text_width(group_label_);
    const int item_indent = group_label_.empty() ? 0 : 1;
    for (const auto& label : labels_)
        max_width = std::max(max_width, item_indent + text::text_width(parse_mnemonic(label).display) + 4);
    if (column_width_ != 0) max_width = column_width_;
    return SizeHint{max_width, max_width, max_width};
}
SizeHint RadioGroup::vertical_size_hint() const {
    const int h = group_label_rows(group_label_) + static_cast<int>(labels_.size());
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
        // Enter is deliberately not handled, for the reason CheckGroup gives
        // above. A radio group is doubly safe here: its arrow keys already
        // select as they move, so there is nothing Enter was needed for.
        case Key::Char:
            if (is_space_request(event)) {
                if (!labels_.empty()) set_selected(static_cast<int>(cursor_));
                return true;
            }
            if (!is_mnemonic_request(event)) return false;
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
    const int row = event.cell.y - abs.y - group_label_rows(group_label_);
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
    const int label_rows = group_label_rows(group_label_);
    if (label_rows != 0) {
        const Style title_style = group_label_style(theme, group_label_role_, focused_role_, has_focus_);
        painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", title_style));
        painter.draw_text(Point{0, 0}, text::clip_to_width(group_label_, bounds().width), title_style);
    }
    for (std::size_t i = 0; i < labels_.size(); ++i) {
        const Style style = (has_focus_ && i == cursor_) ? theme.resolve(focused_role_)
                                                          : theme.resolve(normal_role_);
        const int row = label_rows + static_cast<int>(i);
        const int item_indent = label_rows == 0 ? 0 : 1;
        painter.fill(Rect{0, row, bounds().width, 1}, Cell::from_grapheme(" ", style));
        const std::string marker = (static_cast<int>(i) == selected_) ? "(•) " : "( ) ";
        painter.draw_text(Point{item_indent, row}, marker, style);
        draw_mnemonic(painter, Point{item_indent + 4, row}, parse_mnemonic(labels_[i]),
                      bounds().width - item_indent - 4,
                      style, accent_style(style, theme.resolve(mnemonic_role_)));
    }
}

}  // namespace ckv::widgets
