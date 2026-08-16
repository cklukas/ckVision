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
    editor_.set_context(context());
    editor_.set_role_override(normal_role_, focused_role_, ui::kInvalidRole);
    editor_.on_attached();
    on_resized();
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
    editor_.set_text(text_);
    selected_index_.reset();
    history_index_ = -1;
    invalidate();
    if (on_text_changed) on_text_changed(text_);
}

void ComboBox::sync_text_from_editor() {
    const std::string next = editor_.text();
    if (next == text_) return;
    text_ = next;
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

int ComboBox::visible_rows() const noexcept { return std::max(0, bounds().height - 1); }

void ComboBox::ensure_selection_visible() {
    // A list taller than the space it was given still has to show the item
    // that is selected -- and hand the mouse the same item the eye sees.
    const int rows = visible_rows();
    const int count = static_cast<int>(items_.size());
    if (rows <= 0 || count <= rows) {
        first_visible_ = 0;
        return;
    }
    const int selected = selected_index_ ? static_cast<int>(*selected_index_) : 0;
    if (selected < first_visible_) first_visible_ = selected;
    if (selected >= first_visible_ + rows) first_visible_ = selected - rows + 1;
    first_visible_ = std::clamp(first_visible_, 0, count - rows);
}

void ComboBox::open_dropdown() {
    if (dropdown_open_) return;
    dropdown_open_ = true;
    if (!selected_index_ && !items_.empty()) selected_index_ = 0;
    ensure_selection_visible();
    size_hint_changed();
    invalidate();
}

void ComboBox::close_dropdown() {
    if (!dropdown_open_) return;
    dropdown_open_ = false;
    size_hint_changed();
    invalidate();
}

void ComboBox::select_index(std::size_t index, bool notify) {
    if (index >= items_.size()) return;
    selected_index_ = index;
    ensure_selection_visible();
    text_ = items_[index];
    editor_.set_text(text_);
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
        editor_.set_text(text_);
        selected_index_.reset();
        invalidate();
        if (on_text_changed) on_text_changed(text_);
    } else if (static_cast<std::size_t>(index) < entries.size()) {
        text_ = entries[static_cast<std::size_t>(index)];
        editor_.set_text(text_);
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

ui::SizeHint ComboBox::vertical_size_hint() const {
    // Closed, one row. Open, the row plus the list drawn under it -- what the
    // control needs, not what it currently has.
    const int preferred = dropdown_open_ ? 1 + static_cast<int>(items_.size()) : 1;
    return ui::SizeHint{1, preferred, ui::kUnboundedExtent};
}

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
            if (editable() && !dropdown_open_) {
                if (editor_.on_key(event)) {
                    sync_text_from_editor();
                    return true;
                }
                return false;
            }
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
            if (!editable() || dropdown_open_) return false;
            if (editor_.on_key(event)) {
                sync_text_from_editor();
                return true;
            }
            return false;
        default:
            if (editable() && !dropdown_open_ && editor_.on_key(event)) {
                sync_text_from_editor();
                return true;
            }
            return false;
    }
}

bool ComboBox::on_text(const TextEvent& event) {
    if (!editable() || dropdown_open_ || !editor_.on_text(event)) return false;
    sync_text_from_editor();
    return true;
}

bool ComboBox::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    const int row = event.cell.y - abs.y;
    if (row < 0 || row >= bounds().height) return false;
    if (row == 0) {
        if (editable() && event.cell.x - abs.x < bounds().width - 2) {
            if (editor_.on_mouse(event)) return true;
        }
        dropdown_open_ ? close_dropdown() : open_dropdown();
        return true;
    }
    if (!dropdown_open_) return false;
    const std::size_t index = static_cast<std::size_t>(first_visible_ + row - 1);
    if (index >= items_.size()) return false;
    select_index(index, true);
    close_dropdown();
    return true;
}

void ComboBox::on_focus(const FocusEvent& event) {
    has_focus_ = event.gained;
    editor_.on_focus(event);
    invalidate();
}

void ComboBox::on_resized() {
    ensure_selection_visible();
    const Rect absolute = absolute_bounds();
    editor_.set_bounds(Rect{absolute.x, absolute.y, std::max(0, bounds().width - 2), 1});
}

void ComboBox::draw(scene::Painter& painter) {
    const Style normal = context().theme->resolve(has_focus_ ? focused_role_ : normal_role_);
    const Style selected = context().theme->resolve(selected_role_);
    const Style shadow = context().theme->resolve(shadow_role_);
    const int w = bounds().width;
    if (w <= 0 || bounds().height <= 0) return;

    painter.fill(Rect{0, 0, w, 1}, Cell::from_grapheme(" ", normal));
    if (editable()) {
        const Rect absolute = absolute_bounds();
        editor_.set_bounds(Rect{absolute.x, absolute.y, std::max(0, w - 2), 1});
        scene::Painter editor_painter = painter.translated(Point{0, 0}, Rect{0, 0, std::max(0, w - 2), 1});
        editor_.draw(editor_painter);
    } else {
        painter.draw_text(Point{0, 0}, text::clip_to_width(text_, std::max(0, w - 2)), normal);
    }
    painter.draw_text(Point{w - 1, 0}, dropdown_open_ ? "▴" : "▾", normal);

    if (!dropdown_open_) return;
    const int item_width = std::max(0, w - 1);
    const int max_rows = std::min(static_cast<int>(items_.size()) - first_visible_, visible_rows());
    for (int row = 0; row < max_rows; ++row) {
        const std::size_t item = static_cast<std::size_t>(first_visible_ + row);
        const bool active = selected_index_ && *selected_index_ == item;
        const Style style = active ? selected : normal;
        painter.fill(Rect{0, row + 1, item_width, 1}, Cell::from_grapheme(" ", style));
        painter.draw_text(Point{0, row + 1}, text::clip_to_width(items_[item], item_width), style);
        if (w > 1) painter.draw_text(Point{w - 1, row + 1}, "█", shadow);
    }
}

}  // namespace ckv::widgets
