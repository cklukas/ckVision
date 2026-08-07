// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/list_view.hpp"

#include <algorithm>
#include <cctype>

namespace ckv::widgets {

ListView::ListView(bool multi_select) : multi_select_(multi_select) {
    auto scrollbar = std::make_unique<Scrollbar>(Orientation::Vertical);
    scrollbar_ = static_cast<Scrollbar*>(add_child(std::move(scrollbar)));
    set_focus_policy(ui::FocusPolicy::TabStop);
}

void ListView::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.list.normal");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
}

void ListView::set_items(std::vector<std::string> items) {
    items_ = std::move(items);
    selected_.assign(items_.size(), false);
    cursor_ = items_.empty() ? -1 : 0;
    on_resized();
    ensure_cursor_visible();
    invalidate();
}

bool ListView::is_selected(std::size_t index) const {
    if (index >= selected_.size()) return false;
    return selected_[index];
}

void ListView::select_only(std::size_t index) {
    if (index >= items_.size()) return;
    bool changed = false;
    for (std::size_t i = 0; i < selected_.size(); ++i) {
        const bool want = i == index;
        if (selected_[i] != want) {
            selected_[i] = want;
            changed = true;
        }
    }
    invalidate();
    if (changed && on_selection_changed) on_selection_changed();
}

void ListView::toggle_selected(std::size_t index) { set_selected(index, !is_selected(index)); }

void ListView::set_selected(std::size_t index, bool selected) {
    if (index >= items_.size()) return;
    if (!multi_select_) {
        if (selected) select_only(index);
        return;
    }
    if (selected_[index] == selected) return;
    selected_[index] = selected;
    invalidate();
    if (on_selection_changed) on_selection_changed();
}

std::vector<std::size_t> ListView::selected_indices() const {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < selected_.size(); ++i)
        if (selected_[i]) out.push_back(i);
    return out;
}

void ListView::move_cursor(int new_cursor, bool select_on_move) {
    if (items_.empty()) return;
    cursor_ = std::clamp(new_cursor, 0, static_cast<int>(items_.size()) - 1);
    ensure_cursor_visible();
    if (select_on_move) select_only(static_cast<std::size_t>(cursor_));
    invalidate();
}

void ListView::ensure_cursor_visible() {
    if (cursor_ < 0 || scrollbar_ == nullptr) return;
    if (cursor_ < scrollbar_->position()) {
        scrollbar_->set_position(cursor_);
    } else if (cursor_ >= scrollbar_->position() + scrollbar_->viewport_size()) {
        scrollbar_->set_position(cursor_ - scrollbar_->viewport_size() + 1);
    }
}

void ListView::on_resized() {
    if (scrollbar_ == nullptr) return;
    scrollbar_->set_bounds(Rect{std::max(0, bounds().width - 1), 0, std::min(1, bounds().width), bounds().height});
    scrollbar_->set_range(static_cast<int>(items_.size()), std::max(1, bounds().height));
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
            move_cursor(static_cast<int>(items_.size()) - 1, !multi_select_);
            return true;
        case Key::Enter:
            if (cursor_ >= 0 && on_activate) on_activate(static_cast<std::size_t>(cursor_));
            return true;
        case Key::Char:
            if (event.chord.text == " ") {
                if (cursor_ >= 0) {
                    if (multi_select_)
                        toggle_selected(static_cast<std::size_t>(cursor_));
                    else
                        select_only(static_cast<std::size_t>(cursor_));
                }
                return true;
            }
            if (!event.chord.text.empty() && !items_.empty()) {
                const char query = static_cast<char>(std::tolower(static_cast<unsigned char>(event.chord.text[0])));
                const std::size_t n = items_.size();
                for (std::size_t step = 1; step <= n; ++step) {
                    const std::size_t i = (static_cast<std::size_t>(cursor_) + step) % n;
                    if (!items_[i].empty() &&
                        std::tolower(static_cast<unsigned char>(items_[i][0])) == query) {
                        move_cursor(static_cast<int>(i), !multi_select_);
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
    if (index < 0 || static_cast<std::size_t>(index) >= items_.size()) return false;

    if (index == cursor_) {
        if (on_activate) on_activate(static_cast<std::size_t>(index));
    } else {
        move_cursor(index, !multi_select_);
        if (multi_select_) select_only(static_cast<std::size_t>(index));
    }
    return true;
}

void ListView::draw(scene::Painter& painter) {
    const int visible_width = std::max(0, bounds().width - 1);
    const int top = scrollbar_ != nullptr ? scrollbar_->position() : 0;
    for (int row = 0; row < bounds().height; ++row) {
        const std::size_t index = static_cast<std::size_t>(top + row);
        const Style style = (index < items_.size() && is_selected(index)) ? context().theme->resolve(selected_role_)
                                                                            : context().theme->resolve(normal_role_);
        painter.fill(Rect{0, row, visible_width, 1}, Cell::from_grapheme(" ", style));
        if (index < items_.size()) painter.draw_text(Point{0, row}, items_[index], style);
    }
}

}  // namespace ckv::widgets
