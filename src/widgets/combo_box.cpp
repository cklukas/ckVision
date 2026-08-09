// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/combo_box.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"

namespace ckv::widgets {

ComboBox::ComboBox(ComboBoxMode mode) : mode_(mode) {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{16, 1});
}

void ComboBox::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
    if (shadow_role_ == ui::kInvalidRole) shadow_role_ = context().roles->find("ckv.button.shadow");
}

void ComboBox::set_items(std::vector<std::string> items) {
    items_ = std::move(items);
    if (selected_index_ && *selected_index_ >= items_.size()) selected_index_.reset();
    if (selected_index_) text_ = items_[*selected_index_];
    invalidate();
}

void ComboBox::set_mode(ComboBoxMode mode) {
    if (mode_ == mode) return;
    mode_ = mode;
    if (!editable() && !selected_index_ && !items_.empty()) select_index(0, false);
    invalidate();
}

void ComboBox::set_text(std::string text) {
    if (text_ == text) return;
    text_ = std::move(text);
    selected_index_.reset();
    history_index_ = -1;
    invalidate();
    if (on_text_changed) on_text_changed(text_);
}

void ComboBox::set_selected_index(std::optional<std::size_t> index) {
    if (!index || *index >= items_.size()) {
        selected_index_.reset();
        invalidate();
        return;
    }
    select_index(*index, false);
}

void ComboBox::set_history(ui::HistoryRegistry* registry, std::string key) {
    history_registry_ = registry;
    history_key_ = std::move(key);
    history_index_ = -1;
}

void ComboBox::commit_to_history() {
    if (history_registry_ == nullptr || text_.empty()) return;
    history_registry_->record(history_key_, text_);
    history_index_ = -1;
}

void ComboBox::open_dropdown() {
    if (dropdown_open_) return;
    dropdown_open_ = true;
    if (!selected_index_ && !items_.empty()) selected_index_ = 0;
    invalidate();
}

void ComboBox::close_dropdown() {
    if (!dropdown_open_) return;
    dropdown_open_ = false;
    invalidate();
}

void ComboBox::select_index(std::size_t index, bool notify) {
    if (index >= items_.size()) return;
    selected_index_ = index;
    text_ = items_[index];
    history_index_ = -1;
    invalidate();
    if (notify && on_select) on_select(index);
    if (on_text_changed) on_text_changed(text_);
}

void ComboBox::move_selection(int delta) {
    if (items_.empty()) return;
    const int current = selected_index_ ? static_cast<int>(*selected_index_) : 0;
    const int next = std::clamp(current + delta, 0, static_cast<int>(items_.size()) - 1);
    select_index(static_cast<std::size_t>(next), false);
}

void ComboBox::recall_history(int index) {
    if (history_registry_ == nullptr) return;
    const auto& entries = history_registry_->entries(history_key_);
    if (history_index_ == -1 && index != -1) history_saved_text_ = text_;
    history_index_ = index;
    if (index == -1) {
        text_ = history_saved_text_;
        selected_index_.reset();
        invalidate();
        if (on_text_changed) on_text_changed(text_);
    } else if (static_cast<std::size_t>(index) < entries.size()) {
        text_ = entries[static_cast<std::size_t>(index)];
        selected_index_.reset();
        invalidate();
        if (on_text_changed) on_text_changed(text_);
    }
}

ui::SizeHint ComboBox::horizontal_size_hint() const {
    int width = 8;
    for (const auto& item : items_) width = std::max(width, text::text_width(item) + 3);
    width = std::max(width, text::text_width(text_) + 3);
    return ui::SizeHint{4, width, ui::kUnboundedExtent};
}

ui::SizeHint ComboBox::vertical_size_hint() const { return ui::SizeHint{1, 1, ui::kUnboundedExtent}; }

bool ComboBox::on_key(const KeyEvent& event) {
    if (event.action == KeyAction::Release) return false;
    switch (event.chord.key) {
        case Key::Down:
            if (editable() && !dropdown_open_ && history_registry_ != nullptr) {
                const auto& entries = history_registry_->entries(history_key_);
                if (!entries.empty()) recall_history(std::min(history_index_ + 1, static_cast<int>(entries.size()) - 1));
                return true;
            }
            if (!dropdown_open_) {
                open_dropdown();
            } else {
                move_selection(1);
            }
            return true;
        case Key::Up:
            if (editable() && !dropdown_open_ && history_registry_ != nullptr) {
                if (history_index_ >= 0) recall_history(history_index_ - 1);
                return true;
            }
            if (!dropdown_open_) {
                open_dropdown();
            } else {
                move_selection(-1);
            }
            return true;
        case Key::Enter:
            if (dropdown_open_) {
                if (selected_index_) select_index(*selected_index_, true);
                close_dropdown();
                return true;
            }
            commit_to_history();
            return true;
        case Key::Escape:
            if (!dropdown_open_) return false;
            close_dropdown();
            return true;
        case Key::Backspace:
            if (!editable() || text_.empty()) return false;
            {
                std::vector<std::string> graphemes;
                for (std::string_view g : text::split_graphemes(text_)) graphemes.emplace_back(g);
                graphemes.pop_back();
                std::string next;
                for (const auto& g : graphemes) next += g;
                set_text(next);
            }
            return true;
        case Key::Char:
            if (!editable() || has_modifier(event.chord.modifiers, Modifier::Alt) ||
                has_modifier(event.chord.modifiers, Modifier::Ctrl) ||
                has_modifier(event.chord.modifiers, Modifier::Super))
                return false;
            set_text(text_ + event.chord.text);
            return true;
        default:
            return false;
    }
}

bool ComboBox::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    const int row = event.cell.y - abs.y;
    if (row < 0 || row >= bounds().height) return false;
    if (row == 0) {
        dropdown_open_ ? close_dropdown() : open_dropdown();
        return true;
    }
    if (!dropdown_open_) return false;
    const std::size_t index = static_cast<std::size_t>(row - 1);
    if (index >= items_.size()) return false;
    select_index(index, true);
    close_dropdown();
    return true;
}

void ComboBox::on_focus(const FocusEvent& event) {
    has_focus_ = event.gained;
    invalidate();
}

void ComboBox::draw(scene::Painter& painter) {
    const Style normal = context().theme->resolve(has_focus_ ? focused_role_ : normal_role_);
    const Style selected = context().theme->resolve(selected_role_);
    const Style shadow = context().theme->resolve(shadow_role_);
    const int w = bounds().width;
    if (w <= 0 || bounds().height <= 0) return;

    painter.fill(Rect{0, 0, w, 1}, Cell::from_grapheme(" ", normal));
    painter.draw_text(Point{0, 0}, text::clip_to_width(text_, std::max(0, w - 2)), normal);
    painter.draw_text(Point{w - 1, 0}, dropdown_open_ ? "▴" : "▾", normal);

    if (!dropdown_open_) return;
    const int item_width = std::max(0, w - 1);
    const int max_rows = std::min(static_cast<int>(items_.size()), std::max(0, bounds().height - 1));
    for (int row = 0; row < max_rows; ++row) {
        const bool active = selected_index_ && *selected_index_ == static_cast<std::size_t>(row);
        const Style style = active ? selected : normal;
        painter.fill(Rect{0, row + 1, item_width, 1}, Cell::from_grapheme(" ", style));
        painter.draw_text(Point{0, row + 1}, text::clip_to_width(items_[static_cast<std::size_t>(row)], item_width),
                          style);
        if (w > 1) painter.draw_text(Point{w - 1, row + 1}, "█", shadow);
    }
}

}  // namespace ckv::widgets
