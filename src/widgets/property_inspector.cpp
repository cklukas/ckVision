// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/property_inspector.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <utility>
#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "widgets/common_internal.hpp"
#include "widgets/mnemonic_internal.hpp"

namespace ckv::widgets {
using detail::is_press;
using detail::stepped;
using detail::enclosing_desktop;
using detail::tail_to_width;
using detail::without_last_grapheme;

namespace {
// The gap between the name column and the value column.
constexpr int kPropertyGutter = 2;

bool property_true(std::string_view value) noexcept { return value == "true"; }

// A bound as the reader typed numbers: whole for an Integer row, the shortest
// exact spelling for a Real one. Locale-free either way.
std::string bound_text(double bound, PropertyKind kind) {
    char buffer[64];
    const auto result = kind == PropertyKind::Integer
                            ? std::to_chars(std::begin(buffer), std::end(buffer), static_cast<long long>(bound))
                            : std::to_chars(std::begin(buffer), std::end(buffer), bound);
    return result.ec == std::errc{} ? std::string(buffer, result.ptr) : std::string{};
}
}  // namespace

PropertyInspector::PropertyInspector() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    // Built once and hidden, one shown at a time over the value being edited.
    // An editor's own callback (a choice picked from its list) may end the
    // edit, which must not destroy the editor while it is still running.
    text_editor_ = make<InputLine>();
    choice_editor_ = make<ComboBox>(ComboBoxMode::PickOnly);
    date_editor_ = make<DatePicker>();
    time_editor_ = make<TimePicker>();
    for (ui::View* editor : {static_cast<ui::View*>(text_editor_), static_cast<ui::View*>(choice_editor_),
                             static_cast<ui::View*>(date_editor_), static_cast<ui::View*>(time_editor_)})
        editor->set_visible(false);
    date_editor_->set_empty_allowed(false);
    // A refused value stays refused only until it is changed: the reason is
    // about the value the reader tried, not the one they are fixing it into.
    text_editor_->on_change = [this] { clear_reason(); };
    choice_editor_->on_text_change = [this](const std::string&) { clear_reason(); };
    date_editor_->on_change = [this](std::optional<DateValue>) { clear_reason(); };
    time_editor_->on_change = [this](TimeValue) { clear_reason(); };
    // A choice from the list is the whole of a Choice edit.
    choice_editor_->on_change = [this](std::size_t) { commit_edit(); };
}
void PropertyInspector::set_items(std::vector<PropertyItem> items) {
    // An edit belongs to the value it began on, and that value is gone.
    cancel_edit();
    clear_reason();
    items_ = std::move(items);
    cursor_ = items_.empty() ? -1 : 0;
    rows_.resize(items_.size());
    rebuild_rows();
    invalidate();
}
void PropertyInspector::set_messages(ui::UiStrings messages) {
    messages_ = std::move(messages);
    custom_messages_ = true;
}
int PropertyInspector::value_x() const {
    int widest = 0;
    for (const PropertyItem& item : items_) widest = std::max(widest, text::text_width(item.name));
    const int names = std::min(widest, std::max(0, (bounds().width - kPropertyGutter) / 2));
    return names + kPropertyGutter;
}
void PropertyInspector::set_presentation(PropertyPresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    rebuild_rows();
    place_editor();
    invalidate();
}
void PropertyInspector::set_banded_rows(bool banded) {
    if (banded_rows_ == banded) return;
    banded_rows_ = banded;
    invalidate();
}
bool PropertyInspector::begins_group(std::size_t index) const noexcept {
    return presentation_ == PropertyPresentation::Sectioned && !items_[index].group.empty() &&
           (index == 0 || items_[index].group != items_[index - 1].group);
}
void PropertyInspector::rebuild_rows() {
    int row = 0;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (begins_group(i)) row += i == 0 ? 1 : 2;
        rows_[i] = row++;
        if (!reason_.empty() && static_cast<int>(i) == reason_index_) ++row;
    }
    content_rows_ = row;
    size_hint_changed();
}
ui::SizeHint PropertyInspector::vertical_size_hint() const {
    return {0, std::max(content_rows_, preferred_size().height), ui::kUnboundedExtent};
}
int PropertyInspector::row_of(std::size_t index) const noexcept { return rows_[index] - scroll_y_; }
int PropertyInspector::item_at_row(int y) const noexcept {
    const int virtual_y = y + scroll_y_;
    const auto found = std::lower_bound(rows_.begin(), rows_.end(), virtual_y);
    return found != rows_.end() && *found == virtual_y ? static_cast<int>(found - rows_.begin()) : -1;
}
void PropertyInspector::set_scroll_y(int scroll_y) {
    const int max_scroll = std::max(0, content_rows_ - std::max(0, bounds().height));
    const int clamped = std::clamp(scroll_y, 0, max_scroll);
    if (clamped == scroll_y_) return;
    scroll_y_ = clamped;
    place_editor();
    invalidate();
}
void PropertyInspector::ensure_cursor_visible() {
    if (cursor_ < 0 || static_cast<std::size_t>(cursor_) >= rows_.size()) return;
    const int h = std::max(0, bounds().height);
    if (h <= 0) return;
    const int virtual_y = rows_[static_cast<std::size_t>(cursor_)];
    if (virtual_y < scroll_y_) {
        set_scroll_y(virtual_y);
    } else if (virtual_y >= scroll_y_ + h) {
        set_scroll_y(virtual_y - h + 1);
    }
}
Rect PropertyInspector::item_bounds(std::size_t index) const noexcept {
    if (index >= items_.size()) return {};
    return Rect{0, row_of(index), std::max(0, bounds().width), 1}.intersected(Rect{0, 0, bounds().width, bounds().height});
}
Rect PropertyInspector::value_bounds(std::size_t index) const {
    const Rect row = item_bounds(index);
    const int x = value_x();
    return Rect{x, row.y, std::max(0, bounds().width - x), 1}.intersected(row);
}
std::optional<PointerShape> PropertyInspector::pointer_shape_at(Point local) const {
    if (!Rect{0, 0, bounds().width, bounds().height}.contains(local)) return std::nullopt;
    const int index = item_at_row(local.y);
    if (index < 0) return std::nullopt;
    if (!enabled_in_tree()) return PointerShape::NotAllowed;
    if (local.x < value_x()) return PointerShape::Default;
    if (!items_[static_cast<std::size_t>(index)].editable) return PointerShape::Default;
    return items_[static_cast<std::size_t>(index)].kind == PropertyKind::Text ||
                   items_[static_cast<std::size_t>(index)].kind == PropertyKind::Integer ||
                   items_[static_cast<std::size_t>(index)].kind == PropertyKind::Real
               ? PointerShape::Text : PointerShape::Pointer;
}
void PropertyInspector::draw(scene::Painter& target) {
    auto painter = target.clipped(Rect{0, 0, bounds().width, bounds().height});
    const bool enabled = enabled_in_tree();
    const Style inert = context().theme->resolve(disabled_role_);
    const Style normal = enabled ? context().theme->resolve(role_) : inert;
    const bool active = has_focus() || (editor_ != nullptr && editor_->has_focus());
    const Style selected = enabled && active
                               ? context().theme->resolve(selected_role_)
                               : accent_style(context().theme->resolve(selected_inactive_role_), normal);
    const int width = bounds().width;
    const int value_column = value_x();
    const int names = std::max(0, value_column - kPropertyGutter);
    painter.fill(Rect{0, 0, width, bounds().height}, Cell::from_grapheme(" ", normal));
    if (presentation_ == PropertyPresentation::Divided)
        painter.vline(Point{names, 0}, bounds().height, scene::LineStyle::Single,
                      accent_style(normal, context().theme->resolve(divider_role_)));
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const int y = row_of(i);
        if (begins_group(i) && y - 1 >= 0 && y - 1 < bounds().height) {
            Style heading = enabled ? accent_style(normal, context().theme->resolve(heading_role_)) : inert;
            heading.attrs |= Attr::Bold;
            painter.draw_text(Point{0, y - 1}, text::clip_to_width_view(items_[i].group, width), heading);
        }
        if (y < 0) continue;
        if (y >= bounds().height) break;
        const bool cursor_row = static_cast<int>(i) == cursor_;
        const Style style = cursor_row ? selected : enabled && banded_rows_ && i % 2
            ? context().theme->resolve(banded_role_) : normal;
        painter.fill(Rect{0, y, width, 1}, Cell::from_grapheme(" ", style));
        painter.draw_text(Point{0, y}, text::clip_to_width_view(items_[i].name, names), style);
        if (presentation_ == PropertyPresentation::Divided)
            painter.vline(Point{names, y}, 1, scene::LineStyle::Single,
                          accent_style(style, context().theme->resolve(divider_role_)));
        if (value_column >= width || (cursor_row && editing())) continue;
        const std::string_view shown = items_[i].kind == PropertyKind::Bool
            ? (property_true(items_[i].value) ? "[X]" : "[ ]") : std::string_view(items_[i].value);
        painter.draw_text(Point{value_column, y}, text::clip_to_width_view(shown, width - value_column), style);
    }
    if (!reason_.empty() && reason_index_ >= 0 && value_column < width) {
        const Style reason = context().theme->resolve(reason_role_);
        const int y = row_of(static_cast<std::size_t>(reason_index_)) + 1;
        painter.fill(Rect{value_column, y, width - value_column, 1}, Cell::from_grapheme(" ", reason));
        painter.draw_text(Point{value_column, y}, text::clip_to_width_view(reason_, width - value_column), reason);
    }
}
void PropertyInspector::place_editor() {
    if (editor_ == nullptr) return;
    editor_->set_bounds(value_bounds(static_cast<std::size_t>(cursor_)));
}
void PropertyInspector::on_resized() {
    set_scroll_y(scroll_y_);
    place_editor();
}
bool PropertyInspector::begin_edit() {
    if (!enabled_in_tree() || editing() || cursor_ < 0 || cursor_ >= static_cast<int>(items_.size())) return false;
    const PropertyItem& item = items_[static_cast<std::size_t>(cursor_)];
    if (!item.editable) return false;
    if (item.kind == PropertyKind::Bool) {
        toggle(static_cast<std::size_t>(cursor_));
        return true;
    }
    ui::Application* const app = context().app;
    if (app == nullptr) return false;
    switch (item.kind) {
        case PropertyKind::Text:
        case PropertyKind::Integer:
        case PropertyKind::Real:
            text_editor_->set_text(item.value);
            text_editor_->set_valid(true);
            editor_ = text_editor_;
            break;
        case PropertyKind::Choice: {
            choice_editor_->set_items(item.choices);
            const auto chosen = std::find(item.choices.begin(), item.choices.end(), item.value);
            choice_editor_->set_selected_index(
                chosen == item.choices.end()
                    ? std::nullopt
                    : std::optional<std::size_t>(static_cast<std::size_t>(chosen - item.choices.begin())));
            editor_ = choice_editor_;
            break;
        }
        case PropertyKind::Date:
            date_editor_->set_value(parse_iso_date(item.value).value_or(DateValue{}));
            date_editor_->set_valid(true);
            // Space and the "▾" drop the calendar, where there is a desktop
            // to drop it on.
            for (ui::View* p = parent(); p != nullptr; p = p->parent())
                if (auto* desktop = dynamic_cast<Desktop*>(p)) {
                    date_editor_->set_calendar_host(*app, *desktop);
                    break;
                }
            editor_ = date_editor_;
            break;
        case PropertyKind::Time: {
            const std::optional<TimeValue> time = parse_iso_time(item.value);
            time_editor_->set_show_seconds(item.value.size() > 5);
            time_editor_->set_value(time.value_or(TimeValue{}));
            time_editor_->set_valid(true);
            editor_ = time_editor_;
            break;
        }
        case PropertyKind::Bool: break;
    }
    clear_reason();
    place_editor();
    editor_->set_visible(true);
    app->set_focus(editor_);
    invalidate();
    // A Choice edit is a choice: the list is what the reader asked for.
    if (editor_ == choice_editor_) choice_editor_->open_dropdown();
    return true;
}
std::string PropertyInspector::editor_text() const {
    if (editor_ == text_editor_) return text_editor_->text();
    if (editor_ == choice_editor_) return choice_editor_->text();
    if (editor_ == date_editor_) return date_editor_->value() ? format_iso_date(*date_editor_->value()) : std::string{};
    if (editor_ == time_editor_) return format_iso_time(time_editor_->value(), time_editor_->show_seconds());
    return {};
}
std::optional<std::string> PropertyInspector::check(const PropertyItem& item, std::string& text) const {
    const auto out_of_range = [&](double value) -> std::optional<std::string> {
        if (item.minimum && value < *item.minimum) return messages_.at_least + " " + bound_text(*item.minimum, item.kind);
        if (item.maximum && value > *item.maximum) return messages_.at_most + " " + bound_text(*item.maximum, item.kind);
        return std::nullopt;
    };
    const char* const first = text.data();
    const char* const last = text.data() + text.size();
    if (item.kind == PropertyKind::Integer) {
        long long value = 0;
        const auto parsed = std::from_chars(first, last, value);
        if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != last) return messages_.whole_number;
        if (auto reason = out_of_range(static_cast<double>(value))) return reason;
        // Exact for every whole number, where going through a double would
        // not be past 2^53.
        char buffer[32];
        const auto written = std::to_chars(std::begin(buffer), std::end(buffer), value);
        text.assign(buffer, written.ptr);
    } else if (item.kind == PropertyKind::Real) {
        double value = 0;
        const auto parsed = std::from_chars(first, last, value);
        if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != last || !std::isfinite(value))
            return messages_.number;
        if (auto reason = out_of_range(value)) return reason;
        text = bound_text(value, PropertyKind::Real);
    }
    if (item.validate) return item.validate(text);
    return std::nullopt;
}
void PropertyInspector::refuse(std::string reason) {
    reason_ = std::move(reason);
    reason_index_ = cursor_;
    rebuild_rows();
    if (editor_ == text_editor_) text_editor_->set_valid(false);
    if (editor_ == date_editor_) date_editor_->set_valid(false);
    if (editor_ == time_editor_) time_editor_->set_valid(false);
    // The reason takes the row under the edited one; the editor stays where
    // it is, and the rows after it move down.
    place_editor();
    invalidate();
}
void PropertyInspector::clear_reason() {
    if (reason_.empty()) return;
    reason_.clear();
    reason_index_ = -1;
    rebuild_rows();
    text_editor_->set_valid(true);
    date_editor_->set_valid(true);
    time_editor_->set_valid(true);
    // The rows the reason pushed down come back up, the editor's with them.
    place_editor();
    invalidate();
}
bool PropertyInspector::commit_edit() {
    if (!editing()) return false;
    const std::size_t index = static_cast<std::size_t>(cursor_);
    std::string text = editor_text();
    if (std::optional<std::string> reason = check(items_[index], text)) {
        refuse(std::move(*reason));
        return false;
    }
    const bool changed = items_[index].value != text;
    items_[index].value = std::move(text);
    close_editor();
    if (changed && on_change) on_change(index, items_[index].value);
    return true;
}
void PropertyInspector::cancel_edit() { close_editor(); }
void PropertyInspector::close_editor() {
    if (editor_ == nullptr) return;
    ui::View* const editor = editor_;
    clear_reason();
    editor_ = nullptr;
    // A popup the editor dropped closes with it, before the keyboard moves:
    // the calendar holds a modal scope of its own, and left open it would go
    // on editing a picker nobody can see.
    if (editor == choice_editor_) choice_editor_->close_dropdown();
    if (editor == date_editor_) date_editor_->close_calendar();
    // The keyboard comes back before the editor goes, so it is never left on
    // a view that is not there to hold it.
    if (context().app != nullptr && (editor->has_focus() || context().app->focused() == nullptr))
        context().app->set_focus(this);
    editor->set_visible(false);
    invalidate();
}
void PropertyInspector::toggle(std::size_t index) {
    PropertyItem& item = items_[index];
    std::string text = property_true(item.value) ? "false" : "true";
    // A refused toggle changes nothing and says why, under the row, until the
    // reader does something else.
    if (item.validate) {
        if (std::optional<std::string> reason = item.validate(text)) {
            reason_ = std::move(*reason);
            reason_index_ = static_cast<int>(index);
            rebuild_rows();
            invalidate();
            return;
        }
    }
    clear_reason();
    item.value = std::move(text);
    invalidate();
    if (on_change) on_change(index, item.value);
}
bool PropertyInspector::move_cursor_to(int index) {
    if (editing() && !commit_edit()) return false;
    clear_reason();
    cursor_ = std::clamp(index, 0, static_cast<int>(items_.size()) - 1);
    ensure_cursor_visible();
    invalidate();
    return true;
}
bool PropertyInspector::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || !is_press(event) || items_.empty()) return false;
    if (editing()) {
        // What the editor left: the keys that end an edit.
        switch (event.chord.key) {
            case Key::Enter: commit_edit(); return true;
            case Key::Escape: cancel_edit(); return true;
            // Leaving the field is a commit, as in a form; a refused value
            // keeps the reader where they can fix it.
            case Key::Tab: return !commit_edit();
            case Key::Up: move_cursor_to(cursor_ - 1); return true;
            case Key::Down: move_cursor_to(cursor_ + 1); return true;
            default: return false;
        }
    }
    const PropertyItem& item = items_[static_cast<std::size_t>(cursor_)];
    switch (event.chord.key) {
        case Key::Down: return move_cursor_to(cursor_ + 1);
        case Key::Up: return move_cursor_to(cursor_ - 1);
        case Key::PageDown: return move_cursor_to(cursor_ + std::max(1, bounds().height - 1));
        case Key::PageUp: return move_cursor_to(cursor_ - std::max(1, bounds().height - 1));
        case Key::Home: return move_cursor_to(0);
        case Key::End: return move_cursor_to(static_cast<int>(items_.size()) - 1);
        case Key::Enter:
        case Key::F2: return begin_edit() || item.editable;
        case Key::Char:
            // Space is the check box's own key.
            if (event.chord.text == " " && item.kind == PropertyKind::Bool && item.editable &&
                event.chord.modifiers == Modifier::None)
                return begin_edit();
            return false;
        default: return false;
    }
}
bool PropertyInspector::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree()) return false;
    if (event.action == MouseAction::Wheel) {
        if (event.button == MouseButton::WheelUp) {
            set_scroll_y(scroll_y_ - ui::kWheelRows);
            return true;
        }
        if (event.button == MouseButton::WheelDown) {
            set_scroll_y(scroll_y_ + ui::kWheelRows);
            return true;
        }
        return false;
    }
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    if (!Rect{0, 0, bounds().width, bounds().height}.contains(local)) return false;
    const int index = item_at_row(local.y);
    if (index < 0) return false;
    if (index != cursor_ && !move_cursor_to(index)) return true;
    // A press on a value asks to change it; on a name, only to point at it.
    if (event.cell.x - abs.x >= value_x() && !editing()) begin_edit();
    return true;
}
void PropertyInspector::on_focus(const FocusEvent&) { invalidate(); }
void PropertyInspector::on_attached() {
    banded_role_ = context().roles->find("ckv.list.banded");
    heading_role_ = context().roles->find("ckv.label.text");
    divider_role_ = context().roles->find("ckv.label.disabled");
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.list.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
    if (selected_inactive_role_ == ui::kInvalidRole)
        selected_inactive_role_ = context().roles->find("ckv.list.selected.inactive");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.list.disabled");
    if (reason_role_ == ui::kInvalidRole) reason_role_ = context().roles->find("ckv.input.invalid");
    if (!custom_messages_ && context().app != nullptr) messages_ = context().app->strings();
}


}  // namespace ckv::widgets
