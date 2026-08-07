// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/table.hpp"

#include <algorithm>
#include <numeric>
#include <optional>

#include "cvision/core/assert.hpp"

namespace ckv::widgets {

Table::Table() {
    scrollbar_ = make<Scrollbar>(Orientation::Vertical);
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void Table::on_attached() {
    if (header_role_ == ui::kInvalidRole) header_role_ = context().roles->find("ckv.table.header");
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.list.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
}

void Table::set_columns(std::vector<TableColumn> columns) {
    columns_ = std::move(columns);
    invalidate();
}

void Table::set_rows(std::vector<std::vector<std::string>> rows) {
    for (const auto& r : rows) CKV_ASSERT(r.size() == columns_.size());
    rows_ = std::move(rows);
    rebuild_order();
    cursor_row_ = rows_.empty() ? -1 : 0;
    on_resized();
    ensure_cursor_visible();
    invalidate();
}

const std::vector<std::string>& Table::row(std::size_t display_index) const {
    CKV_ASSERT(display_index < order_.size());
    return rows_[order_[display_index]];
}

void Table::set_cell_style_hook(std::function<Style(std::size_t, std::size_t, Style)> hook) {
    cell_style_hook_ = std::move(hook);
    invalidate();
}

void Table::rebuild_order() {
    order_.resize(rows_.size());
    std::iota(order_.begin(), order_.end(), 0);
    if (sort_column_ < 0) return;
    const std::size_t col = static_cast<std::size_t>(sort_column_);
    std::stable_sort(order_.begin(), order_.end(), [&](std::size_t a, std::size_t b) {
        return sort_ascending_ ? rows_[a][col] < rows_[b][col] : rows_[a][col] > rows_[b][col];
    });
}

void Table::sort_by(int column, bool ascending) {
    CKV_ASSERT(column == -1 || (column >= 0 && static_cast<std::size_t>(column) < columns_.size()));

    // Preserve selection IDENTITY (which underlying row is selected),
    // not just the display index — a sort must not silently move the
    // cursor onto a different row's data.
    std::optional<std::size_t> underlying_cursor;
    if (cursor_row_ >= 0 && static_cast<std::size_t>(cursor_row_) < order_.size())
        underlying_cursor = order_[static_cast<std::size_t>(cursor_row_)];

    sort_column_ = column;
    sort_ascending_ = ascending;
    rebuild_order();

    if (underlying_cursor) {
        const auto it = std::find(order_.begin(), order_.end(), *underlying_cursor);
        if (it != order_.end()) cursor_row_ = static_cast<int>(it - order_.begin());
    }
    invalidate();
}

int Table::column_start_x(std::size_t column) const noexcept {
    int x = 0;
    for (std::size_t i = 0; i < column && i < columns_.size(); ++i) x += columns_[i].width + 1;
    return x;
}

int Table::column_at_x(int local_x) const noexcept {
    int x = 0;
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (local_x >= x && local_x < x + columns_[i].width) return static_cast<int>(i);
        x += columns_[i].width + 1;
    }
    return -1;
}

void Table::on_resized() {
    if (scrollbar_ == nullptr) return;
    const int data_height = std::max(0, bounds().height - 1);
    scrollbar_->set_bounds(Rect{std::max(0, bounds().width - 1), 1, std::min(1, bounds().width), data_height});
    scrollbar_->set_range(static_cast<int>(order_.size()), std::max(1, data_height));
}

void Table::ensure_cursor_visible() {
    if (scrollbar_ == nullptr || cursor_row_ < 0) return;
    if (cursor_row_ < scrollbar_->position()) {
        scrollbar_->set_position(cursor_row_);
    } else if (cursor_row_ >= scrollbar_->position() + scrollbar_->viewport_size()) {
        scrollbar_->set_position(cursor_row_ - scrollbar_->viewport_size() + 1);
    }
}

bool Table::on_key(const KeyEvent& event) {
    if (order_.empty()) return false;
    const int last = static_cast<int>(order_.size()) - 1;
    const int data_height = std::max(1, bounds().height - 1);
    switch (event.chord.key) {
        case Key::Up:
            cursor_row_ = std::clamp(cursor_row_ - 1, 0, last);
            ensure_cursor_visible();
            invalidate();
            return true;
        case Key::Down:
            cursor_row_ = std::clamp(cursor_row_ + 1, 0, last);
            ensure_cursor_visible();
            invalidate();
            return true;
        case Key::PageUp:
            cursor_row_ = std::clamp(cursor_row_ - data_height, 0, last);
            ensure_cursor_visible();
            invalidate();
            return true;
        case Key::PageDown:
            cursor_row_ = std::clamp(cursor_row_ + data_height, 0, last);
            ensure_cursor_visible();
            invalidate();
            return true;
        case Key::Home:
            cursor_row_ = 0;
            ensure_cursor_visible();
            invalidate();
            return true;
        case Key::End:
            cursor_row_ = last;
            ensure_cursor_visible();
            invalidate();
            return true;
        default:
            return false;
    }
}

bool Table::on_mouse(const MouseEvent& event) {
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};

    if (event.action == MouseAction::Down) {
        if (local.y < 0 || local.y >= bounds().height || local.x < 0) return false;
        if (local.y == 0) {
            for (std::size_t c = 0; c < columns_.size(); ++c) {
                const int boundary = column_start_x(c) + columns_[c].width;
                if (local.x == boundary) {
                    resizing_column_ = static_cast<int>(c);
                    resize_start_x_ = local.x;
                    resize_start_width_ = columns_[c].width;
                    return true;
                }
            }
            const int column = column_at_x(local.x);
            if (column < 0) return false;
            if (sort_column_ == column)
                sort_by(column, !sort_ascending_);
            else
                sort_by(column, true);
            return true;
        }
        if (order_.empty()) return false;
        const int index = scrollbar_->position() + (local.y - 1);
        if (index < 0 || static_cast<std::size_t>(index) >= order_.size()) return false;
        cursor_row_ = index;
        invalidate();
        return true;
    }

    if (resizing_column_ < 0) return false;
    if (event.action == MouseAction::Move) {
        const int delta = local.x - resize_start_x_;
        columns_[static_cast<std::size_t>(resizing_column_)].width =
            std::max(columns_[static_cast<std::size_t>(resizing_column_)].min_width, resize_start_width_ + delta);
        invalidate();
        return true;
    }
    if (event.action == MouseAction::Up) {
        resizing_column_ = -1;
        return true;
    }
    return false;
}

void Table::draw(scene::Painter& painter) {
    const Style header_style = context().theme->resolve(header_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", header_style));
    for (std::size_t c = 0; c < columns_.size(); ++c) {
        const int x = column_start_x(c);
        if (x >= bounds().width) break;
        std::string title = columns_[c].title;
        if (static_cast<int>(c) == sort_column_) title += sort_ascending_ ? " ^" : " v";
        painter.draw_text(Point{x, 0}, title, header_style);
    }

    const int top = scrollbar_ != nullptr ? scrollbar_->position() : 0;
    const int data_height = std::max(0, bounds().height - 1);
    for (int row_i = 0; row_i < data_height; ++row_i) {
        const int display_index = top + row_i;
        const Style row_style = (display_index == cursor_row_) ? context().theme->resolve(selected_role_)
                                                                 : context().theme->resolve(normal_role_);
        painter.fill(Rect{0, row_i + 1, std::max(0, bounds().width - 1), 1}, Cell::from_grapheme(" ", row_style));
        if (display_index < 0 || static_cast<std::size_t>(display_index) >= order_.size()) continue;
        const std::size_t underlying = order_[static_cast<std::size_t>(display_index)];
        for (std::size_t c = 0; c < columns_.size(); ++c) {
            const int x = column_start_x(c);
            if (x >= bounds().width) break;
            const Style style = cell_style_hook_ ? cell_style_hook_(underlying, c, row_style) : row_style;
            painter.draw_text(Point{x, row_i + 1}, rows_[underlying][c], style);
        }
    }
}

}  // namespace ckv::widgets
