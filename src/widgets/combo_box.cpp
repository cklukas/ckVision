// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/combo_box.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/desktop.hpp"

namespace ckv::widgets {

ComboBox::ComboBox(ComboBoxMode mode) : mode_(mode) {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{16, 1});
}

void ComboBox::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");

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

void ComboBox::open_dropdown() {
    if (popup_ != nullptr) return;
    ui::Application* const app = context().app;
    Desktop* desktop = nullptr;
    for (ui::View* p = parent(); p != nullptr; p = p->parent())
        if (auto* d = dynamic_cast<Desktop*>(p)) {
            desktop = d;
            break;
        }
    // Without a desktop there is nowhere for a popup to float, and drawing
    // the list inside this control instead would push everything beside it
    // around. The arrows still move the selection, so the control works.
    if (app == nullptr || desktop == nullptr || items_.empty()) return;
    if (!selected_index_) selected_index_ = 0;  // a list opens on something
    popup_ = show_popup_list(
        absolute_bounds(), items_, selected_index_, *app, *desktop,
        [this](std::size_t index) {
            popup_ = nullptr;
            select_index(index, true);
        },
        [this] { popup_ = nullptr; invalidate(); });
    invalidate();
}

void ComboBox::close_dropdown() {
    if (popup_ == nullptr) return;
    PopupList* const popup = popup_;
    popup_ = nullptr;
    popup->request_dismiss();  // closing is not choosing
    invalidate();
}

void ComboBox::select_index(std::size_t index, bool notify) {
    if (index >= items_.size()) return;
    selected_index_ = index;
    text_ = items_[index];
    editor_.set_text(text_);
    history_index_ = -1;
    invalidate();
    if (notify && on_select) on_select(index);
    if (on_text_changed) on_text_changed(text_);
}

void ComboBox::move_selection(int delta) {
    if (items_.empty()) return;
    // From nothing chosen, a step lands on the first item rather than
    // stepping past it.
    if (!selected_index_) {
        select_index(0, false);
        return;
    }
    const int current = static_cast<int>(*selected_index_);
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

// One row, open or closed: the list is a popup over the surface, not part of
// this control's own box.
ui::SizeHint ComboBox::vertical_size_hint() const { return ui::SizeHint{1, 1, ui::kUnboundedExtent}; }

bool ComboBox::on_key(const KeyEvent& event) {
    if (event.action == KeyAction::Release) return false;
    switch (event.chord.key) {
        case Key::Down:
            if (editable() && !dropdown_open() && history_registry_ != nullptr) {
                const auto& entries = history_registry_->entries(history_key_);
                if (!entries.empty()) recall_history(std::min(history_index_ + 1, static_cast<int>(entries.size()) - 1));
                return true;
            }
            open_dropdown();
            // Nowhere to drop a list: the arrows step through the items in
            // place, so the control is still usable.
            if (!dropdown_open()) move_selection(1);
            return true;
        case Key::Up:
            if (editable() && !dropdown_open() && history_registry_ != nullptr) {
                if (history_index_ >= 0) recall_history(history_index_ - 1);
                return true;
            }
            open_dropdown();
            if (!dropdown_open()) move_selection(-1);
            return true;
        case Key::Enter:
            // An open list has the keys -- it is focused and holds the mouse
            // -- so Enter here is always the closed control's.
            //
            // It records the value and then does NOT claim the key. A closed
            // combo has nothing to confirm: the value is already chosen, and
            // Enter in a form means "accept the form". Claiming it left a
            // dialog's default button unreachable from the keyboard for as
            // long as any combo had focus -- which, in a settings dialog that
            // opens on one, is from the moment it appears. CheckGroup and
            // RadioGroup had the same defect and were fixed the same way.
            commit_to_history();
            return false;
        case Key::Escape:
            return false;  // an open list closes itself; a closed one has nothing to close
        case Key::Backspace:
            if (editable() && !dropdown_open()) {
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
            if (!editable() || dropdown_open()) return false;
            if (editor_.on_key(event)) {
                sync_text_from_editor();
                return true;
            }
            return false;
        default:
            if (editable() && !dropdown_open() && editor_.on_key(event)) {
                sync_text_from_editor();
                return true;
            }
            return false;
    }
}

bool ComboBox::on_text(const TextEvent& event) {
    if (!editable() || dropdown_open() || !editor_.on_text(event)) return false;
    sync_text_from_editor();
    return true;
}

bool ComboBox::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    const int row = event.cell.y - abs.y;
    if (row != 0) return false;  // one row: the list is a popup, not a part of this
    if (editable() && event.cell.x - abs.x < bounds().width - 2) {
        if (editor_.on_mouse(event)) return true;
    }
    dropdown_open() ? close_dropdown() : open_dropdown();
    return true;
}

void ComboBox::on_focus(const FocusEvent& event) {
    has_focus_ = event.gained;
    editor_.on_focus(event);
    invalidate();
}

void ComboBox::on_resized() {
    const Rect absolute = absolute_bounds();
    editor_.set_bounds(Rect{absolute.x, absolute.y, std::max(0, bounds().width - 2), 1});
}

void ComboBox::draw(scene::Painter& painter) {
    const Style normal = context().theme->resolve(has_focus_ ? focused_role_ : normal_role_);
    const int w = bounds().width;
    if (w <= 0 || bounds().height <= 0) return;

    painter.fill(Rect{0, 0, w, 1}, Cell::from_grapheme(" ", normal));
    if (editable()) {
        const Rect absolute = absolute_bounds();
        editor_.set_bounds(Rect{absolute.x, absolute.y, std::max(0, w - 2), 1});
        scene::Painter editor_painter = painter.translated(Point{0, 0}, Rect{0, 0, std::max(0, w - 2), 1});
        editor_.draw(editor_painter);
    } else {
        // Up to the arrow, not one cell short of it: a combo given exactly
        // the room its longest item needs should show that item, not clip it
        // to keep a blank nobody asked for.
        painter.draw_text(Point{0, 0}, text::clip_to_width(text_, std::max(0, w - 1)), normal);
    }
    painter.draw_text(Point{w - 1, 0}, dropdown_open() ? "▴" : "▾", normal);
}

}  // namespace ckv::widgets
