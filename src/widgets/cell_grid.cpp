// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/cell_grid.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"

namespace ckv::widgets {

namespace {

// Relative luminance on the 0..255 scale, from the encoded channels with the
// usual perceptual weights and no linearisation: the question is "do these
// two read together", not "what is the exact ratio", and an integer answer
// is the same on every platform.
int luminance(const Color& color) noexcept {
    return (2126 * color.r() + 7152 * color.g() + 722 * color.b()) / 10000;
}

// Whether text in `foreground` on `background` reads: the two differ, and
// the contrast — the lighter luminance over the darker, each lifted by five
// percent of the scale — reaches 3:1. A colour the grid cannot measure (an
// indexed one, or the terminal's default) is taken as readable: it says
// something the terminal knows and the grid does not.
bool readable(const Color& foreground, const Color& background) noexcept {
    if (foreground == background) return false;
    if (!foreground.is_rgb() || !background.is_rgb()) return true;
    const int lighter = std::max(luminance(foreground), luminance(background));
    const int darker = std::min(luminance(foreground), luminance(background));
    return lighter * 100 + 1275 >= 3 * (darker * 100 + 1275);
}

int text_start(int x, int width, int text_width, CellAlignment alignment) noexcept {
    switch (alignment) {
        case CellAlignment::Start:
            return x;
        case CellAlignment::End:
            return x + std::max(0, width - text_width);
        case CellAlignment::Center:
            return x + std::max(0, width - text_width) / 2;
    }
    return x;
}

}  // namespace

std::string spreadsheet_column_label(GridIndex column) {
    std::string label;
    GridIndex value = column;
    for (;;) {
        label.insert(label.begin(), static_cast<char>('A' + static_cast<int>(value % 26)));
        if (value < 26) break;
        value = value / 26 - 1;
    }
    return label;
}

// --- MaterializedCellGridModel ---------------------------------------------

void MaterializedCellGridModel::set_cells(std::vector<std::vector<std::string>> rows) {
    rows_.clear();
    column_count_ = 0;
    for (const auto& row : rows) column_count_ = std::max(column_count_, row.size());
    rows_.reserve(rows.size());
    for (auto& row : rows) {
        std::vector<GridCell> cells(column_count_);
        for (std::size_t index = 0; index < row.size(); ++index) cells[index].text = std::move(row[index]);
        rows_.push_back(std::move(cells));
    }
    cursor_ = clamped(cursor_);
    anchor_.reset();
    clamp_origin();
    scroll_to_cursor();
}

void MaterializedCellGridModel::set_cell(GridPosition at, GridCell cell) {
    CKV_ASSERT(at.row < rows_.size() && at.column < column_count_);
    rows_[at.row][at.column] = std::move(cell);
}

const GridCell& MaterializedCellGridModel::cell_at(GridPosition at) const {
    CKV_ASSERT(at.row < rows_.size() && at.column < column_count_);
    return rows_[at.row][at.column];
}

void MaterializedCellGridModel::set_uniform_column_width(int width) {
    uniform_column_width_ = std::max(1, width);
    scroll_to_cursor();
}

void MaterializedCellGridModel::set_column_widths(std::vector<int> widths) {
    column_widths_ = std::move(widths);
    for (int& width : column_widths_) width = std::max(1, width);
    scroll_to_cursor();
}

void MaterializedCellGridModel::set_row_labels(std::vector<std::string> labels) { row_labels_ = std::move(labels); }

void MaterializedCellGridModel::set_column_labels(std::vector<std::string> labels) {
    column_labels_ = std::move(labels);
}

void MaterializedCellGridModel::set_frozen(std::size_t rows, std::size_t columns) {
    frozen_rows_ = rows;
    frozen_columns_ = columns;
    clamp_origin();
    scroll_to_cursor();
}

void MaterializedCellGridModel::set_spans(std::vector<GridRange> spans) { spans_ = std::move(spans); }

void MaterializedCellGridModel::set_viewport(int rows, int width) {
    viewport_rows_ = std::max(0, rows);
    viewport_width_ = std::max(0, width);
    // The origin is kept valid, and nothing more: a reader who scrolled
    // away from the cursor must not be brought back merely because the grid
    // restated its size. The next cursor movement brings the body back.
    clamp_origin();
}

std::size_t MaterializedCellGridModel::frozen_row_band() const noexcept {
    // The band never takes the whole viewport: at least one row is left for
    // the body, or nothing could ever scroll into view.
    const std::size_t at_most = viewport_rows_ > 1 ? static_cast<std::size_t>(viewport_rows_ - 1) : 0;
    return std::min({frozen_rows_, rows_.size(), at_most});
}

std::size_t MaterializedCellGridModel::frozen_column_band() const noexcept {
    return std::min(frozen_columns_, column_count_ > 0 ? column_count_ - 1 : 0);
}

std::size_t MaterializedCellGridModel::body_rows() const noexcept {
    const std::size_t band = frozen_row_band();
    const std::size_t rows = static_cast<std::size_t>(viewport_rows_);
    return rows > band ? rows - band : 0;
}

std::size_t MaterializedCellGridModel::fitted_columns(GridIndex from) const noexcept {
    int available = viewport_width_;
    for (GridIndex column = 0; column < frozen_column_band(); ++column) available -= column_width(column);
    std::size_t count = 0;
    for (GridIndex column = from; column < column_count_; ++column) {
        available -= column_width(column);
        if (available < 0 && count > 0) break;
        ++count;
        if (available <= 0) break;
    }
    return std::max<std::size_t>(count, 1);
}

GridFrame MaterializedCellGridModel::frame() const {
    GridFrame frame;
    if (rows_.empty() || column_count_ == 0) return frame;
    const std::size_t frozen_rows = frozen_row_band();
    const std::size_t frozen_columns = frozen_column_band();
    for (GridIndex row = 0; row < frozen_rows; ++row) frame.rows.push_back(row);
    for (std::size_t taken = 0; taken < body_rows() && origin_.row + taken < rows_.size(); ++taken)
        frame.rows.push_back(origin_.row + taken);
    for (GridIndex column = 0; column < frozen_columns; ++column) frame.columns.push_back(column);
    const std::size_t fitted = fitted_columns(origin_.column);
    for (std::size_t taken = 0; taken < fitted && origin_.column + taken < column_count_; ++taken)
        frame.columns.push_back(origin_.column + taken);
    frame.frozen_rows = frozen_rows;
    frame.frozen_columns = frozen_columns;
    if (frame.rows.empty() || frame.columns.empty()) return frame;
    const GridRange window{{frame.rows.front(), frame.columns.front()},
                           {frame.rows.back(), frame.columns.back()}};
    for (const GridRange& span : spans_)
        if (span.first.row <= window.last.row && span.last.row >= window.first.row &&
            span.first.column <= window.last.column && span.last.column >= window.first.column)
            frame.spans.push_back(span);
    return frame;
}

int MaterializedCellGridModel::column_width(GridIndex column) const {
    if (column < column_widths_.size()) return column_widths_[column];
    return uniform_column_width_;
}

int MaterializedCellGridModel::row_header_width() const {
    const std::size_t rows = std::max<std::size_t>(rows_.size(), 1);
    return static_cast<int>(std::to_string(rows).size()) + 1;
}

std::string MaterializedCellGridModel::row_label(GridIndex row) const {
    if (row < row_labels_.size()) return row_labels_[row];
    return std::to_string(row + 1);
}

std::string MaterializedCellGridModel::column_label(GridIndex column) const {
    if (column < column_labels_.size()) return column_labels_[column];
    return spreadsheet_column_label(column);
}

GridCell MaterializedCellGridModel::cell(GridIndex row, GridIndex column) const {
    if (row >= rows_.size() || column >= column_count_) return {};
    return rows_[row][column];
}

std::optional<GridRange> MaterializedCellGridModel::selection() const {
    if (!anchor_) return std::nullopt;
    return GridRange{{std::min(anchor_->row, cursor_.row), std::min(anchor_->column, cursor_.column)},
                     {std::max(anchor_->row, cursor_.row), std::max(anchor_->column, cursor_.column)}};
}

void MaterializedCellGridModel::navigate(GridMove move, bool extend) {
    if (rows_.empty() || column_count_ == 0) return;
    begin_extend(extend);
    const GridIndex last_row = rows_.size() - 1;
    const GridIndex last_column = column_count_ - 1;
    const std::size_t page = std::max<std::size_t>(body_rows(), 1);
    GridPosition at = cursor_;
    switch (move) {
        case GridMove::Up:
            at.row = at.row > 0 ? at.row - 1 : 0;
            break;
        case GridMove::Down:
            at.row = std::min(at.row + 1, last_row);
            break;
        case GridMove::Left:
            at.column = at.column > 0 ? at.column - 1 : 0;
            break;
        case GridMove::Right:
            at.column = std::min(at.column + 1, last_column);
            break;
        case GridMove::PageUp:
            at.row = at.row > page ? at.row - page : 0;
            break;
        case GridMove::PageDown:
            at.row = std::min(at.row + page, last_row);
            break;
        case GridMove::JumpUp:
            at.row = 0;
            break;
        case GridMove::JumpDown:
            at.row = last_row;
            break;
        case GridMove::JumpLeft:
            at.column = 0;
            break;
        case GridMove::JumpRight:
            at.column = last_column;
            break;
        case GridMove::RowStart:
            at.column = 0;
            break;
        case GridMove::RowEnd:
            at.column = last_column;
            break;
        case GridMove::GridStart:
            at = {0, 0};
            break;
        case GridMove::GridEnd:
            at = {last_row, last_column};
            break;
    }
    cursor_ = at;
    scroll_to_cursor();
}

void MaterializedCellGridModel::place_cursor(GridPosition at, bool extend) {
    if (rows_.empty() || column_count_ == 0) return;
    begin_extend(extend);
    cursor_ = clamped(at);
    scroll_to_cursor();
}

void MaterializedCellGridModel::scroll_by(int rows, int columns) {
    if (rows_.empty() || column_count_ == 0) return;
    const auto shifted = [](GridIndex value, int delta, GridIndex low, GridIndex high) {
        if (delta < 0) {
            const GridIndex back = static_cast<GridIndex>(-delta);
            return value > low + back ? value - back : low;
        }
        return std::min(value + static_cast<GridIndex>(delta), high);
    };
    origin_.row = shifted(origin_.row, rows, frozen_row_band(), rows_.size() - 1);
    origin_.column = shifted(origin_.column, columns, frozen_column_band(), column_count_ - 1);
}

void MaterializedCellGridModel::begin_extend(bool extend) {
    if (!extend) {
        anchor_.reset();
        return;
    }
    if (!anchor_) anchor_ = cursor_;
}

void MaterializedCellGridModel::clamp_origin() {
    if (rows_.empty() || column_count_ == 0) {
        origin_ = {};
        return;
    }
    origin_.row = std::clamp(origin_.row, frozen_row_band(), rows_.size() - 1);
    origin_.column = std::clamp(origin_.column, frozen_column_band(), column_count_ - 1);
}

void MaterializedCellGridModel::scroll_to_cursor() {
    if (rows_.empty() || column_count_ == 0) return;
    clamp_origin();
    const std::size_t frozen_rows = frozen_row_band();
    const std::size_t frozen_columns = frozen_column_band();
    const std::size_t body = std::max<std::size_t>(body_rows(), 1);
    if (cursor_.row >= frozen_rows) {
        if (cursor_.row < origin_.row) origin_.row = cursor_.row;
        if (cursor_.row >= origin_.row + body) origin_.row = cursor_.row - body + 1;
    }
    if (cursor_.column >= frozen_columns) {
        if (cursor_.column < origin_.column) origin_.column = cursor_.column;
        while (cursor_.column >= origin_.column + fitted_columns(origin_.column)) ++origin_.column;
    }
}

GridPosition MaterializedCellGridModel::clamped(GridPosition at) const noexcept {
    if (rows_.empty() || column_count_ == 0) return {};
    return {std::min(at.row, rows_.size() - 1), std::min(at.column, column_count_ - 1)};
}

// --- CellGrid -----------------------------------------------------------------

CellGrid::CellGrid() { set_focus_policy(ui::FocusPolicy::TabStop); }

void CellGrid::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.cellgrid.normal");
    if (header_role_ == ui::kInvalidRole) header_role_ = context().roles->find("ckv.cellgrid.header");
    if (cursor_role_ == ui::kInvalidRole) cursor_role_ = context().roles->find("ckv.cellgrid.cursor");
    if (selection_role_ == ui::kInvalidRole)
        selection_role_ = context().roles->find("ckv.cellgrid.selection");
}

void CellGrid::set_model(CellGridModel& model) {
    model_ = &model;
    relayout();
    invalidate();
}

void CellGrid::clear_model() {
    model_ = nullptr;
    dragging_ = false;
    relayout();
    invalidate();
}

void CellGrid::model_changed() {
    relayout();
    invalidate();
}

void CellGrid::set_column_header_visible(bool visible) {
    if (column_header_ == visible) return;
    column_header_ = visible;
    relayout();
    invalidate();
}

void CellGrid::on_resized() { relayout(); }

void CellGrid::relayout() {
    frame_ = GridFrame{};
    column_widths_.clear();
    gutter_width_ = 0;
    if (model_ == nullptr) return;
    const int rows = std::max(0, bounds().height - header_height());
    const int width = std::max(0, bounds().width);
    int gutter = std::clamp(model_->row_header_width(), 0, width);
    // Twice is enough to settle: the second reading is for the rows the
    // first viewport showed, and a third would repeat it.
    for (int pass = 0; pass < 2; ++pass) {
        model_->set_viewport(rows, width - gutter);
        frame_ = model_->frame();
        const int settled = std::clamp(model_->row_header_width(), 0, width);
        if (settled == gutter) break;
        gutter = settled;
    }
    gutter_width_ = gutter;
    column_widths_.resize(frame_.columns.size());
    for (std::size_t index = 0; index < frame_.columns.size(); ++index)
        column_widths_[index] = std::max(1, model_->column_width(frame_.columns[index]));
}

int CellGrid::column_x(std::size_t index) const noexcept {
    int x = gutter_width_;
    for (std::size_t before = 0; before < index && before < column_widths_.size(); ++before)
        x += column_widths_[before];
    return x;
}

const GridRange* CellGrid::span_covering(GridPosition at) const noexcept {
    for (const GridRange& span : frame_.spans)
        if (span.contains(at)) return &span;
    return nullptr;
}

bool CellGrid::column_listed(GridIndex column) const noexcept {
    return std::find(frame_.columns.begin(), frame_.columns.end(), column) != frame_.columns.end();
}

std::optional<GridPosition> CellGrid::cell_at(Point local) const {
    if (model_ == nullptr || local.x < gutter_width_) return std::nullopt;
    const int row = local.y - header_height();
    if (row < 0 || static_cast<std::size_t>(row) >= frame_.rows.size()) return std::nullopt;
    int x = gutter_width_;
    for (std::size_t index = 0; index < frame_.columns.size(); ++index) {
        const int next = x + column_widths_[index];
        if (local.x >= x && local.x < next) return GridPosition{frame_.rows[static_cast<std::size_t>(row)], frame_.columns[index]};
        x = next;
    }
    return std::nullopt;
}

std::optional<Rect> CellGrid::cell_rect(GridPosition at) const {
    const auto row = std::find(frame_.rows.begin(), frame_.rows.end(), at.row);
    const auto column = std::find(frame_.columns.begin(), frame_.columns.end(), at.column);
    if (row == frame_.rows.end() || column == frame_.columns.end()) return std::nullopt;
    const std::size_t column_index = static_cast<std::size_t>(column - frame_.columns.begin());
    int width = column_widths_[column_index];
    if (const GridRange* span = span_covering(at); span != nullptr && span->first == at)
        for (std::size_t next = column_index + 1;
             next < frame_.columns.size() && frame_.columns[next] <= span->last.column; ++next)
            width += column_widths_[next];
    return Rect{column_x(column_index), header_height() + static_cast<int>(row - frame_.rows.begin()), width, 1};
}

Style CellGrid::cell_style(const GridCell& cell, bool is_cursor, bool is_selected) const {
    const ui::Theme& theme = *context().theme;
    Style style = theme.resolve(normal_role_);
    if (cell.style.foreground) style.fg = *cell.style.foreground;
    if (cell.style.background) style.bg = *cell.style.background;
    if (cell.style.attributes) style.attrs = *cell.style.attributes;
    if (cell.style.underline) style.underline = *cell.style.underline;
    if (!is_cursor && !is_selected) return style;

    const Style overlay = theme.resolve(is_cursor ? cursor_role_ : selection_role_);
    if (!cell.style.has_color()) {
        style.fg = overlay.fg;
        style.bg = overlay.bg;
        style.attrs |= overlay.attrs;
        return style;
    }
    Style swapped = style;
    swapped.fg = style.bg;
    swapped.bg = style.fg;
    if (!readable(swapped.fg, swapped.bg)) {
        swapped.fg = overlay.fg;
        swapped.bg = overlay.bg;
    }
    if (is_cursor) swapped.attrs |= Attr::Bold | Attr::Underline;
    return swapped;
}

void CellGrid::draw(scene::Painter& painter) {
    const ui::Theme& theme = *context().theme;
    const Style header = theme.resolve(header_role_);
    const Style normal = theme.resolve(normal_role_);
    const int width = bounds().width;
    const int height = bounds().height;
    if (width <= 0 || height <= 0) return;

    if (column_header_) {
        painter.fill(Rect{0, 0, width, 1}, Cell::from_grapheme(" ", header));
        for (std::size_t index = 0; index < frame_.columns.size(); ++index) {
            const int x = column_x(index);
            if (x >= width) break;
            const int visible = std::min(column_widths_[index], width - x);
            const std::string label =
                text::clip_to_width(model_->column_label(frame_.columns[index]), visible);
            painter.draw_text(Point{text_start(x, visible, text::text_width(label), CellAlignment::Center), 0},
                              label, header);
        }
    }

    const GridPosition cursor = model_ != nullptr ? model_->cursor() : GridPosition{};
    const std::optional<GridRange> selection = model_ != nullptr ? model_->selection() : std::nullopt;
    int y = header_height();
    for (std::size_t row_index = 0; row_index < frame_.rows.size() && y < height; ++row_index, ++y) {
        const GridIndex row = frame_.rows[row_index];
        painter.fill(Rect{0, y, width, 1}, Cell::from_grapheme(" ", normal));
        if (gutter_width_ > 0) {
            painter.fill(Rect{0, y, gutter_width_, 1}, Cell::from_grapheme(" ", header));
            const int label_width = gutter_width_ - 1;
            const std::string label = text::clip_to_width(model_->row_label(row), label_width);
            painter.draw_text(Point{text_start(0, label_width, text::text_width(label), CellAlignment::End), y},
                              label, header);
        }
        for (std::size_t column_index = 0; column_index < frame_.columns.size(); ++column_index) {
            const int x = column_x(column_index);
            if (x >= width) break;
            const GridPosition at{row, frame_.columns[column_index]};
            const bool is_cursor = at == cursor;
            const bool is_selected = !is_cursor && selection.has_value() && selection->contains(at);
            const GridRange* const span = span_covering(at);
            const bool anchor = span != nullptr && span->first == at;
            // A cell the anchor's paint already covers keeps that paint —
            // unless the cursor is on it, which must show where it really is.
            if (span != nullptr && !anchor && at.row == span->first.row && column_listed(span->first.column) &&
                !is_cursor)
                continue;
            int span_width = column_widths_[column_index];
            if (anchor)
                for (std::size_t next = column_index + 1;
                     next < frame_.columns.size() && frame_.columns[next] <= span->last.column; ++next)
                    span_width += column_widths_[next];
            const GridCell cell = model_->cell(at.row, at.column);
            const Style style = cell_style(cell, is_cursor, is_selected);
            painter.fill(Rect{x, y, span_width, 1}, Cell::from_grapheme(" ", style));
            // A covered cell shows no text of its own, whatever the provider
            // still holds for it: the span's anchor speaks for the region.
            const bool covered = span != nullptr && !anchor;
            const std::string shown = text::clip_to_width(covered ? std::string_view{} : cell.text, span_width);
            painter.draw_text(Point{text_start(x, span_width, text::text_width(shown), cell.alignment), y}, shown,
                              style);
        }
    }
    for (; y < height; ++y) painter.fill(Rect{0, y, width, 1}, Cell::from_grapheme(" ", normal));
}

void CellGrid::after_request() {
    relayout();
    invalidate();
    if (on_changed) on_changed();
}

bool CellGrid::request(GridMove move, bool extend) {
    model_->navigate(move, extend);
    after_request();
    return true;
}

bool CellGrid::on_key(const KeyEvent& event) {
    if (event.action == KeyAction::Release || model_ == nullptr) return false;
    // A grid with nothing in it has nothing to move or activate; its owner
    // may still mean something by the key.
    if (frame_.rows.empty() || frame_.columns.empty()) return false;
    const Modifier modifiers = event.chord.modifiers;
    const bool shift = has_modifier(modifiers, Modifier::Shift);
    const bool ctrl = has_modifier(modifiers, Modifier::Ctrl);
    const bool other = has_modifier(modifiers, Modifier::Alt) || has_modifier(modifiers, Modifier::Super);
    if (other) return false;
    switch (event.chord.key) {
        case Key::Up:
            return request(ctrl ? GridMove::JumpUp : GridMove::Up, shift);
        case Key::Down:
            return request(ctrl ? GridMove::JumpDown : GridMove::Down, shift);
        case Key::Left:
            return request(ctrl ? GridMove::JumpLeft : GridMove::Left, shift);
        case Key::Right:
            return request(ctrl ? GridMove::JumpRight : GridMove::Right, shift);
        case Key::PageUp:
            // Ctrl+PageUp/PageDown are left to the owner: an application with
            // several sheets turns pages of a different kind with them.
            return !ctrl && request(GridMove::PageUp, shift);
        case Key::PageDown:
            return !ctrl && request(GridMove::PageDown, shift);
        case Key::Home:
            return request(ctrl ? GridMove::GridStart : GridMove::RowStart, shift);
        case Key::End:
            return request(ctrl ? GridMove::GridEnd : GridMove::RowEnd, shift);
        case Key::Enter:
        case Key::F2:
            if (!on_activate || ctrl || shift) return false;
            on_activate();
            return true;
        case Key::Delete:
            if (!on_clear_request || ctrl || shift) return false;
            on_clear_request();
            return true;
        case Key::Char:
            if (!on_type_ahead || ctrl || event.chord.text.empty()) return false;
            on_type_ahead(event.chord.text);
            return true;
        default:
            return false;
    }
}

bool CellGrid::on_mouse(const MouseEvent& event) {
    if (model_ == nullptr) return false;
    const Rect absolute = absolute_bounds();
    const Point local{event.cell.x - absolute.x, event.cell.y - absolute.y};
    switch (event.action) {
        case MouseAction::Wheel: {
            int rows = 0;
            int columns = 0;
            switch (event.button) {
                case MouseButton::WheelUp:
                    rows = -kWheelRows;
                    break;
                case MouseButton::WheelDown:
                    rows = kWheelRows;
                    break;
                case MouseButton::WheelLeft:
                    columns = -1;
                    break;
                case MouseButton::WheelRight:
                    columns = 1;
                    break;
                default:
                    return false;
            }
            model_->scroll_by(rows, columns);
            after_request();
            return true;
        }
        case MouseAction::Down: {
            if (event.button != MouseButton::Left) return false;
            const std::optional<GridPosition> at = cell_at(local);
            if (!at) return false;
            const std::int64_t now = context().app != nullptr ? context().app->clock().now_nanos() : -1;
            const bool double_click = now >= 0 && last_click_nanos_ >= 0 && last_click_ == at &&
                                      now - last_click_nanos_ <= kDoubleClickIntervalNanos;
            last_click_ = at;
            last_click_nanos_ = now;
            model_->place_cursor(*at, has_modifier(event.modifiers, Modifier::Shift));
            dragging_ = !double_click;
            after_request();
            if (double_click) {
                last_click_.reset();
                if (on_activate) on_activate();
            }
            return true;
        }
        case MouseAction::DoubleClick: {
            const std::optional<GridPosition> at = cell_at(local);
            if (!at) return false;
            dragging_ = false;
            if (*at != model_->cursor()) {
                model_->place_cursor(*at, false);
                after_request();
            }
            if (on_activate) on_activate();
            return true;
        }
        case MouseAction::Move: {
            if (!dragging_) return false;
            const std::optional<GridPosition> at = cell_at(local);
            if (!at || *at == model_->cursor()) return true;
            model_->place_cursor(*at, true);
            after_request();
            return true;
        }
        case MouseAction::Up:
            if (!dragging_) return false;
            dragging_ = false;
            return true;
    }
    return false;
}

}  // namespace ckv::widgets
