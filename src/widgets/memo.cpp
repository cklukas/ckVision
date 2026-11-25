// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/memo.hpp"

#include "cvision/core/ascii.hpp"
#include "cvision/widgets/text_layout.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "widgets/frame_scrollbar_internal.hpp"

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
    scrollbar_liveness_ = scrollbar_->lifetime_token();
    scrollbar_->bind_subject(this);
    scrollbar_->set_policy(ScrollbarPolicy::Auto);
    scrollbar_->on_change = [this](int p) { set_top_row(p); };
    h_scrollbar_ = make<Scrollbar>(Orientation::Horizontal);
    h_scrollbar_liveness_ = h_scrollbar_->lifetime_token();
    h_scrollbar_->bind_subject(this);
    h_scrollbar_->set_policy(ScrollbarPolicy::Auto);
    h_scrollbar_->on_change = [this](int p) { set_left_column(p); };
    set_focus_policy(ui::FocusPolicy::TabStop);
    lines_.emplace_back();
}

Memo::~Memo() {
    detail::sever_scrollbar(*this, vertical_bar());
    detail::sever_scrollbar(*this, horizontal_bar());
}

Scrollbar* Memo::vertical_bar() const noexcept {
    return scrollbar_ != nullptr && !scrollbar_liveness_.expired() ? scrollbar_ : nullptr;
}

Scrollbar* Memo::horizontal_bar() const noexcept {
    return h_scrollbar_ != nullptr && !h_scrollbar_liveness_.expired() ? h_scrollbar_ : nullptr;
}

bool Memo::vertical_bar_embedded() const noexcept {
    Scrollbar* const bar = vertical_bar();
    return bar != nullptr && bar->parent() == this;
}

bool Memo::horizontal_bar_embedded() const noexcept {
    Scrollbar* const bar = horizontal_bar();
    return bar != nullptr && bar->parent() == this;
}

std::unique_ptr<Scrollbar> Memo::take_vertical_scrollbar() {
    if (!vertical_bar_embedded()) return nullptr;
    v_policy_ = scrollbar_->policy();
    std::unique_ptr<ui::View> detached = remove_child(scrollbar_);
    std::unique_ptr<Scrollbar> owned(static_cast<Scrollbar*>(detached.release()));
    relayout_scrollbars();
    ensure_cursor_visible();
    invalidate();
    return owned;
}

std::unique_ptr<Scrollbar> Memo::take_horizontal_scrollbar() {
    if (!horizontal_bar_embedded()) return nullptr;
    h_policy_ = h_scrollbar_->policy();
    std::unique_ptr<ui::View> detached = remove_child(h_scrollbar_);
    std::unique_ptr<Scrollbar> owned(static_cast<Scrollbar*>(detached.release()));
    relayout_scrollbars();
    ensure_cursor_visible();
    invalidate();
    return owned;
}

void Memo::on_detaching() {
    const bool v_detached = detail::detach_orphaned_scrollbar(*this, scrollbar_, scrollbar_liveness_);
    const bool h_detached = detail::detach_orphaned_scrollbar(*this, h_scrollbar_, h_scrollbar_liveness_);
    if (v_detached || h_detached) relayout_scrollbars();
}

void Memo::on_effective_visibility_changed() {
    if (Scrollbar* v = vertical_bar())
        v->set_visible(show_vertical_ && detail::subject_visible_for_bar(*this, *v));
    if (Scrollbar* h = horizontal_bar())
        h->set_visible(show_horizontal_ && detail::subject_visible_for_bar(*this, *h));
}

void Memo::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.memo.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.memo.focused");
    if (invalid_role_ == ui::kInvalidRole) invalid_role_ = context().roles->find("ckv.memo.invalid");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.memo.disabled");
    if (selection_role_ == ui::kInvalidRole) selection_role_ = context().roles->find("ckv.input.selection");
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
    set_valid(true);
    on_resized();
    ensure_cursor_visible();
    invalidate();
}

void Memo::set_valid(bool valid) noexcept {
    if (valid == valid_) return;
    valid_ = valid;
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
    set_valid(true);
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
    set_valid(true);
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

void Memo::set_top_row(int row) {
    const int max_top = std::max(0, visual_row_count_ - std::max(1, viewport_height_));
    const int clamped = std::clamp(row, 0, max_top);
    const bool changed = clamped != top_row_;
    top_row_ = clamped;
    if (Scrollbar* v = vertical_bar()) v->set_position(top_row_);
    if (changed) invalidate();
}

void Memo::set_left_column(int column) {
    const int max_left = std::max(0, content_width_ - std::max(1, viewport_width_));
    const int clamped = std::clamp(column, 0, max_left);
    const bool changed = clamped != left_column_;
    left_column_ = clamped;
    if (Scrollbar* h = horizontal_bar()) h->set_position(left_column_);
    if (changed) invalidate();
}

void Memo::ensure_cursor_visible() {
    const std::vector<VisualRow> rows = visual_rows(viewport_width_);
    visual_row_count_ = std::max(1, static_cast<int>(rows.size()));
    const int cursor_row = visual_row_for_position(cursor_, viewport_width_);
    const int vp_h = std::max(1, viewport_height_);
    if (cursor_row < top_row_) {
        set_top_row(cursor_row);
    } else if (cursor_row >= top_row_ + vp_h) {
        set_top_row(cursor_row - vp_h + 1);
    }

    // Horizontally too: with wrapping off, a cursor moving along a long line
    // would otherwise walk straight off the edge and out of sight.
    if (cursor_row < 0 || static_cast<std::size_t>(cursor_row) >= rows.size()) return;
    const int cursor_x = column_x(rows[static_cast<std::size_t>(cursor_row)], cursor_.column);
    const int vp_w = std::max(1, viewport_width_);
    if (cursor_x < left_column_) {
        set_left_column(cursor_x);
    } else if (cursor_x >= left_column_ + vp_w) {
        set_left_column(cursor_x - vp_w + 1);
    }
}

int Memo::left_column() const noexcept { return left_column_; }

void Memo::set_vertical_scrollbar_policy(ScrollbarPolicy policy) {
    v_policy_ = policy;
    if (Scrollbar* v = vertical_bar()) v->set_policy(policy);
    relayout_scrollbars();
    invalidate();
}

void Memo::set_horizontal_scrollbar_policy(ScrollbarPolicy policy) {
    h_policy_ = policy;
    if (Scrollbar* h = horizontal_bar()) h->set_policy(policy);
    relayout_scrollbars();
    invalidate();
}

ScrollbarPolicy Memo::vertical_scrollbar_policy() const noexcept {
    Scrollbar* const v = vertical_bar();
    return v != nullptr ? v->policy() : v_policy_;
}

ScrollbarPolicy Memo::horizontal_scrollbar_policy() const noexcept {
    Scrollbar* const h = horizontal_bar();
    return h != nullptr ? h->policy() : h_policy_;
}

void Memo::relayout_scrollbars() {
    Scrollbar* const v = vertical_bar();
    Scrollbar* const h = horizontal_bar();
    const bool v_embedded = vertical_bar_embedded();
    const bool h_embedded = horizontal_bar_embedded();
    v_policy_ = vertical_scrollbar_policy();
    h_policy_ = horizontal_scrollbar_policy();

    const ScrollGeometry geometry = resolve_scroll_geometry(
        Size{bounds().width, bounds().height},
        v_policy_,
        h_policy_,
        [this](int viewport_width) {
            const std::vector<VisualRow> rows = visual_rows(viewport_width);
            int widest = 0;
            for (const VisualRow& row : rows) widest = std::max(widest, column_x(row, row.end));
            return Size{widest, static_cast<int>(rows.size())};
        },
        v_embedded, h_embedded);

    viewport_width_ = geometry.viewport_width;
    viewport_height_ = geometry.viewport_height;
    show_vertical_ = geometry.show_vertical;
    show_horizontal_ = geometry.show_horizontal;
    const std::vector<VisualRow> rows = visual_rows(viewport_width_);
    visual_row_count_ = std::max(1, static_cast<int>(rows.size()));
    content_width_ = 0;
    for (const VisualRow& row : rows) content_width_ = std::max(content_width_, column_x(row, row.end));

    top_row_ = std::clamp(top_row_, 0, std::max(0, visual_row_count_ - std::max(1, viewport_height_)));
    left_column_ = std::clamp(left_column_, 0, std::max(0, content_width_ - std::max(1, viewport_width_)));

    const int v_width = (v_embedded && geometry.show_vertical) ? std::min(1, bounds().width) : 0;
    const int h_height = (h_embedded && geometry.show_horizontal) ? std::min(1, bounds().height) : 0;
    if (v != nullptr) {
        v->set_range(static_cast<int>(rows.size()), std::max(1, geometry.viewport_height));
        v->set_position(top_row_);
        if (v_embedded)
            v->set_bounds(Rect{std::max(0, bounds().width - v_width), 0, v_width,
                               std::max(0, bounds().height - h_height)});
        v->set_visible(geometry.show_vertical && detail::subject_visible_for_bar(*this, *v));
    }
    if (h != nullptr) {
        h->set_range(content_width_, std::max(1, geometry.viewport_width));
        h->set_position(left_column_);
        if (h_embedded)
            h->set_bounds(Rect{0, std::max(0, bounds().height - h_height),
                               std::max(0, bounds().width - v_width), h_height});
        h->set_visible(geometry.show_horizontal && detail::subject_visible_for_bar(*this, *h));
    }
}

void Memo::on_resized() { relayout_scrollbars(); }

bool Memo::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || event.action == KeyAction::Release) return false;
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
        const char chord = ascii_lower(event.chord.text[0]);
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
            // An anchor spanning nothing is no selection: dropped, so the
            // key removes what it would without one, and nothing is recorded
            // for an edit that changes nothing.
            if (selection_anchor_ && *selection_anchor_ == cursor_) selection_anchor_.reset();
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
            }
            on_resized();
            ensure_cursor_visible();
            invalidate();
            return true;
        case Key::Delete:
            if (selection_anchor_ && *selection_anchor_ == cursor_) selection_anchor_.reset();
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
    if (!enabled_in_tree()) return false;
    // The wheel scrolls the visual rows and leaves the cursor where it is.
    if (const int rows = ui::wheel_scroll_rows(event); rows != 0) {
        set_top_row(top_row_ + rows);
        invalidate();
        return true;
    }
    if (const int cols = ui::wheel_scroll_cols(event); cols != 0) {
        const int max_left = std::max(0, content_width_ - std::max(1, viewport_width_));
        if (max_left > 0) {
            set_left_column(left_column_ + cols);
            invalidate();
            return true;
        }
    }
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
        position_at_visual_row_column(top_row_ + row, local_x, viewport_width_);

    if (event.action == MouseAction::Move && dragging_selection_) {
        move_cursor(target, true);
        return true;
    }
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    // The second press of a double click (MouseEvent::click_count) selects
    // the word under it and starts no drag.
    if (event.click_count == 2) {
        dragging_selection_ = false;
        select_word_at(target);
        return true;
    }
    dragging_selection_ = true;
    move_cursor(target, has_modifier(event.modifiers, Modifier::Shift));
    return true;
}

void Memo::select_word_at(MemoPosition at) {
    const MemoPosition position = clamp_position(at);
    const auto& line = lines_[static_cast<std::size_t>(position.line)];
    const int length = line_length(position.line);
    // Past the line's end there is nothing to select: the caret goes there.
    if (position.column >= length) {
        move_cursor(position, false);
        return;
    }
    // A word is the run of word graphemes around the one pressed on, the
    // same run Ctrl+Left and Ctrl+Right step over; any other grapheme is
    // selected alone.
    int begin = position.column;
    int end = position.column + 1;
    if (word_grapheme(line[static_cast<std::size_t>(begin)])) {
        while (begin > 0 && word_grapheme(line[static_cast<std::size_t>(begin - 1)])) --begin;
        while (end < length && word_grapheme(line[static_cast<std::size_t>(end)])) ++end;
    }
    move_cursor(MemoPosition{position.line, begin}, false);
    move_cursor(MemoPosition{position.line, end}, true);
}

void Memo::on_focus(const FocusEvent&) { invalidate(); }

void Memo::draw(scene::Painter& painter) {
    const bool enabled = enabled_in_tree();
    const Style base = context().theme->resolve(!enabled    ? disabled_role_
                                                : !valid_   ? invalid_role_
                                                : has_focus() ? focused_role_
                                                              : normal_role_);
    const int visible_width = viewport_width_;
    const std::vector<VisualRow> rows = visual_rows(visible_width);
    const int top = top_row_;
    const int left = left_column();
    const auto [sel_begin, sel_end] = selection_range();
    // Clipped to the viewport, so a glyph the left edge cuts in half shows as
    // a blank in its own style (a selected one stays selected) and the right
    // edge never writes into the bar beside it.
    scene::Painter viewport = painter.clipped(Rect{0, 0, visible_width, viewport_height_});
    painter.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", base));

    const Style selection_style =
        selection_role_ != ui::kInvalidRole ? context().theme->resolve(selection_role_)
                                            : Style{base.bg, base.fg, base.attrs};

    for (int row = 0; row < viewport_height_; ++row) {
        const int visual_index = top + row;
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
            const MemoPosition p{line_index, i};
            const Style style = (enabled && selection_anchor_ && !position_less(p, sel_begin) &&
                                 position_less(p, sel_end))
                                    ? selection_style
                                    : base;
            viewport.draw_text(Point{x, row}, line[static_cast<std::size_t>(i)], style);
        }
        if (enabled && has_focus() && line_index == cursor_.line && cursor_.column >= visual_row.begin &&
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

std::optional<CursorState> Memo::cursor_state() const {
    if (!enabled_in_tree() || !has_focus() || viewport_width_ <= 0 || viewport_height_ <= 0)
        return std::nullopt;
    const std::vector<VisualRow> rows = visual_rows(viewport_width_);
    const int visual_row = visual_row_for_position(cursor_, viewport_width_);
    const int top = top_row_;
    const int local_y = visual_row - top;
    if (local_y < 0 || local_y >= viewport_height_) return std::nullopt;
    if (visual_row < 0 || static_cast<std::size_t>(visual_row) >= rows.size()) return std::nullopt;
    const int local_x = column_x(rows[static_cast<std::size_t>(visual_row)], cursor_.column) - left_column();
    if (local_x < 0 || local_x >= viewport_width_) return std::nullopt;
    const Rect abs = absolute_bounds();
    return CursorState{true, Point{abs.x + local_x, abs.y + local_y}, CursorShape::Bar};
}

}  // namespace ckv::widgets
