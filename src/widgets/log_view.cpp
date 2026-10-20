// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/log_view.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/application.hpp"

namespace ckv::widgets {
namespace {
std::size_t boundary(std::string_view value, std::size_t byte) noexcept {
    byte = std::min(byte, value.size());
    std::size_t previous = 0;
    for (std::size_t at = 0; at < byte;) {
        previous = at;
        at = text::grapheme_end(value, at);
        if (at > byte) return previous;
    }
    return byte;
}
int cells(std::string_view value) noexcept { return text::text_width(value); }
int as_int(std::size_t value) noexcept {
    return static_cast<int>(std::min(value, static_cast<std::size_t>(std::numeric_limits<int>::max() / 4)));
}
}

LogView::LogView(const LogRecordProvider& provider, std::size_t max_viewport_rows)
    : ui::View({}, ui::FocusPolicy::TabStop), provider_(provider),
      text_limit_(provider.max_text_bytes()), row_limit_(max_viewport_rows) {
    if (text_limit_ == 0 || text_limit_ > kLogBatchBytes * 3 ||
        row_limit_ == 0 || row_limit_ > 4096)
        throw std::invalid_argument("invalid log view capacity");
    layout_.reserve(text_limit_ + 1);
    rows_.reserve(row_limit_);
    highlight_bytes_.resize(text_limit_);
    vertical_ = make<Scrollbar>(Orientation::Vertical);
    horizontal_ = make<Scrollbar>(Orientation::Horizontal);
    vertical_->on_position_changed = [this](int value) {
        if (!syncing_bars_) {
            const auto first = first_index();
            if (first) set_top_record(*first + static_cast<std::size_t>(value));
        }
    };
    horizontal_->on_position_changed = [this](int value) {
        if (!syncing_bars_) set_left_column(value);
    };
    seen_sequence_ = provider_.latest_sequence();
}

std::size_t LogView::allocated_bytes() const noexcept {
    return sizeof(*this) + layout_.capacity() * sizeof(WrapSegment) + rows_.capacity() * sizeof(Row) + highlight_bytes_.capacity();
}

void LogView::on_attached() {
    if (!context().roles) return;
    text_role_ = context().roles->find("ckv.textview.text");
    selection_role_ = context().roles->find("ckv.list.selected");
    inactive_role_ = context().roles->find("ckv.list.selected.inactive");
}

void LogView::on_resized() {
    const bool vertical = bounds().width >= 2 && bounds().height >= 1;
    const bool horizontal = bounds().height >= 2 && bounds().width >= 1;
    width_ = std::max(0, bounds().width - (vertical ? 1 : 0));
    height_ = std::max(0, bounds().height - (horizontal ? 1 : 0));
    state_.viewport_exceeded = static_cast<std::size_t>(height_) > row_limit_ || bounds().width > 65535;
    vertical_->set_bounds({width_, 0, vertical ? 1 : 0, height_});
    horizontal_->set_bounds({0, height_, width_, horizontal ? 1 : 0});
    dirty_ = true;
    refresh();
}

bool LogView::eligible(const LogRecord& record) const noexcept {
    return record.id.sequence > hidden_through_;
}

std::optional<std::size_t> LogView::first_index() const noexcept {
    // Providers preserve admission order. A lower-bound search avoids walking
    // the hidden prefix left by clear_view.
    std::size_t first = 0, last = provider_.size();
    while (first < last) {
        const auto middle = first + (last - first) / 2;
        const auto record = provider_.at(middle);
        if (!record) return std::nullopt;
        if (eligible(*record)) last = middle;
        else first = middle + 1;
    }
    return first < provider_.size() ? std::optional{first} : std::nullopt;
}

bool LogView::normalize(LogPosition& position) const noexcept {
    const auto index = provider_.index_of(position.id);
    const auto record = index ? provider_.at(*index) : std::nullopt;
    if (!record || !eligible(*record)) return false;
    position.byte = boundary(record->text, position.byte);
    return true;
}

bool LogView::layout(const LogRecord& record) {
    if (record.text.size() > text_limit_) {
        state_.record_exceeded = true;
        return false;
    }
    if (layout_id_ == record.id && layout_revision_ == record.text_revision &&
        layout_width_ == width_ && layout_mode_ == wrap_) return true;
    layout_.clear();
    // LF is an explicit multiline-record separator, not a display glyph.
    std::size_t line = 0;
    for (;;) {
        const auto newline = record.text.find('\n', line);
        const auto end = newline == std::string_view::npos ? record.text.size() : newline;
        const auto value = record.text.substr(line, end - line);
        std::size_t begin = 0;
        do {
            const auto segment = next_text_row(value, begin, {width_, wrap_, 0});
            layout_.push_back({line + segment.begin, line + segment.end});
            begin = segment.end;
        } while (begin < value.size());
        if (newline == std::string_view::npos) break;
        line = newline + 1;
    }
    layout_id_ = record.id;
    layout_revision_ = record.text_revision;
    layout_width_ = width_;
    layout_mode_ = wrap_;
    return true;
}

std::optional<LogView::Row> LogView::row_at(LogPosition position) {
    const auto index = provider_.index_of(position.id);
    const auto record = index ? provider_.at(*index) : std::nullopt;
    if (!record || !eligible(*record) || !layout(*record)) return std::nullopt;
    auto segment = std::upper_bound(layout_.begin(), layout_.end(), position.byte,
        [](std::size_t byte, const WrapSegment& candidate) { return byte < candidate.begin; });
    if (segment != layout_.begin()) --segment;
    return Row{{position.id, segment->begin}, segment->end};
}

std::optional<LogPosition> LogView::next_row(LogPosition position) {
    const auto row = row_at(position);
    if (!row) return std::nullopt;
    const auto segment = std::lower_bound(layout_.begin(), layout_.end(), row->start.byte,
        [](const WrapSegment& candidate, std::size_t byte) { return candidate.begin < byte; });
    if (segment + 1 != layout_.end()) return LogPosition{position.id, (segment + 1)->begin};
    const auto index = provider_.index_of(position.id);
    const auto record = index ? provider_.at(*index + 1) : std::nullopt;
    return record && eligible(*record) ? std::optional{LogPosition{record->id, 0}} : std::nullopt;
}

std::optional<LogPosition> LogView::previous_row(LogPosition position) {
    const auto row = row_at(position);
    if (!row) return std::nullopt;
    const auto segment = std::lower_bound(layout_.begin(), layout_.end(), row->start.byte,
        [](const WrapSegment& candidate, std::size_t byte) { return candidate.begin < byte; });
    if (segment != layout_.begin()) return LogPosition{position.id, (segment - 1)->begin};
    const auto index = provider_.index_of(position.id);
    if (!index || *index == 0) return std::nullopt;
    const auto record = provider_.at(*index - 1);
    if (!record || !eligible(*record) || !layout(*record)) return std::nullopt;
    return LogPosition{record->id, layout_.back().begin};
}

std::optional<LogPosition> LogView::last_row() {
    if (provider_.size() == 0) return std::nullopt;
    const auto record = provider_.at(provider_.size() - 1);
    if (!record || !eligible(*record) || !layout(*record)) return std::nullopt;
    return LogPosition{record->id, layout_.back().begin};
}

void LogView::reconcile_selection() {
    if (!selection_) return;
    if (!normalize(selection_->anchor) || !normalize(selection_->cursor)) {
        selection_.reset();
        dragging_ = false;
        state_.selection_unavailable = true;
    }
}

void LogView::refresh() {
    const auto sequence = provider_.latest_sequence();
    if (!state_.following && sequence >= seen_sequence_) {
        const auto count = sequence - seen_sequence_;
        state_.unread += std::min(count, std::numeric_limits<std::uint64_t>::max() - state_.unread);
    }
    seen_sequence_ = sequence;
    state_.evicted = provider_.evicted_count();
    if (provider_.revision() != seen_revision_) {
        seen_revision_ = provider_.revision();
        dirty_ = true;
        reconcile_selection();
    }
    if (dirty_) {
        rebuild();
        invalidate();
    }
    publish_state();
}

void LogView::rebuild() {
    dirty_ = false;
    rows_.clear();
    if (state_.viewport_exceeded) {
        vertical_->set_visible(false);
        horizontal_->set_visible(false);
        publish_state();
        return;
    }
    if (state_.following) {
        top_ = last_row();
        for (int row = 1; top_ && row < height_; ++row) {
            const auto previous = previous_row(*top_);
            if (!previous) break;
            top_ = previous;
        }
    } else if (top_ && !normalize(*top_)) {
        top_.reset();
        state_.anchor_unavailable = true;
    }
    if (!top_) {
        const auto first = first_index();
        const auto record = first ? provider_.at(*first) : std::nullopt;
        if (record) top_ = LogPosition{record->id, 0};
    }
    auto cursor = top_;
    for (int y = 0; cursor && y < height_; ++y) {
        const auto row = row_at(*cursor);
        if (!row) break;
        if (y == 0) top_ = row->start;
        rows_.push_back(*row);
        const auto index = provider_.index_of(row->start.id);
        const auto record = index ? provider_.at(*index) : std::nullopt;
        if (record) encountered_width_ = std::max(encountered_width_, cells(record->text.substr(row->start.byte, row->end - row->start.byte)));
        cursor = next_row(row->start);
    }
    syncing_bars_ = true;
    const auto first = first_index();
    const int count = first ? as_int(provider_.size() - *first) : 0;
    int shown_records = 0;
    std::optional<LogRecordId> previous;
    for (const auto& row : rows_) if (previous != row.start.id) {
        ++shown_records; previous = row.start.id;
    }
    vertical_->set_range(count, std::max(1, shown_records));
    const auto index = top_ ? provider_.index_of(top_->id) : std::nullopt;
    vertical_->set_position(index && first ? as_int(*index - *first) : 0);
    vertical_->set_visible(bounds().width >= 2 && bounds().height >= 1);
    horizontal_->set_range(std::max(encountered_width_, left_ + width_), std::max(1, width_));
    horizontal_->set_position(left_);
    horizontal_->set_visible(bounds().height >= 2 && bounds().width >= 1);
    syncing_bars_ = false;
}

void LogView::publish_state() {
    if (state_ == published_state_) return;
    published_state_ = state_;
    if (on_state_changed) on_state_changed(state_);
}

void LogView::pause() {
    refresh();
    state_.following = false;
    publish_state();
}
void LogView::resume() {
    state_.following = true;
    state_.unread = 0;
    seen_sequence_ = provider_.latest_sequence();
    dirty_ = true;
    refresh();
}
void LogView::acknowledge_notices() {
    state_.anchor_unavailable = false;
    state_.selection_unavailable = false;
    state_.record_exceeded = false;
    publish_state();
}
void LogView::set_wrap_mode(WrapMode mode) {
    if (wrap_ == mode) return;
    wrap_ = mode;
    encountered_width_ = 0;
    left_ = 0;
    dirty_ = true;
    refresh();
}
void LogView::set_top_record(std::size_t index) {
    pause();
    const auto first = first_index();
    if (!first) return;
    index = std::clamp(index, *first, provider_.size() - 1);
    const auto record = provider_.at(index);
    if (!record) return;
    top_ = LogPosition{record->id, 0};
    dirty_ = true;
    refresh();
}
void LogView::scroll_rows(int delta) {
    pause();
    // One input event cannot request unbounded work, even programmatically.
    delta = std::clamp(delta, -as_int(row_limit_), as_int(row_limit_));
    while (delta != 0 && top_) {
        const auto next = delta > 0 ? next_row(*top_) : previous_row(*top_);
        if (!next) break;
        top_ = next;
        delta += delta > 0 ? -1 : 1;
    }
    dirty_ = true;
    refresh();
}
void LogView::set_left_column(int column) {
    left_ = std::clamp(column, 0, as_int(text_limit_ * 2));
    dirty_ = true;
    refresh();
}

bool LogView::set_selection(LogSelection selection) {
    if (!normalize(selection.anchor) || !normalize(selection.cursor)) return false;
    selection_ = selection;
    invalidate();
    return true;
}
void LogView::clear_selection() { selection_.reset(); dragging_ = false; invalidate(); }
bool LogView::reveal(LogRecordId id, LogTextRange range) {
    refresh();
    const auto index = provider_.index_of(id);
    const auto record = index ? provider_.at(*index) : std::nullopt;
    if (!record || !eligible(*record) || range.begin >= range.end || range.end > record->text.size()) return false;
    const auto begin = boundary(record->text, range.begin);
    auto end = boundary(record->text, range.end);
    if (end != range.end) end = text::grapheme_end(record->text, end);
    pause();
    selection_ = LogSelection{{id, begin}, {id, end}};
    top_ = LogPosition{id, begin};
    left_ = 0;
    dirty_ = true;
    refresh();
    // Reveal a horizontally clipped occurrence within the anchored row.
    if (!rows_.empty()) {
        const auto& row = rows_.front();
        const auto column = cells(record->text.substr(row.start.byte, begin - row.start.byte));
        if (column >= width_) set_left_column(column);
    }
    return true;
}
void LogView::clear_view() {
    hidden_through_ = provider_.latest_sequence();
    top_.reset();
    clear_selection();
    dirty_ = true;
    resume();
}
void LogView::reset_view() {
    hidden_through_ = 0;
    dirty_ = true;
    refresh();
}

bool LogView::selected(LogRecordId id, std::size_t byte) const noexcept {
    if (!selection_) return false;
    auto begin = selection_->anchor, end = selection_->cursor;
    if (begin.id.sequence > end.id.sequence || (begin.id == end.id && begin.byte > end.byte)) std::swap(begin, end);
    return id.sequence >= begin.id.sequence && id.sequence <= end.id.sequence &&
        (id != begin.id || byte >= begin.byte) && (id != end.id || byte < end.byte);
}

LogCopy LogView::selected_text(std::size_t max_bytes) const {
    LogCopy result;
    if (!selection_) return result;
    auto begin = selection_->anchor, end = selection_->cursor;
    if (!normalize(begin) || !normalize(end)) return result;
    if (begin.id.sequence > end.id.sequence || (begin.id == end.id && begin.byte > end.byte)) std::swap(begin, end);
    const auto first = provider_.index_of(begin.id), last = provider_.index_of(end.id);
    if (!first || !last) return result;
    max_bytes = std::min(max_bytes, std::size_t{1024 * 1024});
    result.state = LogCopyState::Complete;
    for (auto index = *first; index <= *last; ++index) {
        const auto record = provider_.at(index);
        if (!record) { result.state = LogCopyState::Unavailable; result.text.clear(); return result; }
        if (index != *first) {
            if (result.text.size() == max_bytes) { result.state = LogCopyState::Truncated; return result; }
            result.text.push_back('\n');
        }
        const auto start = index == *first ? begin.byte : 0;
        const auto stop = index == *last ? end.byte : record->text.size();
        const auto value = record->text.substr(start, stop - start);
        const auto available = max_bytes - result.text.size();
        const auto take = boundary(value, std::min(value.size(), available));
        result.text.append(value.substr(0, take));
        if (take != value.size()) { result.state = LogCopyState::Truncated; return result; }
    }
    return result;
}
bool LogView::copy_selection() {
    auto copy = selected_text();
    if (copy.state != LogCopyState::Unavailable && context().app) context().app->set_clipboard_text(std::move(copy.text));
    if (on_copy_result) on_copy_result(copy.state);
    return copy.state != LogCopyState::Unavailable;
}

std::optional<LogPosition> LogView::row_column(const Row& row, int column) const {
    const auto index = provider_.index_of(row.start.id);
    const auto record = index ? provider_.at(*index) : std::nullopt;
    if (!record) return std::nullopt;
    int x = 0;
    auto byte = row.start.byte;
    while (byte < row.end) {
        const auto next = text::grapheme_end(record->text, byte);
        const auto width = std::max(1, text::grapheme_width(record->text.substr(byte, next - byte)));
        if (column < x + width) break;
        x += width;
        byte = next;
    }
    return LogPosition{row.start.id, byte};
}
std::optional<LogPosition> LogView::hit(Point local) const {
    if (rows_.empty()) return std::nullopt;
    const auto index = static_cast<std::size_t>(std::clamp(local.y, 0, as_int(rows_.size()) - 1));
    return row_column(rows_[index], std::max(0, local.x + left_));
}

bool LogView::extend_selection(Key key) {
    if (!selection_) {
        if (rows_.empty()) return false;
        selection_ = LogSelection{rows_.front().start, rows_.front().start};
    }
    auto cursor = selection_->cursor;
    if (!normalize(cursor)) return false;
    const auto index = provider_.index_of(cursor.id);
    const auto record = provider_.at(*index);
    const auto current_row = row_at(cursor);
    if (!record || !current_row) return false;
    if (key == Key::Home) cursor.byte = 0;
    else if (key == Key::End) cursor.byte = record->text.size();
    else if (key == Key::Right) {
        if (cursor.byte < record->text.size()) cursor.byte = text::grapheme_end(record->text, cursor.byte);
        else if (const auto next = provider_.at(*index + 1)) cursor = {next->id, 0};
    } else if (key == Key::Left) {
        if (cursor.byte != 0) cursor.byte = boundary(record->text, cursor.byte - 1);
        else if (*index != 0) {
            const auto previous = provider_.at(*index - 1);
            if (previous && eligible(*previous)) cursor = {previous->id, previous->text.size()};
        }
    } else {
        const int column = cells(record->text.substr(current_row->start.byte, cursor.byte - current_row->start.byte));
        const auto target = key == Key::Up ? previous_row(current_row->start) : next_row(current_row->start);
        const auto row = target ? row_at(*target) : std::nullopt;
        if (row) cursor = *row_column(*row, column);
    }
    selection_->cursor = cursor;
    pause();
    bool visible = false;
    for (const auto& row : rows_) if (row.start.id == cursor.id && cursor.byte >= row.start.byte && cursor.byte <= row.end) visible = true;
    if (!visible) { top_ = cursor; dirty_ = true; refresh(); }
    invalidate();
    return true;
}

bool LogView::on_key(const KeyEvent& event) {
    refresh();
    const auto& chord = event.chord;
    if (has_modifier(chord.modifiers, Modifier::Alt) || has_modifier(chord.modifiers, Modifier::Super)) return false;
    if (has_modifier(chord.modifiers, Modifier::Ctrl)) {
        if (chord.key == Key::Char && (chord.text == "c" || chord.text == "C")) return copy_selection();
        if (chord.key == Key::Char && (chord.text == "a" || chord.text == "A")) {
            const auto first = first_index();
            const auto start = first ? provider_.at(*first) : std::nullopt;
            const auto end = provider_.size() ? provider_.at(provider_.size() - 1) : std::nullopt;
            return start && end && set_selection({{start->id, 0}, {end->id, end->text.size()}});
        }
        return false;
    }
    if (has_modifier(chord.modifiers, Modifier::Shift) &&
        (chord.key == Key::Up || chord.key == Key::Down || chord.key == Key::Left || chord.key == Key::Right ||
         chord.key == Key::Home || chord.key == Key::End)) return extend_selection(chord.key);
    switch (chord.key) {
        case Key::Up: scroll_rows(-1); break;
        case Key::Down: scroll_rows(1); break;
        case Key::PageUp: scroll_rows(-std::max(1, height_)); break;
        case Key::PageDown: scroll_rows(std::max(1, height_)); break;
        case Key::Home: set_top_record(0); break;
        case Key::End: resume(); break;
        case Key::Left: set_left_column(left_ - 1); break;
        case Key::Right: set_left_column(left_ + 1); break;
        case Key::Escape: if (!selection_) return false; clear_selection(); break;
        default: return false;
    }
    return true;
}

bool LogView::on_mouse(const MouseEvent& event) {
    refresh();
    if (event.action == MouseAction::Wheel) {
        switch (event.button) {
            case MouseButton::WheelUp: scroll_rows(-3); return true;
            case MouseButton::WheelDown: scroll_rows(3); return true;
            case MouseButton::WheelLeft: set_left_column(left_ - 3); return true;
            case MouseButton::WheelRight: set_left_column(left_ + 3); return true;
            default: return false;
        }
    }
    const auto absolute = absolute_bounds();
    const Point local{event.cell.x - absolute.x, event.cell.y - absolute.y};
    if (event.action == MouseAction::Down && event.button == MouseButton::Left &&
        local.x >= 0 && local.x < width_ && local.y >= 0 && local.y < height_) {
        const auto position = hit(local);
        if (!position) return false;
        pause();
        if (has_modifier(event.modifiers, Modifier::Shift) && selection_) selection_->cursor = *position;
        else selection_ = LogSelection{*position, *position};
        dragging_ = true;
        invalidate();
        return true;
    }
    if (dragging_ && (event.action == MouseAction::Move || event.action == MouseAction::Up)) {
        if (local.y < 0) scroll_rows(-1);
        else if (local.y >= height_) scroll_rows(1);
        if (const auto position = hit(local); position && selection_) selection_->cursor = *position;
        if (event.action == MouseAction::Up) dragging_ = false;
        invalidate();
        return true;
    }
    return false;
}

void LogView::draw(scene::Painter& painter) {
    refresh();
    const auto style = context().theme ? context().theme->resolve(text_role_) : Style{};
    painter.fill({0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", style));
    auto content = painter.clipped({0, 0, width_, height_});
    if (state_.viewport_exceeded || state_.record_exceeded) {
        content.draw_text({0, 0}, "Log view capacity exceeded", style);
        return;
    }
    for (std::size_t y = 0; y < rows_.size(); ++y) {
        const auto& row = rows_[y];
        const auto index = provider_.index_of(row.start.id);
        const auto record = index ? provider_.at(*index) : std::nullopt;
        if (!record) continue;
        auto base = style;
        base.attrs |= record->attrs;
        int x = -left_;
        auto begin = row.start.byte;
        while (begin < row.end) {
            const auto end = text::grapheme_end(record->text, begin);
            const auto width = std::max(1, text::grapheme_width(record->text.substr(begin, end - begin)));
            if (x + width > 0) break;
            x += width; begin = end;
        }
        auto visible_end = begin;
        int measured_x = x;
        while (visible_end < row.end && measured_x < width_) {
            const auto end = text::grapheme_end(record->text, visible_end);
            measured_x += std::max(1, text::grapheme_width(record->text.substr(visible_end, end - visible_end)));
            visible_end = end;
        }
        auto marks = std::span<unsigned char>{highlight_bytes_.data(), visible_end - begin};
        if (highlighter_ && !marks.empty()) highlighter_->mark(*record, begin, visible_end, marks);
        for (auto byte = begin; byte < visible_end && x < width_;) {
            const auto end = text::grapheme_end(record->text, byte);
            const auto cluster = record->text.substr(byte, end - byte);
            auto rendition = base;
            if (highlighter_ && std::any_of(marks.begin() + static_cast<std::ptrdiff_t>(byte - begin),
                                          marks.begin() + static_cast<std::ptrdiff_t>(end - begin),
                                          [](unsigned char marked) { return marked != 0; }))
                rendition.attrs |= Attr::Bold | Attr::Underline;
            if (selected(record->id, byte) && context().theme) {
                rendition = context().theme->resolve(has_focus() ? selection_role_ : inactive_role_);
                rendition.attrs |= record->attrs;
                // An inactive selection can share the text role's colors.
                // Keep selected bytes visible while the filter owns focus.
                if (rendition.fg == base.fg && rendition.bg == base.bg) rendition.attrs |= Attr::Reverse;
            }
            content.draw_text({x, static_cast<int>(y)}, cluster, rendition);
            x += std::max(1, text::grapheme_width(cluster));
            byte = end;
        }
        if (row.end < record->text.size() && record->text[row.end] == '\n' && x >= 0 && x < width_) {
            auto rendition = base;
            if (highlighter_) {
                std::array<unsigned char, 1> newline{};
                highlighter_->mark(*record, row.end, row.end + 1, newline);
                if (newline[0]) rendition.attrs |= Attr::Bold | Attr::Underline;
            }
            if (selected(record->id, row.end)) rendition.attrs |= Attr::Reverse;
            content.draw_text({x, static_cast<int>(y)}, " ", rendition);
        }
    }
}

}  // namespace ckv::widgets
