// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/input_line.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"

namespace ckv::widgets {

namespace {
bool contains(const Rect& r, Point p) noexcept {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}
}  // namespace

InputLine::InputLine(const ui::Theme& theme, ui::RoleId normal_role, ui::RoleId focused_role,
                      ui::RoleId invalid_role)
    : theme_(theme), normal_role_(normal_role), focused_role_(focused_role), invalid_role_(invalid_role) {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{10, 1});
}

void InputLine::set_text(std::string text) {
    graphemes_.clear();
    for (std::string_view g : text::split_graphemes(text)) graphemes_.emplace_back(g);
    cursor_ = graphemes_.size();
    selection_anchor_.reset();
    invalidate();
}

std::string InputLine::text() const {
    std::string out;
    for (const auto& g : graphemes_) out += g;
    return out;
}

std::pair<std::size_t, std::size_t> InputLine::selection_range() const noexcept {
    if (!selection_anchor_) return {cursor_, cursor_};
    return {std::min(*selection_anchor_, cursor_), std::max(*selection_anchor_, cursor_)};
}

void InputLine::set_valid(bool valid) noexcept {
    if (valid == valid_) return;
    valid_ = valid;
    invalidate();
}

void InputLine::insert_graphemes(const std::vector<std::string>& graphemes) {
    if (graphemes.empty()) return;
    if (selection_anchor_) erase_selection();
    if (overwrite_mode_ && !selection_anchor_) {
        for (const auto& g : graphemes) {
            if (cursor_ < graphemes_.size())
                graphemes_[cursor_] = g;
            else
                graphemes_.push_back(g);
            ++cursor_;
        }
    } else {
        graphemes_.insert(graphemes_.begin() + static_cast<std::ptrdiff_t>(cursor_), graphemes.begin(),
                           graphemes.end());
        cursor_ += graphemes.size();
    }
    selection_anchor_.reset();
    invalidate();
}

void InputLine::erase_selection() {
    if (!selection_anchor_) return;
    const auto [begin, end] = selection_range();
    graphemes_.erase(graphemes_.begin() + static_cast<std::ptrdiff_t>(begin),
                      graphemes_.begin() + static_cast<std::ptrdiff_t>(end));
    cursor_ = begin;
    selection_anchor_.reset();
}

void InputLine::move_cursor(std::size_t new_cursor, bool extend_selection) {
    new_cursor = std::min(new_cursor, graphemes_.size());
    if (extend_selection) {
        if (!selection_anchor_) selection_anchor_ = cursor_;
    } else {
        selection_anchor_.reset();
    }
    cursor_ = new_cursor;
    invalidate();
}

bool InputLine::on_key(const KeyEvent& event) {
    const bool shift = has_modifier(event.chord.modifiers, Modifier::Shift);
    switch (event.chord.key) {
        case Key::Left:
            if (cursor_ > 0) move_cursor(cursor_ - 1, shift);
            return true;
        case Key::Right:
            move_cursor(cursor_ + 1, shift);
            return true;
        case Key::Home:
            move_cursor(0, shift);
            return true;
        case Key::End:
            move_cursor(graphemes_.size(), shift);
            return true;
        case Key::Backspace:
            if (selection_anchor_) {
                erase_selection();
                invalidate();
            } else if (cursor_ > 0) {
                graphemes_.erase(graphemes_.begin() + static_cast<std::ptrdiff_t>(cursor_ - 1));
                --cursor_;
                invalidate();
            }
            return true;
        case Key::Delete:
            if (selection_anchor_) {
                erase_selection();
                invalidate();
            } else if (cursor_ < graphemes_.size()) {
                graphemes_.erase(graphemes_.begin() + static_cast<std::ptrdiff_t>(cursor_));
                invalidate();
            }
            return true;
        case Key::Insert:
            overwrite_mode_ = !overwrite_mode_;
            invalidate();
            return true;
        default:
            return false;
    }
}

bool InputLine::on_text(const TextEvent& event) {
    std::vector<std::string> graphemes;
    for (std::string_view g : text::split_graphemes(event.text)) graphemes.emplace_back(g);
    insert_graphemes(graphemes);
    return true;
}

bool InputLine::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down) return false;
    if (!contains(absolute_bounds(), event.cell)) return false;
    const int local_x = event.cell.x - absolute_bounds().x;
    int col = 0;
    std::size_t index = graphemes_.size();
    for (std::size_t i = 0; i < graphemes_.size(); ++i) {
        const int w = text::grapheme_width(graphemes_[i]);
        if (local_x < col + w) {
            index = i;
            break;
        }
        col += w;
    }
    move_cursor(index, false);
    return true;
}

void InputLine::on_focus(const FocusEvent& event) {
    has_focus_ = event.gained;
    invalidate();
}

SizeHint InputLine::horizontal_size_hint() const {
    return SizeHint{4, std::max(10, text::text_width(text()) + 1), ui::kUnboundedExtent};
}

int InputLine::scroll_offset_for_display() const {
    std::vector<int> col_of(graphemes_.size() + 1, 0);
    for (std::size_t i = 0; i < graphemes_.size(); ++i) col_of[i + 1] = col_of[i] + text::grapheme_width(graphemes_[i]);

    const int width = std::max(bounds().width, 1);
    if (col_of[cursor_] < width) return 0;
    for (std::size_t scroll = cursor_; scroll-- > 0;) {
        if (col_of[cursor_] - col_of[scroll] > width - 1) return static_cast<int>(scroll) + 1;
    }
    return 0;
}

void InputLine::draw(scene::Painter& painter) {
    const ui::RoleId role = !valid_ ? invalid_role_ : (has_focus_ ? focused_role_ : normal_role_);
    const Style base = theme_.resolve(role);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", base));

    std::vector<int> col_of(graphemes_.size() + 1, 0);
    for (std::size_t i = 0; i < graphemes_.size(); ++i) col_of[i + 1] = col_of[i] + text::grapheme_width(graphemes_[i]);

    const int scroll = scroll_offset_for_display();
    const auto [sel_begin, sel_end] = selection_range();

    for (std::size_t i = static_cast<std::size_t>(scroll); i < graphemes_.size(); ++i) {
        const int x = col_of[i] - col_of[scroll];
        if (x >= bounds().width) break;
        Style style = base;
        if (i >= sel_begin && i < sel_end) style.attrs |= Attr::Reverse;
        painter.draw_text(Point{x, 0}, graphemes_[i], style);
    }

    if (has_focus_) {
        const int cursor_x = col_of[cursor_] - col_of[scroll];
        if (cursor_x >= 0 && cursor_x < bounds().width) {
            Style cursor_style = base;
            cursor_style.attrs |= Attr::Reverse;
            const std::string glyph = cursor_ < graphemes_.size() ? graphemes_[cursor_] : std::string(" ");
            painter.draw_text(Point{cursor_x, 0}, glyph, cursor_style);
        }
    }
}

}  // namespace ckv::widgets
