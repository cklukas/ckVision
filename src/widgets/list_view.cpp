// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/list_view.hpp"

#include "cvision/core/text.hpp"

#include <algorithm>
#include <cctype>

#include "cvision/ui/application.hpp"

namespace ckv::widgets {

ListView::ListView(bool multi_select) : multi_select_(multi_select) {
    scrollbar_ = make<Scrollbar>(Orientation::Vertical);
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void ListView::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.list.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
    if (selected_inactive_role_ == ui::kInvalidRole)
        selected_inactive_role_ = context().roles->find("ckv.list.selected.inactive");
}

void ListView::on_focus(const FocusEvent& event) {
    if (focused_ == event.gained) return;
    focused_ = event.gained;
    // The selection changes appearance, so the rows have to be repainted even
    // though nothing about the model moved.
    invalidate();
}

void ListView::set_model(ListModel& model) {
    model_ = &model;
    items_.clear();
    selected_ids_.clear();
    cursor_id_ = kInvalidListItemId;
    cursor_ = -1;
    last_click_index_ = -1;
    last_click_nanos_ = -1;
    resolve_model_identities();
    on_resized();
    invalidate();
}

void ListView::clear_model() {
    model_ = nullptr;
    selected_ids_.clear();
    cursor_id_ = kInvalidListItemId;
    cursor_ = -1;
    if (scrollbar_ != nullptr) scrollbar_->set_range(0, std::max(1, bounds().height));
    invalidate();
}

void ListView::model_changed() {
    resolve_model_identities();
    on_resized();
    ensure_cursor_visible();
    invalidate();
}

void ListView::set_items(std::vector<std::string> items) {
    model_ = nullptr;
    items_ = std::move(items);
    selected_ids_.clear();
    cursor_id_ = items_.empty() ? kInvalidListItemId : 1;
    cursor_ = items_.empty() ? -1 : 0;
    last_click_index_ = -1;
    last_click_nanos_ = -1;
    on_resized();
    ensure_cursor_visible();
    invalidate();
}

std::size_t ListView::item_count() const { return model_ != nullptr ? model_->item_count() : items_.size(); }

ListItem ListView::item_at(std::size_t index) const {
    if (model_ != nullptr) return model_->item_at(index);
    if (index >= items_.size()) return {};
    return ListItem{static_cast<ListItemId>(index + 1), items_[index], std::nullopt};
}

std::optional<std::size_t> ListView::index_of(ListItemId id) const {
    if (id == kInvalidListItemId) return std::nullopt;
    if (model_ != nullptr) return model_->index_of(id);
    const std::size_t index = static_cast<std::size_t>(id - 1);
    return index < items_.size() ? std::optional<std::size_t>(index) : std::nullopt;
}

ListItemId ListView::id_at(std::size_t index) const {
    return index < item_count() ? item_at(index).id : kInvalidListItemId;
}

std::optional<ListItemId> ListView::cursor_id() const noexcept {
    return cursor_id_ == kInvalidListItemId ? std::nullopt : std::optional<ListItemId>(cursor_id_);
}

ui::SizeHint ListView::horizontal_size_hint() const {
    // The widest of the items worth measuring, plus the room the scrollbar takes
    // when it is there. A minimum of a few columns so an empty list is still a
    // list rather than a sliver.
    const std::size_t count = item_count();
    const std::size_t measured = std::min(count, kMeasuredItemsForWidth);
    int widest = 0;
    for (std::size_t index = 0; index < measured; ++index)
        widest = std::max(widest, static_cast<int>(text::text_width(item_at(index).text)));
    const int scrollbar_columns = scrollbar_policy() == ScrollbarPolicy::Hidden ? 0 : 1;
    // Two columns of padding: a list whose text touches both edges reads as
    // clipped even when it is not.
    const int preferred = widest + scrollbar_columns + 2;
    return ui::SizeHint{std::min(8, preferred), std::max(8, preferred), ui::kUnboundedExtent};
}

ui::SizeHint ListView::vertical_size_hint() const {
    const std::size_t count = item_count();
    const int rows = static_cast<int>(std::clamp<std::size_t>(count, 1, kPreferredVisibleRows));
    // One row is the floor rather than zero: a container that gave a list no
    // height at all would be showing a reader an empty box and no way to know
    // whether it means "nothing here" or "not enough room".
    return ui::SizeHint{1, rows, ui::kUnboundedExtent};
}

bool ListView::contains_selected(ListItemId id) const noexcept {
    return std::find(selected_ids_.begin(), selected_ids_.end(), id) != selected_ids_.end();
}

bool ListView::is_selected_id(ListItemId id) const {
    return id != kInvalidListItemId && contains_selected(id);
}

bool ListView::is_selected(std::size_t index) const { return is_selected_id(id_at(index)); }

void ListView::notify_selection(ListItemId id) {
    if (on_selection_changed_id) on_selection_changed_id(id);
    if (on_selection_changed) {
        if (const auto index = index_of(id)) on_selection_changed(*index);
    }
}

void ListView::select_only(std::size_t index) {
    const ListItemId id = id_at(index);
    if (id == kInvalidListItemId) return;
    const bool changed = selected_ids_.size() != 1 || selected_ids_.front() != id;
    selected_ids_.assign(1, id);
    invalidate();
    if (changed) notify_selection(id);
}

void ListView::toggle_selected(ListItemId id) { set_selected_id(id, !is_selected_id(id)); }

void ListView::set_cursor(std::size_t index) {
    if (index >= item_count()) return;
    move_cursor(static_cast<int>(index), /*select_on_move=*/!multi_select_);
}

void ListView::set_selected(std::size_t index, bool selected) { set_selected_id(id_at(index), selected); }

void ListView::set_selected_id(ListItemId id, bool selected) {
    if (id == kInvalidListItemId || !index_of(id)) return;
    if (!multi_select_) {
        if (selected) select_only(*index_of(id));
        return;
    }
    const auto it = std::find(selected_ids_.begin(), selected_ids_.end(), id);
    if ((it != selected_ids_.end()) == selected) return;
    if (selected)
        selected_ids_.push_back(id);
    else
        selected_ids_.erase(it);
    invalidate();
    notify_selection(id);
}

std::vector<std::size_t> ListView::selected_indices() const {
    std::vector<std::size_t> out;
    for (ListItemId id : selected_ids_)
        if (const auto index = index_of(id)) out.push_back(*index);
    return out;
}

void ListView::move_cursor(int new_cursor, bool select_on_move) {
    const std::size_t count = item_count();
    if (count == 0) return;
    const int previous = cursor_;
    cursor_ = std::clamp(new_cursor, 0, static_cast<int>(count) - 1);
    cursor_id_ = id_at(static_cast<std::size_t>(cursor_));
    ensure_cursor_visible();
    if (select_on_move) select_only(static_cast<std::size_t>(cursor_));
    invalidate();
    // After the move is complete, so a listener that reads the list back
    // sees where it now is; and only on a real move, so a listener may
    // do real work without checking whether anything changed.
    if (cursor_ != previous && on_cursor_changed)
        on_cursor_changed(static_cast<std::size_t>(cursor_));
}

void ListView::ensure_cursor_visible() {
    if (cursor_ < 0 || scrollbar_ == nullptr) return;
    if (cursor_ < scrollbar_->position()) {
        scrollbar_->set_position(cursor_);
    } else if (cursor_ >= scrollbar_->position() + scrollbar_->viewport_size()) {
        scrollbar_->set_position(cursor_ - scrollbar_->viewport_size() + 1);
    }
}

void ListView::set_scrollbar_role_override(ui::RoleId track_role, ui::RoleId thumb_role) noexcept {
    // Applied through the bar itself rather than remembered here: Scrollbar
    // already owns this exact override, and a second copy of the state
    // would be one more thing to keep in step with it.
    if (scrollbar_ != nullptr) scrollbar_->set_role_override(track_role, thumb_role);
}

void ListView::set_scrollbar_policy(ScrollbarPolicy policy) {
    if (scrollbar_ != nullptr) scrollbar_->set_policy(policy);
}

ScrollbarPolicy ListView::scrollbar_policy() const noexcept {
    return scrollbar_ != nullptr ? scrollbar_->policy() : ScrollbarPolicy::Always;
}

void ListView::on_resized() {
    if (scrollbar_ == nullptr) return;
    scrollbar_->set_bounds(Rect{std::max(0, bounds().width - 1), 0, std::min(1, bounds().width), bounds().height});
    scrollbar_->set_range(static_cast<int>(item_count()), std::max(1, bounds().height));
}

bool ListView::on_key(const KeyEvent& event) {
    switch (event.chord.key) {
        case Key::Up:
            move_cursor(cursor_ - 1, !multi_select_);
            return true;
        case Key::Down:
            move_cursor(cursor_ + 1, !multi_select_);
            return true;
        case Key::PageUp:
            move_cursor(cursor_ - std::max(1, bounds().height), !multi_select_);
            return true;
        case Key::PageDown:
            move_cursor(cursor_ + std::max(1, bounds().height), !multi_select_);
            return true;
        case Key::Home:
            move_cursor(0, !multi_select_);
            return true;
        case Key::End:
            move_cursor(static_cast<int>(item_count()) - 1, !multi_select_);
            return true;
        case Key::Enter:
            if (cursor_ >= 0) {
                if (on_activate_id) on_activate_id(cursor_id_);
                if (on_activate) on_activate(static_cast<std::size_t>(cursor_));
            }
            return true;
        case Key::Char:
            if (event.chord.text == " ") {
                if (cursor_ >= 0) {
                    if (multi_select_)
                        toggle_selected(cursor_id_);
                    else
                        select_only(static_cast<std::size_t>(cursor_));
                }
                return true;
            }
            if (!event.chord.text.empty() && item_count() != 0) {
                const char query = static_cast<char>(std::tolower(static_cast<unsigned char>(event.chord.text[0])));
                const std::string folded(1, query);
                if (model_ != nullptr) {
                    const auto found = model_->find_prefix(folded, static_cast<std::size_t>(std::max(0, cursor_)));
                    if (!found || *found >= item_count()) return false;
                    move_cursor(static_cast<int>(*found), !multi_select_);
                    return true;
                }
                const std::size_t n = items_.size();
                for (std::size_t step = 1; step <= n; ++step) {
                    const std::size_t index = (static_cast<std::size_t>(cursor_) + step) % n;
                    if (!items_[index].empty() && std::tolower(static_cast<unsigned char>(items_[index][0])) == query) {
                        move_cursor(static_cast<int>(index), !multi_select_);
                        return true;
                    }
                }
            }
            return false;
        default:
            return false;
    }
}

bool ListView::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down) return false;
    const Rect abs = absolute_bounds();
    const int row = event.cell.y - abs.y;
    if (row < 0 || row >= bounds().height) return false;
    const int index = scrollbar_->position() + row;
    if (index < 0 || static_cast<std::size_t>(index) >= item_count()) return false;

    const std::int64_t now = context().app != nullptr ? context().app->clock().now_nanos() : -1;
    const bool double_click = now >= 0 && last_click_nanos_ >= 0 && index == last_click_index_ &&
                              now - last_click_nanos_ <= kDoubleClickIntervalNanos;
    last_click_index_ = index;
    last_click_nanos_ = now;

    if (double_click) {
        const ListItemId id = id_at(static_cast<std::size_t>(index));
        if (on_activate_id) on_activate_id(id);
        if (on_activate) on_activate(static_cast<std::size_t>(index));
    } else {
        move_cursor(index, !multi_select_);
        if (multi_select_) select_only(static_cast<std::size_t>(index));
    }
    return true;
}

void ListView::draw(scene::Painter& painter) {
    // A row is painted across the whole list, last column included. The bar
    // is a child and paints after this, over the column it occupies, so
    // stopping a column short here does not make room for it — it only
    // leaves a hole, and when the policy is Auto and there is nothing to
    // scroll, nothing ever covers that hole. Whatever is behind the list
    // shows through it, which in a dialog is a one-column notch down the
    // right edge that does not line up with the widget above or below.
    const int top = scrollbar_ != nullptr ? scrollbar_->position() : 0;
    const std::size_t count = item_count();
    for (int row = 0; row < bounds().height; ++row) {
        const std::size_t index = static_cast<std::size_t>(top + row);
        const ListItem item = index < count ? item_at(index) : ListItem{};
        // The cursor is the active row for a single-select list even before
        // the user makes an explicit selection.  Treat it as selected for
        // painting so a newly presented list has an unambiguous focus row.
        // Multi-select lists retain their independently selected rows while
        // also showing the current navigation cursor.
        // A selected row wears the full highlight only while this list holds
        // the keyboard. Two lists on screen otherwise look identically
        // active, and the reader cannot tell which one their arrow keys will
        // move; the muted form still says "this list's place is here".
        const ui::RoleId selection_role =
            focused_ || selected_inactive_role_ == ui::kInvalidRole ? selected_role_
                                                                    : selected_inactive_role_;
        Style style = item.id != kInvalidListItemId &&
                                  (item.id == cursor_id_ || is_selected_id(item.id))
                          ? context().theme->resolve(selection_role)
                          : context().theme->resolve(normal_role_);
        if (item.style) style = *item.style;
        painter.fill(Rect{0, row, bounds().width, 1}, Cell::from_grapheme(" ", style));
        if (item.id != kInvalidListItemId) painter.draw_text(Point{0, row}, item.text, style);
    }
}

void ListView::resolve_model_identities() {
    const std::size_t count = item_count();
    selected_ids_.erase(std::remove_if(selected_ids_.begin(), selected_ids_.end(),
                                       [this](ListItemId id) { return !index_of(id).has_value(); }),
                        selected_ids_.end());
    if (count == 0) {
        cursor_id_ = kInvalidListItemId;
        cursor_ = -1;
        return;
    }
    if (const auto index = index_of(cursor_id_)) {
        cursor_ = static_cast<int>(*index);
        return;
    }
    cursor_ = 0;
    cursor_id_ = id_at(0);
}

}  // namespace ckv::widgets
