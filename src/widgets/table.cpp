// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/table.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <iomanip>
#include <limits>
#include <locale>
#include <numeric>
#include <sstream>
#include <string_view>
#include <type_traits>

#include "cvision/core/assert.hpp"
#include "cvision/core/text.hpp"

namespace ckv::widgets {

namespace {

std::string ascii_lower(std::string value) {
    for (char& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

std::string format_real(double value) {
#if defined(CKVISION_HAS_FLOAT_CHARCONV)
    char buffer[64];
    const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
    return result.ec == std::errc{} ? std::string(buffer, result.ptr) : std::string{};
#else
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return stream ? stream.str() : std::string{};
#endif
}

bool parse_real(std::string_view text, double& value) {
#if defined(CKVISION_HAS_FLOAT_CHARCONV)
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
#else
    std::istringstream stream{std::string(text)};
    stream.imbue(std::locale::classic());
    stream >> std::noskipws >> value;
    return stream && stream.peek() == std::char_traits<char>::eof();
#endif
}

}  // namespace

std::string format_cell_value(const CellValue& value) {
    return std::visit(
        [](const auto& item) -> std::string {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return {};
            } else if constexpr (std::is_same_v<T, bool>) {
                return item ? "true" : "false";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return item;
            } else if constexpr (std::is_same_v<T, double>) {
                return format_real(item);
            } else {
                char buffer[64];
                const auto result = std::to_chars(std::begin(buffer), std::end(buffer), item);
                return result.ec == std::errc{} ? std::string(buffer, result.ptr) : std::string{};
            }
        },
        value);
}

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
    if (cursor_column_ >= static_cast<int>(columns_.size())) cursor_column_ = std::max(0, static_cast<int>(columns_.size()) - 1);
    cancel_edit();
    invalidate();
}

void Table::set_model(TableModel& model) {
    model_ = &model;
    rows_.clear();
    order_.clear();
    cursor_row_id_ = kInvalidTableRowId;
    cursor_row_ = -1;
    cursor_column_ = 0;
    cancel_edit();
    resolve_model_identity(true);
    on_resized();
    invalidate();
}

void Table::clear_model() {
    model_ = nullptr;
    rows_.clear();
    order_.clear();
    cursor_row_id_ = kInvalidTableRowId;
    cursor_row_ = -1;
    cursor_column_ = 0;
    cancel_edit();
    if (scrollbar_ != nullptr) scrollbar_->set_range(0, std::max(1, bounds().height - 1));
    invalidate();
}

void Table::model_changed() {
    resolve_model_identity(false);
    if (editing_) {
        const auto selected = selected_cell();
        if (!selected) cancel_edit();
    }
    on_resized();
    ensure_cursor_visible();
    invalidate();
}

void Table::set_rows(std::vector<std::vector<std::string>> rows) {
    for (const auto& row : rows) CKV_ASSERT(row.size() == columns_.size());
    model_ = nullptr;
    rows_ = std::move(rows);
    rebuild_order();
    cursor_row_ = rows_.empty() ? -1 : 0;
    cursor_row_id_ = cursor_row_ < 0 ? kInvalidTableRowId : row_id_at(0);
    cursor_column_ = 0;
    cancel_edit();
    on_resized();
    ensure_cursor_visible();
    invalidate();
}

const std::vector<std::string>& Table::row(std::size_t display_index) const {
    CKV_ASSERT(model_ == nullptr);
    CKV_ASSERT(display_index < order_.size());
    return rows_[order_[display_index]];
}

std::size_t Table::row_count() const { return model_ != nullptr ? model_->row_count() : order_.size(); }

void Table::set_cell_style_hook(std::function<Style(std::size_t, std::size_t, Style)> hook) {
    cell_style_hook_ = std::move(hook);
    invalidate();
}

std::size_t Table::model_row_count() const { return model_ != nullptr ? model_->row_count() : order_.size(); }

TableRowId Table::row_id_at(std::size_t display_index) const {
    if (display_index >= model_row_count()) return kInvalidTableRowId;
    if (model_ != nullptr) return model_->row_id_at(display_index);
    return static_cast<TableRowId>(order_[display_index] + 1);
}

std::optional<std::size_t> Table::index_of(TableRowId row_id) const {
    if (row_id == kInvalidTableRowId) return std::nullopt;
    if (model_ != nullptr) return model_->index_of(row_id);
    const std::size_t underlying = static_cast<std::size_t>(row_id - 1);
    const auto found = std::find(order_.begin(), order_.end(), underlying);
    return found == order_.end() ? std::nullopt : std::optional<std::size_t>(static_cast<std::size_t>(found - order_.begin()));
}

TableCell Table::cell_at(TableCellRef reference) const {
    if (reference.row == kInvalidTableRowId || reference.column >= columns_.size()) return {};
    if (model_ != nullptr) return model_->cell(reference);
    const std::size_t underlying = static_cast<std::size_t>(reference.row - 1);
    if (underlying >= rows_.size()) return {};
    const std::string& value = rows_[underlying][reference.column];
    return TableCell{value, value, std::nullopt, columns_[reference.column].editable};
}

void Table::rebuild_order() {
    order_.resize(rows_.size());
    std::iota(order_.begin(), order_.end(), 0);
    if (sort_column_ < 0) return;
    const std::size_t column = static_cast<std::size_t>(sort_column_);
    std::stable_sort(order_.begin(), order_.end(), [this, column](std::size_t left, std::size_t right) {
        return sort_ascending_ ? rows_[left][column] < rows_[right][column]
                               : rows_[left][column] > rows_[right][column];
    });
}

void Table::sort_by(int column, bool ascending) {
    CKV_ASSERT(column == -1 || (column >= 0 && static_cast<std::size_t>(column) < columns_.size()));
    sort_column_ = column;
    sort_ascending_ = ascending;
    if (model_ != nullptr) {
        const std::optional<std::size_t> requested = column < 0 ? std::nullopt : std::optional<std::size_t>(column);
        model_->request_sort(requested, ascending);
        if (const auto selected = selected_cell(); selected && on_sort_requested)
            on_sort_requested(*selected, requested, ascending);
    } else {
        rebuild_order();
        resolve_model_identity(false);
    }
    invalidate();
}

std::optional<TableCellRef> Table::selected_cell() const noexcept {
    if (cursor_row_id_ == kInvalidTableRowId || cursor_column_ < 0 ||
        cursor_column_ >= static_cast<int>(columns_.size()))
        return std::nullopt;
    return TableCellRef{cursor_row_id_, static_cast<std::size_t>(cursor_column_)};
}

void Table::set_selected_cell(TableCellRef reference) {
    const auto row = index_of(reference.row);
    if (!row || reference.column >= columns_.size()) return;
    move_cursor(static_cast<int>(*row), static_cast<int>(reference.column));
}

void Table::resolve_model_identity(bool select_first_when_unset) {
    const std::size_t count = model_row_count();
    if (count == 0) {
        cursor_row_id_ = kInvalidTableRowId;
        cursor_row_ = -1;
        return;
    }
    if (const auto index = index_of(cursor_row_id_)) {
        cursor_row_ = static_cast<int>(*index);
    } else if (select_first_when_unset) {
        cursor_row_ = 0;
        cursor_row_id_ = row_id_at(0);
    } else {
        cursor_row_ = -1;
        cursor_row_id_ = kInvalidTableRowId;
    }
    if (columns_.empty())
        cursor_column_ = 0;
    else
        cursor_column_ = std::clamp(cursor_column_, 0, static_cast<int>(columns_.size()) - 1);
}

void Table::move_cursor(int row, int column, bool notify) {
    const std::size_t count = model_row_count();
    if (count == 0 || columns_.empty()) return;
    const int next_row = std::clamp(row, 0, static_cast<int>(count) - 1);
    const int next_column = std::clamp(column, 0, static_cast<int>(columns_.size()) - 1);
    const TableRowId next_id = row_id_at(static_cast<std::size_t>(next_row));
    const bool changed = next_id != cursor_row_id_ || next_column != cursor_column_;
    cursor_row_ = next_row;
    cursor_row_id_ = next_id;
    cursor_column_ = next_column;
    ensure_cursor_visible();
    if (changed) {
        cancel_edit();
        if (notify && on_selection_changed) on_selection_changed(TableCellRef{cursor_row_id_, static_cast<std::size_t>(cursor_column_)});
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
    scrollbar_->set_range(static_cast<int>(model_row_count()), std::max(1, data_height));
}

void Table::ensure_cursor_visible() {
    if (scrollbar_ == nullptr || cursor_row_ < 0) return;
    if (cursor_row_ < scrollbar_->position()) {
        scrollbar_->set_position(cursor_row_);
    } else if (cursor_row_ >= scrollbar_->position() + scrollbar_->viewport_size()) {
        scrollbar_->set_position(cursor_row_ - scrollbar_->viewport_size() + 1);
    }
}

bool Table::begin_edit() {
    const auto reference = selected_cell();
    if (!reference) return false;
    const TableCell cell = cell_at(*reference);
    if (!columns_[reference->column].editable || !cell.editable) return false;
    edit_text_ = cell.display.empty() ? format_cell_value(cell.value) : cell.display;
    edit_diagnostic_.clear();
    editing_ = true;
    invalidate();
    return true;
}

bool Table::parse_edit_value(CellValue& out, std::string& diagnostic) const {
    if (cursor_column_ < 0 || cursor_column_ >= static_cast<int>(columns_.size())) return false;
    switch (columns_[static_cast<std::size_t>(cursor_column_)].type) {
        case TableCellType::Text:
            out = edit_text_;
            return true;
        case TableCellType::Boolean: {
            const std::string value = ascii_lower(edit_text_);
            if (value == "true" || value == "1") {
                out = true;
                return true;
            }
            if (value == "false" || value == "0") {
                out = false;
                return true;
            }
            diagnostic = "Enter true, false, 1, or 0";
            return false;
        }
        case TableCellType::Integer: {
            std::int64_t value = 0;
            const auto result = std::from_chars(edit_text_.data(), edit_text_.data() + edit_text_.size(), value);
            if (result.ec == std::errc{} && result.ptr == edit_text_.data() + edit_text_.size()) {
                out = value;
                return true;
            }
            diagnostic = "Enter an integer";
            return false;
        }
        case TableCellType::Real: {
            double value = 0.0;
            if (parse_real(edit_text_, value)) {
                out = value;
                return true;
            }
            diagnostic = "Enter a real number";
            return false;
        }
    }
    return false;
}

TableEditResult Table::commit_value(TableCellRef reference, const CellValue& value) {
    if (model_ != nullptr) return model_->commit(reference, value);
    const std::size_t underlying = static_cast<std::size_t>(reference.row - 1);
    if (underlying >= rows_.size() || reference.column >= columns_.size()) return TableEditResult::reject("Cell disappeared");
    rows_[underlying][reference.column] = format_cell_value(value);
    if (sort_column_ >= 0) rebuild_order();
    return TableEditResult::accept();
}

bool Table::commit_edit() {
    const auto reference = selected_cell();
    if (!editing_ || !reference) return false;
    CellValue value;
    std::string diagnostic;
    if (!parse_edit_value(value, diagnostic)) {
        edit_diagnostic_ = std::move(diagnostic);
        invalidate();
        return false;
    }
    const TableEditResult result = commit_value(*reference, value);
    if (on_edit_committed) on_edit_committed(*reference, result);
    if (!result.accepted) {
        edit_diagnostic_ = result.diagnostic;
        invalidate();
        return false;
    }
    editing_ = false;
    edit_diagnostic_.clear();
    resolve_model_identity(false);
    invalidate();
    return true;
}

void Table::cancel_edit() noexcept {
    editing_ = false;
    edit_text_.clear();
    edit_diagnostic_.clear();
}

bool Table::on_key(const KeyEvent& event) {
    if (event.action == KeyAction::Release) return false;
    if (editing_) {
        switch (event.chord.key) {
            case Key::Escape:
                cancel_edit();
                invalidate();
                return true;
            case Key::Enter:
                return commit_edit();
            case Key::Backspace: {
                const auto graphemes = text::split_graphemes(edit_text_);
                if (!graphemes.empty()) edit_text_.erase(graphemes.back().data() - edit_text_.data());
                edit_diagnostic_.clear();
                invalidate();
                return true;
            }
            case Key::Delete:
                edit_text_.clear();
                edit_diagnostic_.clear();
                invalidate();
                return true;
            case Key::Char:
                return on_text(TextEvent{event.chord.text, false});
            default:
                return false;
        }
    }

    if (model_row_count() == 0 || columns_.empty()) return false;
    const int last = static_cast<int>(model_row_count()) - 1;
    const int data_height = std::max(1, bounds().height - 1);
    switch (event.chord.key) {
        case Key::Up:
            move_cursor(cursor_row_ - 1, cursor_column_);
            return true;
        case Key::Down:
            move_cursor(cursor_row_ + 1, cursor_column_);
            return true;
        case Key::Left:
            move_cursor(cursor_row_, cursor_column_ - 1);
            return true;
        case Key::Right:
            move_cursor(cursor_row_, cursor_column_ + 1);
            return true;
        case Key::PageUp:
            move_cursor(cursor_row_ - data_height, cursor_column_);
            return true;
        case Key::PageDown:
            move_cursor(cursor_row_ + data_height, cursor_column_);
            return true;
        case Key::Home:
            move_cursor(0, cursor_column_);
            return true;
        case Key::End:
            move_cursor(last, cursor_column_);
            return true;
        case Key::F2:
        case Key::Enter:
            return begin_edit();
        case Key::Char:
            if (begin_edit()) {
                edit_text_.clear();
                return on_text(TextEvent{event.chord.text, false});
            }
            return false;
        default:
            return false;
    }
}

bool Table::on_text(const TextEvent& event) {
    if (!editing_ || event.text.empty()) return false;
    edit_text_ += event.text;
    edit_diagnostic_.clear();
    invalidate();
    return true;
}

bool Table::on_mouse(const MouseEvent& event) {
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};

    if (event.action == MouseAction::Down) {
        if (local.y < 0 || local.y >= bounds().height || local.x < 0) return false;
        if (local.y == 0) {
            for (std::size_t column = 0; column < columns_.size(); ++column) {
                const int boundary = column_start_x(column) + columns_[column].width;
                if (local.x == boundary) {
                    resizing_column_ = static_cast<int>(column);
                    resize_start_x_ = local.x;
                    resize_start_width_ = columns_[column].width;
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
        const int index = scrollbar_->position() + (local.y - 1);
        if (index < 0 || static_cast<std::size_t>(index) >= model_row_count()) return false;
        const int column = column_at_x(local.x);
        if (column < 0) return false;
        move_cursor(index, column);
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
    const Style normal_style = context().theme->resolve(normal_role_);
    const Style selected_style = context().theme->resolve(selected_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", header_style));
    for (std::size_t column = 0; column < columns_.size(); ++column) {
        const int x = column_start_x(column);
        if (x >= bounds().width) break;
        std::string title = columns_[column].title;
        if (static_cast<int>(column) == sort_column_) title += sort_ascending_ ? " ^" : " v";
        painter.draw_text(Point{x, 0}, text::clip_to_width(title, columns_[column].width), header_style);
    }

    const int top = scrollbar_ != nullptr ? scrollbar_->position() : 0;
    const int data_height = std::max(0, bounds().height - 1);
    const std::size_t count = model_row_count();
    for (int visible_row = 0; visible_row < data_height; ++visible_row) {
        const int display_row = top + visible_row;
        painter.fill(Rect{0, visible_row + 1, std::max(0, bounds().width - 1), 1},
                     Cell::from_grapheme(" ", normal_style));
        if (display_row < 0 || static_cast<std::size_t>(display_row) >= count) continue;
        const TableRowId row_id = row_id_at(static_cast<std::size_t>(display_row));
        for (std::size_t column = 0; column < columns_.size(); ++column) {
            const int x = column_start_x(column);
            if (x >= bounds().width) break;
            const TableCellRef reference{row_id, column};
            const TableCell cell = cell_at(reference);
            Style style = row_id == cursor_row_id_ && static_cast<int>(column) == cursor_column_ ? selected_style : normal_style;
            if (cell.style) style = *cell.style;
            if (model_ == nullptr && cell_style_hook_) {
                style = cell_style_hook_(static_cast<std::size_t>(row_id - 1), column, style);
            }
            std::string shown = cell.display.empty() ? format_cell_value(cell.value) : cell.display;
            if (editing_ && row_id == cursor_row_id_ && static_cast<int>(column) == cursor_column_) shown = edit_text_;
            painter.fill(Rect{x, visible_row + 1, columns_[column].width, 1}, Cell::from_grapheme(" ", style));
            painter.draw_text(Point{x, visible_row + 1}, text::clip_to_width(shown, columns_[column].width), style);
        }
    }
}

}  // namespace ckv::widgets
