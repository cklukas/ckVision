// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/input_line.hpp"

#include <algorithm>
#include <cctype>

#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"

namespace ckv::widgets {

namespace {
bool contains(const Rect& r, Point p) noexcept {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}

bool word_grapheme(std::string_view grapheme) noexcept {
    if (grapheme.empty()) return false;
    const unsigned char value = static_cast<unsigned char>(grapheme.front());
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '_';
}
}  // namespace

InputLine::InputLine() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{10, 1});
}

void InputLine::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
    if (invalid_role_ == ui::kInvalidRole) invalid_role_ = context().roles->find("ckv.input.invalid");
}

void InputLine::set_text(std::string text) {
    graphemes_.clear();
    for (std::string_view g : text::split_graphemes(text)) graphemes_.emplace_back(g);
    cursor_ = graphemes_.size();
    selection_anchor_.reset();
    undo_stack_.clear();
    dragging_selection_ = false;
    invalidate();
    revalidate();
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

void InputLine::set_validator(std::function<bool(const std::string&)> validator) {
    validator_ = std::move(validator);
    revalidate();
}

void InputLine::set_grapheme_filter(std::function<bool(std::string_view)> filter) {
    grapheme_filter_ = std::move(filter);
}

void InputLine::revalidate() {
    if (!validator_) return;
    set_valid(validator_(text()));
}

void InputLine::set_password_echo(bool enabled, char echo_char) {
    password_echo_ = enabled;
    echo_char_ = echo_char;
    invalidate();
}

void InputLine::set_history(ui::HistoryRegistry* registry, std::string key) {
    history_registry_ = registry;
    history_key_ = std::move(key);
    history_index_ = -1;
}

void InputLine::commit_to_history() {
    if (history_registry_ == nullptr) return;
    history_registry_->record(history_key_, text());
    history_index_ = -1;
}

void InputLine::record_undo_state() {
    if (undo_stack_.size() == kMaxUndoDepth) undo_stack_.erase(undo_stack_.begin());
    undo_stack_.push_back(EditState{graphemes_, cursor_, selection_anchor_, overwrite_mode_});
}

bool InputLine::undo() {
    if (undo_stack_.empty()) return false;
    EditState state = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    graphemes_ = std::move(state.graphemes);
    cursor_ = state.cursor;
    selection_anchor_ = state.selection_anchor;
    overwrite_mode_ = state.overwrite_mode;
    dragging_selection_ = false;
    invalidate();
    revalidate();
    return true;
}

std::string InputLine::selected_text() const {
    if (!selection_anchor_) return {};
    const auto [begin, end] = selection_range();
    std::string out;
    for (std::size_t i = begin; i < end; ++i) out += graphemes_[i];
    return out;
}

std::size_t InputLine::previous_word(std::size_t from) const noexcept {
    std::size_t position = std::min(from, graphemes_.size());
    while (position > 0U && !word_grapheme(graphemes_[position - 1U])) --position;
    while (position > 0U && word_grapheme(graphemes_[position - 1U])) --position;
    return position;
}

std::size_t InputLine::next_word(std::size_t from) const noexcept {
    std::size_t position = std::min(from, graphemes_.size());
    while (position < graphemes_.size() && word_grapheme(graphemes_[position])) ++position;
    while (position < graphemes_.size() && !word_grapheme(graphemes_[position])) ++position;
    return position;
}

void InputLine::erase_range(std::size_t begin, std::size_t end) {
    begin = std::min(begin, graphemes_.size());
    end = std::min(std::max(begin, end), graphemes_.size());
    if (mask_.empty()) {
        graphemes_.erase(graphemes_.begin() + static_cast<std::ptrdiff_t>(begin),
                         graphemes_.begin() + static_cast<std::ptrdiff_t>(end));
    } else {
        for (std::size_t position = begin; position < end; ++position)
            if (mask_position_editable(position)) graphemes_[position] = std::string(1, mask_placeholder_);
    }
    cursor_ = begin;
    selection_anchor_.reset();
}

bool InputLine::copy_selection_to_clipboard() {
    if (context().app == nullptr || !selection_anchor_) return false;
    const std::string selected = selected_text();
    if (selected.empty()) return false;
    context().app->set_clipboard_text(selected);
    return true;
}

bool InputLine::cut_selection_to_clipboard() {
    if (!selection_anchor_ || selected_text().empty()) return false;
    if (context().app == nullptr) return false;
    record_undo_state();
    context().app->set_clipboard_text(selected_text());
    const auto [begin, end] = selection_range();
    erase_range(begin, end);
    revalidate();
    invalidate();
    return true;
}

bool InputLine::paste_from_clipboard() {
    if (context().app == nullptr) return false;
    const std::string& clip = context().app->clipboard_text();
    if (clip.empty()) return false;
    return on_text(TextEvent{clip, false});
}

void InputLine::history_show(int index) {
    if (history_registry_ == nullptr) return;
    const auto& entries = history_registry_->entries(history_key_);
    if (history_index_ == -1 && index != -1) history_saved_text_ = text();
    history_index_ = index;
    if (index == -1) {
        set_text(history_saved_text_);
    } else if (static_cast<std::size_t>(index) < entries.size()) {
        set_text(entries[static_cast<std::size_t>(index)]);
    }
}

// --- Input masks ---------------------------------------------------------

void InputLine::set_mask(std::string mask, char placeholder) {
    mask_ = std::move(mask);
    mask_placeholder_ = placeholder;
    if (mask_.empty()) {
        // Disabling the mask: the current (masked) buffer becomes the
        // free-form text as-is — no reinterpretation, no reset.
        cursor_ = std::min(cursor_, graphemes_.size());
        selection_anchor_.reset();
        invalidate();
        revalidate();
        return;
    }
    graphemes_.assign(mask_.size(), std::string());
    for (std::size_t i = 0; i < mask_.size(); ++i)
        graphemes_[i] = mask_position_editable(i) ? std::string(1, mask_placeholder_) : std::string(1, mask_[i]);
    const std::size_t first = mask_next_editable(0);
    cursor_ = first < graphemes_.size() ? first : 0;
    selection_anchor_.reset();
    invalidate();
    revalidate();
}

bool InputLine::mask_position_editable(std::size_t index) const noexcept {
    if (index >= mask_.size()) return false;
    return mask_[index] == '9' || mask_[index] == 'A' || mask_[index] == '*';
}

bool InputLine::mask_char_accepts(char mask_char, std::string_view grapheme) const noexcept {
    if (grapheme.empty()) return false;
    switch (mask_char) {
        case '9':
            return grapheme.size() == 1 && std::isdigit(static_cast<unsigned char>(grapheme[0])) != 0;
        case 'A':
            return grapheme.size() == 1 && std::isalpha(static_cast<unsigned char>(grapheme[0])) != 0;
        case '*':
            return true;
        default:
            return false;
    }
}

std::size_t InputLine::mask_next_editable(std::size_t from) const noexcept {
    for (std::size_t i = from; i < mask_.size(); ++i)
        if (mask_position_editable(i)) return i;
    return graphemes_.size();
}

std::size_t InputLine::mask_previous_editable(std::size_t from) const noexcept {
    for (std::size_t i = from; i-- > 0;)
        if (mask_position_editable(i)) return i;
    return graphemes_.size();
}

bool InputLine::on_key_masked(const KeyEvent& event) {
    const bool shift = has_modifier(event.chord.modifiers, Modifier::Shift);
    const auto move_mask_cursor = [this, shift](std::size_t target) {
        if (shift) {
            if (!selection_anchor_) selection_anchor_ = cursor_;
        } else {
            selection_anchor_.reset();
        }
        cursor_ = target;
        invalidate();
    };
    switch (event.chord.key) {
        case Key::Left: {
            const std::size_t prev = mask_previous_editable(cursor_);
            if (prev < graphemes_.size()) {
                move_mask_cursor(prev);
            }
            return true;
        }
        case Key::Right: {
            const std::size_t next = mask_next_editable(cursor_ + 1);
            move_mask_cursor(next);
            return true;
        }
        case Key::Home: {
            const std::size_t first = mask_next_editable(0);
            move_mask_cursor(first < graphemes_.size() ? first : 0);
            return true;
        }
        case Key::End: {
            std::size_t last = graphemes_.size();
            for (std::size_t i = mask_.size(); i-- > 0;) {
                if (mask_position_editable(i)) {
                    last = i;
                    break;
                }
            }
            move_mask_cursor(last < graphemes_.size() ? last : 0);
            return true;
        }
        case Key::Backspace: {
            if (selection_anchor_) {
                record_undo_state();
                const auto [begin, end] = selection_range();
                erase_range(begin, end);
                revalidate();
                invalidate();
                return true;
            }
            const std::size_t prev = mask_previous_editable(cursor_);
            if (prev < graphemes_.size()) {
                record_undo_state();
                graphemes_[prev] = std::string(1, mask_placeholder_);
                cursor_ = prev;
                invalidate();
                revalidate();
            }
            return true;
        }
        case Key::Delete: {
            if (selection_anchor_) {
                record_undo_state();
                const auto [begin, end] = selection_range();
                erase_range(begin, end);
                revalidate();
                invalidate();
                return true;
            }
            if (cursor_ < graphemes_.size() && mask_position_editable(cursor_)) {
                record_undo_state();
                graphemes_[cursor_] = std::string(1, mask_placeholder_);
                invalidate();
                revalidate();
            }
            return true;
        }
        default:
            return false;
    }
}

bool InputLine::on_text_masked(const TextEvent& event) {
    if (selection_anchor_) {
        record_undo_state();
        const auto [begin, end] = selection_range();
        erase_range(begin, end);
        cursor_ = mask_next_editable(begin);
    }
    bool changed = false;
    for (std::string_view g : text::split_graphemes(event.text)) {
        if (grapheme_filter_ && !grapheme_filter_(g)) continue;
        const std::size_t pos = mask_next_editable(cursor_);
        if (pos >= graphemes_.size()) break;
        if (!mask_char_accepts(mask_[pos], g)) continue;
        if (!changed) record_undo_state();
        graphemes_[pos] = std::string(g);
        cursor_ = mask_next_editable(pos + 1);
        changed = true;
    }
    if (changed) {
        invalidate();
        revalidate();
    }
    return true;
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
    revalidate();
}

void InputLine::erase_selection() {
    if (!selection_anchor_) return;
    const auto [begin, end] = selection_range();
    erase_range(begin, end);
    revalidate();
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
    if (event.chord.key == Key::Enter && event.chord.modifiers == Modifier::None && on_accept) {
        on_accept();
        return true;
    }
    // A disabled field is not merely greyed: it accepts nothing, the same
    // contract TextEditor keeps.
    if (!enabled()) return false;
    const bool has_ctrl = has_modifier(event.chord.modifiers, Modifier::Ctrl);
    const bool has_shift = has_modifier(event.chord.modifiers, Modifier::Shift);
    const bool has_alt_or_super = has_modifier(event.chord.modifiers, Modifier::Alt) ||
                                  has_modifier(event.chord.modifiers, Modifier::Super);
    if (event.chord.key == Key::Insert && !has_alt_or_super) {
        if (has_ctrl) return copy_selection_to_clipboard();
        if (has_shift) return paste_from_clipboard();
    }
    if (event.chord.key == Key::Delete && has_shift && !has_ctrl && !has_alt_or_super)
        return cut_selection_to_clipboard();
    if (event.chord.key == Key::Char && has_modifier(event.chord.modifiers, Modifier::Ctrl) &&
        !has_modifier(event.chord.modifiers, Modifier::Alt) &&
        !has_modifier(event.chord.modifiers, Modifier::Super) && event.chord.text.size() == 1) {
        const char chord = static_cast<char>(std::tolower(static_cast<unsigned char>(event.chord.text[0])));
        if (chord == 'c') return copy_selection_to_clipboard();
        if (chord == 'x') return cut_selection_to_clipboard();
        if (chord == 'v') return paste_from_clipboard();
        if (chord == 'z') return undo();
    }

    // Terminals report ordinary typed characters as Key::Char events, whereas
    // IMEs and bracketed paste arrive as TextEvent. Normalize the former at
    // the editing boundary so both paths obey the same insertion, validation,
    // mask, and selection contract. Alt/Ctrl/Super characters remain chords
    // for command routing; Shift is part of normal text production.
    if (event.chord.key == Key::Char && !has_modifier(event.chord.modifiers, Modifier::Alt) &&
        !has_modifier(event.chord.modifiers, Modifier::Ctrl) &&
        !has_modifier(event.chord.modifiers, Modifier::Super))
        return on_text(TextEvent{event.chord.text, false});

    // History recall is gated on !has_mask(): history_show() calls the
    // free-form set_text() directly, which would splice arbitrary
    // unmasked text into a masked field's fixed-length, class-
    // constrained buffer and corrupt subsequent mask-aware editing.
    // Masked history is a real, separate feature (recalling only
    // entries that already satisfy the mask), not something safe to
    // bolt onto set_text() as-is — deferred rather than done unsoundly.
    if (!has_mask() && history_registry_ != nullptr && event.chord.key == Key::Up) {
        const auto& entries = history_registry_->entries(history_key_);
        if (!entries.empty()) history_show(std::min(history_index_ + 1, static_cast<int>(entries.size()) - 1));
        return true;
    }
    if (!has_mask() && history_registry_ != nullptr && event.chord.key == Key::Down) {
        if (history_index_ >= 0) history_show(history_index_ - 1);
        return true;
    }
    if (has_mask()) return on_key_masked(event);

    const bool shift = has_shift;
    const bool control = has_ctrl;
    switch (event.chord.key) {
        case Key::Left:
            if (cursor_ > 0) move_cursor(control ? previous_word(cursor_) : cursor_ - 1, shift);
            return true;
        case Key::Right:
            move_cursor(control ? next_word(cursor_) : cursor_ + 1, shift);
            return true;
        case Key::Home:
            move_cursor(0, shift);
            return true;
        case Key::End:
            move_cursor(graphemes_.size(), shift);
            return true;
        case Key::Backspace:
            if (selection_anchor_) {
                record_undo_state();
                erase_selection();
                invalidate();
            } else if (cursor_ > 0) {
                record_undo_state();
                const std::size_t begin = control ? previous_word(cursor_) : cursor_ - 1U;
                graphemes_.erase(graphemes_.begin() + static_cast<std::ptrdiff_t>(begin),
                                 graphemes_.begin() + static_cast<std::ptrdiff_t>(cursor_));
                cursor_ = begin;
                invalidate();
                revalidate();
            }
            return true;
        case Key::Delete:
            if (selection_anchor_) {
                record_undo_state();
                erase_selection();
                invalidate();
            } else if (cursor_ < graphemes_.size()) {
                record_undo_state();
                const std::size_t end = control ? next_word(cursor_) : cursor_ + 1U;
                graphemes_.erase(graphemes_.begin() + static_cast<std::ptrdiff_t>(cursor_),
                                 graphemes_.begin() + static_cast<std::ptrdiff_t>(end));
                invalidate();
                revalidate();
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
    if (!enabled()) return false;
    if (has_mask()) return on_text_masked(event);
    std::vector<std::string> graphemes;
    for (std::string_view g : text::split_graphemes(event.text)) {
        if (!grapheme_filter_ || grapheme_filter_(g)) graphemes.emplace_back(g);
    }
    if (!graphemes.empty()) record_undo_state();
    insert_graphemes(graphemes);
    return true;
}

bool InputLine::on_mouse(const MouseEvent& event) {
    if (event.action == MouseAction::Up && dragging_selection_) {
        dragging_selection_ = false;
        return true;
    }
    if (event.action == MouseAction::Move && dragging_selection_) {
        move_cursor(cursor_index_at(event.cell), true);
        return true;
    }
    if (event.action != MouseAction::Down) return false;
    if (!contains(absolute_bounds(), event.cell)) return false;
    dragging_selection_ = true;
    move_cursor(cursor_index_at(event.cell), false);
    return true;
}

std::size_t InputLine::cursor_index_at(Point absolute_cell) const {
    const int local_x = absolute_cell.x - absolute_bounds().x;
    if (local_x <= 0) return static_cast<std::size_t>(scroll_offset_for_display());
    const int scroll = scroll_offset_for_display();
    int col = 0;
    std::size_t index = graphemes_.size();
    for (std::size_t i = static_cast<std::size_t>(scroll); i < graphemes_.size(); ++i) {
        const int w = text::grapheme_width(graphemes_[i]);
        if (local_x < col + w) {
            index = i;
            break;
        }
        col += w;
    }
    return index;
}

void InputLine::on_focus(const FocusEvent& event) {
    has_focus_ = event.gained;
    invalidate();
}

SizeHint InputLine::horizontal_size_hint() const {
    if (has_mask()) {
        const int width = static_cast<int>(mask_.size());
        return SizeHint{width, width, width};
    }
    return SizeHint{4, std::max(10, text::text_width(text()) + 1), ui::kUnboundedExtent};
}

int InputLine::scroll_offset_for_display() const {
    // Password echo always draws one column per grapheme (the echo
    // glyph is a single ASCII char) regardless of the real grapheme's
    // width, so column math must follow the SAME rule here as in
    // draw() — using real widths for one and echo widths for the other
    // would desync the cursor's displayed column from its scroll math.
    std::vector<int> col_of(graphemes_.size() + 1, 0);
    for (std::size_t i = 0; i < graphemes_.size(); ++i)
        col_of[i + 1] = col_of[i] + (password_echo_ ? 1 : text::grapheme_width(graphemes_[i]));

    const int width = std::max(bounds().width, 1);
    // Text that fits in the field never scrolls. Making room for the cursor
    // past the last character would push the first one out of sight, so a
    // four-cell field holding four digits would show three of them.
    if (col_of.back() <= width) return 0;
    if (col_of[cursor_] < width) return 0;
    for (std::size_t scroll = cursor_; scroll-- > 0;) {
        if (col_of[cursor_] - col_of[scroll] > width - 1) return static_cast<int>(scroll) + 1;
    }
    return 0;
}

void InputLine::draw(scene::Painter& painter) {
    const ui::RoleId role = !valid_ ? invalid_role_ : (has_focus_ ? focused_role_ : normal_role_);
    const Style base = context().theme->resolve(role);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", base));

    std::vector<int> col_of(graphemes_.size() + 1, 0);
    for (std::size_t i = 0; i < graphemes_.size(); ++i)
        col_of[i + 1] = col_of[i] + (password_echo_ ? 1 : text::grapheme_width(graphemes_[i]));

    const int scroll = scroll_offset_for_display();
    const auto [sel_begin, sel_end] = selection_range();

    for (std::size_t i = static_cast<std::size_t>(scroll); i < graphemes_.size(); ++i) {
        const int x = col_of[i] - col_of[scroll];
        if (x >= bounds().width) break;
        Style style = base;
        if (i >= sel_begin && i < sel_end) style.attrs |= Attr::Reverse;
        const std::string glyph = password_echo_ ? std::string(1, echo_char_) : graphemes_[i];
        painter.draw_text(Point{x, 0}, glyph, style);
    }

    if (has_focus_) {
        const int cursor_x = col_of[cursor_] - col_of[scroll];
        if (cursor_x >= 0 && cursor_x < bounds().width) {
            Style cursor_style = base;
            cursor_style.attrs |= Attr::Reverse;
            std::string glyph = cursor_ < graphemes_.size() ? graphemes_[cursor_] : std::string(" ");
            if (password_echo_ && cursor_ < graphemes_.size()) glyph = std::string(1, echo_char_);
            painter.draw_text(Point{cursor_x, 0}, glyph, cursor_style);
        }
    }
}

}  // namespace ckv::widgets
