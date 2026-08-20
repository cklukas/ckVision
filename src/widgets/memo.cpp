// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/memo.hpp"

#include "cvision/widgets/text_layout.hpp"

#include <algorithm>
#include <cctype>

#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"

namespace ckv::widgets {

namespace {
bool position_less(const MemoPosition& a, const MemoPosition& b) noexcept {
    return a.line != b.line ? a.line < b.line : a.column < b.column;
}

bool word_grapheme(std::string_view grapheme) noexcept {
    if (grapheme.empty()) return false;
    const unsigned char value = static_cast<unsigned char>(grapheme.front());
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '_';
}
}  // namespace

Memo::Memo() {
    scrollbar_ = make<Scrollbar>(Orientation::Vertical);
    scrollbar_->set_policy(ScrollbarPolicy::Auto);
    h_scrollbar_ = make<Scrollbar>(Orientation::Horizontal);
    h_scrollbar_->set_policy(ScrollbarPolicy::Auto);
    set_focus_policy(ui::FocusPolicy::TabStop);
    lines_.emplace_back();
}

void Memo::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.memo.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.memo.focused");
}

void Memo::set_text(std::string text) {
    lines_.clear();
    std::size_t start = 0;
    while (true) {
        const std::size_t nl = text.find('\n', start);
        const std::string_view segment =
            nl == std::string::npos ? std::string_view(text).substr(start) : std::string_view(text).substr(start, nl - start);
        std::vector<std::string> line;
        for (std::string_view g : text::split_graphemes(segment)) line.emplace_back(g);
        lines_.push_back(std::move(line));
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    cursor_ = {0, 0};
    selection_anchor_.reset();
    undo_stack_.clear();
    dragging_selection_ = false;
    on_resized();
    ensure_cursor_visible();
    invalidate();
}

std::string Memo::text() const {
    std::string out;
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        for (const auto& g : lines_[i]) out += g;
        if (i + 1 < lines_.size()) out += '\n';
    }
    return out;
}

void Memo::set_wrap_mode(WrapMode mode) {
    if (wrap_mode_ == mode) return;
    wrap_mode_ = mode;
    on_resized();
    ensure_cursor_visible();
    invalidate();
}

int Memo::line_length(int line) const noexcept {
    if (line < 0 || static_cast<std::size_t>(line) >= lines_.size()) return 0;
    return static_cast<int>(lines_[static_cast<std::size_t>(line)].size());
}

MemoPosition Memo::clamp_position(MemoPosition p) const noexcept {
    p.line = std::clamp(p.line, 0, static_cast<int>(lines_.size()) - 1);
    p.column = std::clamp(p.column, 0, line_length(p.line));
    return p;
}

std::pair<MemoPosition, MemoPosition> Memo::selection_range() const noexcept {
    if (!selection_anchor_) return {cursor_, cursor_};
    return position_less(*selection_anchor_, cursor_) ? std::make_pair(*selection_anchor_, cursor_)
                                                        : std::make_pair(cursor_, *selection_anchor_);
}

MemoPosition Memo::previous_word(MemoPosition from) const noexcept {
    MemoPosition position = clamp_position(from);
    const auto previous = [this](MemoPosition value) {
        if (value.column > 0) {
            --value.column;
        } else if (value.line > 0) {
            --value.line;
            value.column = line_length(value.line);
        }
        return value;
    };
    const auto at_word = [this](MemoPosition value) {
        return value.column < line_length(value.line) &&
               word_grapheme(lines_[static_cast<std::size_t>(value.line)][static_cast<std::size_t>(value.column)]);
    };
    while (position.line != 0 || position.column != 0) {
        const MemoPosition prior = previous(position);
        if (at_word(prior)) break;
        position = prior;
    }
    while (position.line != 0 || position.column != 0) {
        const MemoPosition prior = previous(position);
        if (!at_word(prior)) break;
        position = prior;
    }
    return position;
}

MemoPosition Memo::next_word(MemoPosition from) const noexcept {
    MemoPosition position = clamp_position(from);
    const auto next = [this](MemoPosition value) {
        if (value.column < line_length(value.line)) {
            ++value.column;
        } else if (value.line + 1 < static_cast<int>(lines_.size())) {
            ++value.line;
            value.column = 0;
        }
        return value;
    };
    const auto at_word = [this](MemoPosition value) {
        return value.column < line_length(value.line) &&
               word_grapheme(lines_[static_cast<std::size_t>(value.line)][static_cast<std::size_t>(value.column)]);
    };
    while (position.line < static_cast<int>(lines_.size()) - 1 || position.column < line_length(position.line)) {
        if (!at_word(position)) break;
        position = next(position);
    }
    while (position.line < static_cast<int>(lines_.size()) - 1 || position.column < line_length(position.line)) {
        if (at_word(position)) break;
        position = next(position);
    }
    return position;
}

void Memo::record_undo_state() {
    if (undo_stack_.size() == kMaxUndoDepth) undo_stack_.erase(undo_stack_.begin());
    undo_stack_.push_back(EditState{lines_, cursor_, selection_anchor_});
}

bool Memo::undo() {
    if (undo_stack_.empty()) return false;
    EditState state = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    lines_ = std::move(state.lines);
    cursor_ = state.cursor;
    selection_anchor_ = state.selection_anchor;
    dragging_selection_ = false;
    on_resized();
    ensure_cursor_visible();
    invalidate();
    return true;
}

std::string Memo::selected_text() const {
    if (!selection_anchor_) return {};
    const auto [begin, end] = selection_range();
    if (begin == end) return {};

    std::string out;
    if (begin.line == end.line) {
        const auto& line = lines_[static_cast<std::size_t>(begin.line)];
        for (int column = begin.column; column < end.column; ++column) out += line[static_cast<std::size_t>(column)];
        return out;
    }

    const auto& first_line = lines_[static_cast<std::size_t>(begin.line)];
    for (int column = begin.column; column < static_cast<int>(first_line.size()); ++column)
        out += first_line[static_cast<std::size_t>(column)];
    out += '\n';

    for (int line_index = begin.line + 1; line_index < end.line; ++line_index) {
        for (const auto& grapheme : lines_[static_cast<std::size_t>(line_index)]) out += grapheme;
        out += '\n';
    }

    const auto& last_line = lines_[static_cast<std::size_t>(end.line)];
    for (int column = 0; column < end.column; ++column) out += last_line[static_cast<std::size_t>(column)];
    return out;
}

bool Memo::copy_selection_to_clipboard() {
    if (context().app == nullptr || !selection_anchor_) return false;
    const std::string selected = selected_text();
    if (selected.empty()) return false;
    context().app->set_clipboard_text(selected);
    return true;
}

bool Memo::cut_selection_to_clipboard() {
    if (context().app == nullptr || !selection_anchor_) return false;
    const std::string selected = selected_text();
    if (selected.empty()) return false;
    record_undo_state();
    context().app->set_clipboard_text(selected);
    erase_selection();
    on_resized();
    ensure_cursor_visible();
    invalidate();
    return true;
}

bool Memo::paste_from_clipboard() {
    if (context().app == nullptr) return false;
    const std::string& clip = context().app->clipboard_text();
    if (clip.empty()) return false;
    return on_text(TextEvent{clip, false});
}

void Memo::move_cursor(MemoPosition target, bool extend_selection) {
    target = clamp_position(target);
    if (extend_selection) {
        if (!selection_anchor_) selection_anchor_ = cursor_;
    } else {
        selection_anchor_.reset();
    }
    cursor_ = target;
    ensure_cursor_visible();
    invalidate();
}

void Memo::erase_selection() {
    if (!selection_anchor_) return;
    const auto [begin, end] = selection_range();
    if (begin.line == end.line) {
        auto& line = lines_[static_cast<std::size_t>(begin.line)];
        line.erase(line.begin() + begin.column, line.begin() + end.column);
    } else {
        std::vector<std::string> tail(lines_[static_cast<std::size_t>(end.line)].begin() + end.column,
                                       lines_[static_cast<std::size_t>(end.line)].end());
        lines_[static_cast<std::size_t>(begin.line)].erase(
            lines_[static_cast<std::size_t>(begin.line)].begin() + begin.column,
            lines_[static_cast<std::size_t>(begin.line)].end());
        for (auto& g : tail) lines_[static_cast<std::size_t>(begin.line)].push_back(std::move(g));
        lines_.erase(lines_.begin() + begin.line + 1, lines_.begin() + end.line + 1);
    }
    cursor_ = begin;
    selection_anchor_.reset();
}

void Memo::insert_text_at_cursor(std::string_view text) {
    if (selection_anchor_) erase_selection();

    std::vector<std::string> segments;
    std::size_t start = 0;
    while (true) {
        const std::size_t nl = text.find('\n', start);
        if (nl == std::string_view::npos) {
            segments.emplace_back(text.substr(start));
            break;
        }
        segments.emplace_back(text.substr(start, nl - start));
        start = nl + 1;
    }

    auto& current = lines_[static_cast<std::size_t>(cursor_.line)];
    std::vector<std::string> tail(current.begin() + cursor_.column, current.end());
    current.erase(current.begin() + cursor_.column, current.end());
    for (std::string_view g : text::split_graphemes(segments[0])) current.push_back(std::string(g));

    int new_line_index = cursor_.line;
    for (std::size_t i = 1; i < segments.size(); ++i) {
        std::vector<std::string> new_line;
        for (std::string_view g : text::split_graphemes(segments[i])) new_line.push_back(std::string(g));
        lines_.insert(lines_.begin() + new_line_index + 1, std::move(new_line));
        ++new_line_index;
    }
    const int new_column = static_cast<int>(lines_[static_cast<std::size_t>(new_line_index)].size());
    for (auto& g : tail) lines_[static_cast<std::size_t>(new_line_index)].push_back(std::move(g));

    cursor_ = {new_line_index, new_column};
    selection_anchor_.reset();
    on_resized();
    ensure_cursor_visible();
    invalidate();
}

std::vector<Memo::VisualRow> Memo::visual_rows(int visible_width) const {
    std::vector<VisualRow> rows;
    if (lines_.empty()) return rows;

    for (int line_index = 0; line_index < static_cast<int>(lines_.size()); ++line_index) {
        const auto& line = lines_[static_cast<std::size_t>(line_index)];
        // One wrap rule for the whole library, so a memo breaks lines exactly
        // where a help page and an editor do.
        for (const WrapSegment& segment :
             wrap_graphemes(line, WrapOptions{visible_width, wrap_mode_, 0}))
            rows.push_back(VisualRow{line_index, static_cast<int>(segment.begin),
                                     static_cast<int>(segment.end)});
    }
    return rows;
}

int Memo::column_x(const VisualRow& row, int column) const {
    const auto& line = lines_[static_cast<std::size_t>(row.line)];
    int x = 0;
    for (int i = row.begin; i < column && static_cast<std::size_t>(i) < line.size(); ++i)
        x += text::grapheme_width(line[static_cast<std::size_t>(i)]);
    return x;
}

int Memo::visual_row_for_position(MemoPosition position, int visible_width) const {
    position = clamp_position(position);
    const std::vector<VisualRow> rows = visual_rows(visible_width);
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const VisualRow& row = rows[static_cast<std::size_t>(i)];
        if (row.line != position.line) continue;
        if (position.column >= row.begin && position.column <= row.end) return i;
    }
    return 0;
}

MemoPosition Memo::position_at_visual_row_column(int visual_row, int local_x, int visible_width) const {
    const std::vector<VisualRow> rows = visual_rows(visible_width);
    if (rows.empty()) return MemoPosition{};
    visual_row = std::clamp(visual_row, 0, static_cast<int>(rows.size()) - 1);
    const VisualRow& row = rows[static_cast<std::size_t>(visual_row)];
    const auto& line = lines_[static_cast<std::size_t>(row.line)];
    int x = 0;
    for (int column = row.begin; column < row.end; ++column) {
        const int grapheme_columns = text::grapheme_width(line[static_cast<std::size_t>(column)]);
        if (local_x < x + grapheme_columns) return MemoPosition{row.line, column};
        x += grapheme_columns;
        if (visible_width > 0 && x >= visible_width) break;
    }
    return MemoPosition{row.line, row.end};
}

void Memo::ensure_cursor_visible() {
    if (scrollbar_ == nullptr || h_scrollbar_ == nullptr) return;
    const std::vector<VisualRow> rows = visual_rows(viewport_width_);
    const int cursor_row = visual_row_for_position(cursor_, viewport_width_);
    if (cursor_row < scrollbar_->position()) {
        scrollbar_->set_position(cursor_row);
    } else if (cursor_row >= scrollbar_->position() + scrollbar_->viewport_size()) {
        scrollbar_->set_position(cursor_row - scrollbar_->viewport_size() + 1);
    }

    // Horizontally too: with wrapping off, a cursor moving along a long line
    // would otherwise walk straight off the edge and out of sight.
    if (cursor_row < 0 || static_cast<std::size_t>(cursor_row) >= rows.size()) return;
    const int cursor_x = column_x(rows[static_cast<std::size_t>(cursor_row)], cursor_.column);
    if (cursor_x < h_scrollbar_->position()) {
        h_scrollbar_->set_position(cursor_x);
    } else if (cursor_x >= h_scrollbar_->position() + std::max(1, viewport_width_)) {
        h_scrollbar_->set_position(cursor_x - std::max(1, viewport_width_) + 1);
    }
}

int Memo::left_column() const noexcept { return h_scrollbar_ != nullptr ? h_scrollbar_->position() : 0; }

void Memo::set_vertical_scrollbar_policy(ScrollbarPolicy policy) {
    if (scrollbar_ != nullptr) scrollbar_->set_policy(policy);
    relayout_scrollbars();
    invalidate();
}

void Memo::set_horizontal_scrollbar_policy(ScrollbarPolicy policy) {
    if (h_scrollbar_ != nullptr) h_scrollbar_->set_policy(policy);
    relayout_scrollbars();
    invalidate();
}

ScrollbarPolicy Memo::vertical_scrollbar_policy() const noexcept {
    return scrollbar_ != nullptr ? scrollbar_->policy() : ScrollbarPolicy::Hidden;
}

ScrollbarPolicy Memo::horizontal_scrollbar_policy() const noexcept {
    return h_scrollbar_ != nullptr ? h_scrollbar_->policy() : ScrollbarPolicy::Hidden;
}

void Memo::relayout_scrollbars() {
    if (scrollbar_ == nullptr || h_scrollbar_ == nullptr) return;

    const ScrollGeometry geometry = resolve_scroll_geometry(
        Size{bounds().width, bounds().height}, scrollbar_->policy(), h_scrollbar_->policy(),
        [this](int viewport_width) {
            const std::vector<VisualRow> rows = visual_rows(viewport_width);
            int widest = 0;
            for (const VisualRow& row : rows) widest = std::max(widest, column_x(row, row.end));
            return Size{widest, static_cast<int>(rows.size())};
        });

    viewport_width_ = geometry.viewport_width;
    viewport_height_ = geometry.viewport_height;
    const std::vector<VisualRow> rows = visual_rows(viewport_width_);
    content_width_ = 0;
    for (const VisualRow& row : rows) content_width_ = std::max(content_width_, column_x(row, row.end));

    const int v_width = geometry.show_vertical ? std::min(1, bounds().width) : 0;
    const int h_height = geometry.show_horizontal ? std::min(1, bounds().height) : 0;
    scrollbar_->set_range(static_cast<int>(rows.size()), std::max(1, geometry.viewport_height));
    h_scrollbar_->set_range(content_width_, std::max(1, geometry.viewport_width));
    scrollbar_->set_bounds(Rect{std::max(0, bounds().width - v_width), 0, v_width,
                                std::max(0, bounds().height - h_height)});
    h_scrollbar_->set_bounds(Rect{0, std::max(0, bounds().height - h_height),
                                  std::max(0, bounds().width - v_width), h_height});
    scrollbar_->set_visible(geometry.show_vertical);
    h_scrollbar_->set_visible(geometry.show_horizontal);
}

void Memo::on_resized() { relayout_scrollbars(); }

bool Memo::on_key(const KeyEvent& event) {
    if (event.action == KeyAction::Release) return false;
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
    // IMEs and bracketed paste arrive as TextEvent. Keep both ingress paths at
    // the editable-control boundary so insertion has one implementation.
    // Alt/Ctrl/Super characters remain chords for command routing; Shift is
    // part of normal text production.
    if (event.chord.key == Key::Char && !has_modifier(event.chord.modifiers, Modifier::Alt) &&
        !has_modifier(event.chord.modifiers, Modifier::Ctrl) &&
        !has_modifier(event.chord.modifiers, Modifier::Super))
        return on_text(TextEvent{event.chord.text, false});

    const bool shift = has_shift;
    const bool control = has_ctrl;
    switch (event.chord.key) {
        case Key::Left: {
            MemoPosition p = cursor_;
            if (control) p = previous_word(p);
            else if (p.column > 0)
                --p.column;
            else if (p.line > 0) {
                --p.line;
                p.column = line_length(p.line);
            }
            move_cursor(p, shift);
            return true;
        }
        case Key::Right: {
            MemoPosition p = cursor_;
            if (control) p = next_word(p);
            else if (p.column < line_length(p.line))
                ++p.column;
            else if (p.line < static_cast<int>(lines_.size()) - 1) {
                ++p.line;
                p.column = 0;
            }
            move_cursor(p, shift);
            return true;
        }
        case Key::Up: {
            MemoPosition p = cursor_;
            if (p.line > 0) {
                --p.line;
                p.column = std::min(p.column, line_length(p.line));
            }
            move_cursor(p, shift);
            return true;
        }
        case Key::Down: {
            MemoPosition p = cursor_;
            if (p.line < static_cast<int>(lines_.size()) - 1) {
                ++p.line;
                p.column = std::min(p.column, line_length(p.line));
            }
            move_cursor(p, shift);
            return true;
        }
        case Key::Home:
            move_cursor(control ? MemoPosition{0, 0} : MemoPosition{cursor_.line, 0}, shift);
            return true;
        case Key::End:
            move_cursor(control ? MemoPosition{static_cast<int>(lines_.size()) - 1,
                                               line_length(static_cast<int>(lines_.size()) - 1)}
                                : MemoPosition{cursor_.line, line_length(cursor_.line)},
                        shift);
            return true;
        case Key::PageUp: {
            MemoPosition p = cursor_;
            p.line = std::max(0, p.line - std::max(1, bounds().height));
            p.column = std::min(p.column, line_length(p.line));
            move_cursor(p, shift);
            return true;
        }
        case Key::PageDown: {
            MemoPosition p = cursor_;
            p.line = std::min(static_cast<int>(lines_.size()) - 1, p.line + std::max(1, bounds().height));
            p.column = std::min(p.column, line_length(p.line));
            move_cursor(p, shift);
            return true;
        }
        case Key::Enter:
            record_undo_state();
            insert_text_at_cursor("\n");
            return true;
        case Key::Backspace:
            if (selection_anchor_) {
                record_undo_state();
                erase_selection();
            } else if (cursor_.column > 0 || cursor_.line > 0) {
                record_undo_state();
                const MemoPosition target = control ? previous_word(cursor_) :
                    (cursor_.column > 0 ? MemoPosition{cursor_.line, cursor_.column - 1}
                                        : MemoPosition{cursor_.line - 1, line_length(cursor_.line - 1)});
                selection_anchor_ = cursor_;
                cursor_ = target;
                erase_selection();
            } else if (cursor_.line > 0) {
                record_undo_state();
                const int prev_len = line_length(cursor_.line - 1);
                for (auto& g : lines_[static_cast<std::size_t>(cursor_.line)])
                    lines_[static_cast<std::size_t>(cursor_.line - 1)].push_back(std::move(g));
                lines_.erase(lines_.begin() + cursor_.line);
                --cursor_.line;
                cursor_.column = prev_len;
            }
            on_resized();
            ensure_cursor_visible();
            invalidate();
            return true;
        case Key::Delete:
            if (selection_anchor_) {
                record_undo_state();
                erase_selection();
            } else if (cursor_.column < line_length(cursor_.line) ||
                       cursor_.line < static_cast<int>(lines_.size()) - 1) {
                record_undo_state();
                const MemoPosition target = control ? next_word(cursor_) :
                    (cursor_.column < line_length(cursor_.line) ? MemoPosition{cursor_.line, cursor_.column + 1}
                                                                 : MemoPosition{cursor_.line + 1, 0});
                selection_anchor_ = cursor_;
                cursor_ = target;
                erase_selection();
            } else if (cursor_.line < static_cast<int>(lines_.size()) - 1) {
                record_undo_state();
                for (auto& g : lines_[static_cast<std::size_t>(cursor_.line + 1)])
                    lines_[static_cast<std::size_t>(cursor_.line)].push_back(std::move(g));
                lines_.erase(lines_.begin() + cursor_.line + 1);
            }
            on_resized();
            invalidate();
            return true;
        default:
            return false;
    }
}

bool Memo::on_text(const TextEvent& event) {
    if (event.text.empty()) return false;
    record_undo_state();
    insert_text_at_cursor(event.text);
    return true;
}

bool Memo::on_mouse(const MouseEvent& event) {
    if (scrollbar_ == nullptr) return false;
    if (event.action == MouseAction::Up && dragging_selection_) {
        dragging_selection_ = false;
        return true;
    }
    const Rect abs = absolute_bounds();
    const int row = event.cell.y - abs.y;
    if (row < 0 || row >= viewport_height_) return false;
    // The click lands in the viewport, so the column it names is offset by
    // however far the view is scrolled sideways.
    const int local_x = std::clamp(event.cell.x - abs.x, 0, std::max(0, viewport_width_ - 1)) + left_column();
    const MemoPosition target =
        position_at_visual_row_column(scrollbar_->position() + row, local_x, viewport_width_);

    if (event.action == MouseAction::Move && dragging_selection_) {
        move_cursor(target, true);
        return true;
    }
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    dragging_selection_ = true;
    move_cursor(target, has_modifier(event.modifiers, Modifier::Shift));
    return true;
}

void Memo::on_focus(const FocusEvent& event) {
    has_focus_ = event.gained;
    invalidate();
}

void Memo::draw(scene::Painter& painter) {
    const Style base = context().theme->resolve(has_focus_ ? focused_role_ : normal_role_);
    const int visible_width = viewport_width_;
    const std::vector<VisualRow> rows = visual_rows(visible_width);
    const int top = scrollbar_ != nullptr ? scrollbar_->position() : 0;
    const int left = left_column();
    const auto [sel_begin, sel_end] = selection_range();

    for (int row = 0; row < viewport_height_; ++row) {
        const int visual_index = top + row;
        painter.fill(Rect{0, row, visible_width, 1}, Cell::from_grapheme(" ", base));
        if (visual_index < 0 || static_cast<std::size_t>(visual_index) >= rows.size()) continue;
        const VisualRow& visual_row = rows[static_cast<std::size_t>(visual_index)];
        const int line_index = visual_row.line;
        const auto& line = lines_[static_cast<std::size_t>(visual_row.line)];
        // Walk the row in its own coordinates and subtract the scroll offset,
        // so a horizontally scrolled row starts part-way through rather than
        // being dropped.
        int cell_x = 0;
        for (int i = visual_row.begin; i < visual_row.end; ++i) {
            const int grapheme_columns = text::grapheme_width(line[static_cast<std::size_t>(i)]);
            const int x = cell_x - left;
            cell_x += grapheme_columns;
            if (x + grapheme_columns <= 0) continue;   // scrolled off to the left
            if (x >= visible_width) break;             // past the right edge
            if (x < 0 || grapheme_columns > visible_width - x) continue;
            Style style = base;
            const MemoPosition p{line_index, i};
            if (selection_anchor_ && !position_less(p, sel_begin) && position_less(p, sel_end))
                style.attrs |= Attr::Reverse;
            painter.draw_text(Point{x, row}, line[static_cast<std::size_t>(i)], style);
        }
        if (has_focus_ && line_index == cursor_.line && cursor_.column >= visual_row.begin &&
            cursor_.column <= visual_row.end) {
            const int cursor_x = column_x(visual_row, cursor_.column) - left;
            if (cursor_x >= 0 && cursor_x < visible_width) {
                Style cursor_style = base;
                cursor_style.attrs |= Attr::Reverse;
                const std::string glyph = static_cast<std::size_t>(cursor_.column) < line.size()
                                               ? line[static_cast<std::size_t>(cursor_.column)]
                                               : std::string(" ");
                painter.draw_text(Point{cursor_x, row}, glyph, cursor_style);
            }
        }
    }
}

}  // namespace ckv::widgets
