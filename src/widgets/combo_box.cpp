// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/input_presentation_internal.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/desktop.hpp"

namespace ckv::widgets {

ComboBox::ComboBox(ComboBoxMode mode) : mode_(mode) {
    set_focus_policy(editable() ? ui::FocusPolicy::None : ui::FocusPolicy::TabStop);
    set_preferred_size(Size{16, 1});
    editor_ = make<InputLine>();
    editor_->set_visible(editable());
    editor_->on_edited = [this] { sync_text_from_editor(); };
}

void ComboBox::on_attached() {
    if (normal_role_ == ui::kInvalidRole) normal_role_ = context().roles->find("ckv.input.normal");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.input.focused");
    if (selected_role_ == ui::kInvalidRole) selected_role_ = context().roles->find("ckv.list.selected");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.input.disabled");

    accessory_role_ = context().roles->find("ckv.input.accessory");
    accessory_hovered_role_ = context().roles->find("ckv.input.accessory.hovered");
    editor_->set_role_override(normal_role_, focused_role_, ui::kInvalidRole);
    on_resized();
}

void ComboBox::set_presentation(InputPresentation presentation) {
    if (presentation_ == presentation) return;
    close_dropdown();
    presentation_ = presentation;
    hover_position_.reset();
    on_resized();
    size_hint_changed();
    invalidate();
}

Rect ComboBox::value_bounds() const noexcept {
    Rect content = content_bounds();
    const int accessory = presentation_ == InputPresentation::Flat && !editable() ? 1 : 2;
    content.width = std::max(0, content.width - accessory);
    return content;
}

void ComboBox::set_items(std::vector<std::string> items) {
    items_ = std::move(items);
    if (selected_index_ && *selected_index_ >= items_.size()) selected_index_.reset();
    if (selected_index_) {
        // The field is what an editable combo shows and what the next key
        // edits, so the selected item's new text has to reach it as well.
        text_ = items_[*selected_index_];
        editor_->set_text(text_);
    }
    invalidate();
}

void ComboBox::set_mode(ComboBoxMode mode) {
    if (mode_ == mode) return;
    const bool owned_focus = has_focus() || editor_->has_focus();
    close_dropdown();
    mode_ = mode;
    set_focus_policy(editable() ? ui::FocusPolicy::None : ui::FocusPolicy::TabStop);
    editor_->set_visible(editable());
    if (owned_focus && context().app) context().app->set_focus(&focus_target());
    if (!editable() && !selected_index_ && !items_.empty()) select_index(0, false);
    on_resized();
    size_hint_changed();
    invalidate();
}

void ComboBox::set_text(std::string text) {
    if (text_ == text) return;
    text_ = std::move(text);
    editor_->set_text(text_);
    selected_index_.reset();
    history_index_ = -1;
    invalidate();
    if (on_text_changed) on_text_changed(text_);
}

void ComboBox::sync_text_from_editor() {
    const std::string next = editor_->text();
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

void ComboBox::set_history_key(std::string key) {
    history_key_ = std::move(key);
    history_index_ = -1;
}

ui::HistoryRegistry* ComboBox::history_registry() const noexcept {
    if (history_key_.empty() || context().app == nullptr) return nullptr;
    return &context().app->history();
}

void ComboBox::commit_to_history() {
    ui::HistoryRegistry* const registry = history_registry();
    if (registry == nullptr || text_.empty()) return;
    registry->record(history_key_, text_);
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
    // A list opens on something -- the selection, or else its first row --
    // but opening chooses nothing: the selection changes only when a row is
    // chosen, so it never names an item the text does not hold.
    popup_ = show_popup_list(
        absolute_bounds(), items_, selected_index_.value_or(0), *app, *desktop,
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
    editor_->set_text(text_);
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
    const ui::HistoryRegistry* const registry = history_registry();
    if (registry == nullptr) return;
    const auto& entries = registry->entries(history_key_);
    if (history_index_ == -1 && index != -1) history_saved_text_ = text_;
    history_index_ = index;
    if (index == -1) {
        text_ = history_saved_text_;
        editor_->set_text(text_);
        selected_index_.reset();
        invalidate();
        if (on_text_changed) on_text_changed(text_);
    } else if (static_cast<std::size_t>(index) < entries.size()) {
        text_ = entries[static_cast<std::size_t>(index)];
        editor_->set_text(text_);
        selected_index_.reset();
        invalidate();
        if (on_text_changed) on_text_changed(text_);
    }
}

ui::SizeHint ComboBox::horizontal_size_hint() const {
    int width = 8;
    for (const auto& item : items_) width = std::max(width, text::text_width(item) + 3);
    width = std::max(width, text::text_width(text_) + 3);
    const int padding = presentation_ == InputPresentation::Flat ? 0 : 2;
    return ui::SizeHint{4 + padding, width + padding, ui::kUnboundedExtent};
}

// Opening the floating list never changes the requested field height.
ui::SizeHint ComboBox::vertical_size_hint() const {
    const int height = input_presentation_height(presentation_);
    return ui::SizeHint{height, height, height};
}

bool ComboBox::on_key(const KeyEvent& event) {
    if (event.action == KeyAction::Release) return false;
    switch (event.chord.key) {
        case Key::Down:
            if (const ui::HistoryRegistry* const history = history_registry();
                editable() && !dropdown_open() && history != nullptr) {
                const auto& entries = history->entries(history_key_);
                if (!entries.empty()) recall_history(std::min(history_index_ + 1, static_cast<int>(entries.size()) - 1));
                return true;
            }
            open_dropdown();
            // Nowhere to drop a list: the arrows step through the items in
            // place, so the control is still usable.
            if (!dropdown_open()) move_selection(1);
            return true;
        case Key::Up:
            if (editable() && !dropdown_open() && history_registry() != nullptr) {
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
                if (editor_->on_key(event)) {
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
            if (editor_->on_key(event)) {
                sync_text_from_editor();
                return true;
            }
            return false;
        default:
            if (editable() && !dropdown_open() && editor_->on_key(event)) {
                sync_text_from_editor();
                return true;
            }
            return false;
    }
}

bool ComboBox::on_text(const TextEvent& event) {
    if (!editable() || dropdown_open() || !editor_->on_text(event)) return false;
    sync_text_from_editor();
    return true;
}

bool ComboBox::on_mouse(const MouseEvent& event) {
    const Rect absolute = absolute_bounds();
    const Point local{event.cell.x - absolute.x, event.cell.y - absolute.y};
    if (event.action == MouseAction::Move) { hover_position_ = local; invalidate(); return false; }
    if (!enabled_in_tree() || event.action != MouseAction::Down || event.button != MouseButton::Left || !content_bounds().contains(local)) return false;
    if (editable()) {
        if (value_bounds().contains(local)) return editor_->on_mouse(event);
        const Rect content = content_bounds();
        if (local.x != content.right() - 1) return false;
    }
    dropdown_open() ? close_dropdown() : open_dropdown();
    return true;
}

std::optional<PointerShape> ComboBox::pointer_shape_at(Point local) const {
    const Rect content = content_bounds();
    if (!content.contains(local)) return std::nullopt;
    if (!enabled_in_tree()) return PointerShape::NotAllowed;
    if (!editable()) return PointerShape::Pointer;
    if (value_bounds().contains(local)) return PointerShape::Text;
    return local.x == content.right() - 1 ? std::optional{PointerShape::Pointer} : std::nullopt;
}

void ComboBox::on_hover_changed(bool hovered) {
    if (!hovered) hover_position_.reset();
    invalidate();
}

void ComboBox::on_focus(const FocusEvent&) { invalidate(); }

void ComboBox::on_resized() {
    hover_position_.reset();
    editor_->set_bounds(value_bounds());
    editor_->set_presentation(InputPresentation::Flat);
}

void ComboBox::draw(scene::Painter& painter) {
    const bool enabled = enabled_in_tree();
    const bool focused = has_focus() || editor_->has_focus();
    const Style normal = context().theme->resolve(!enabled ? disabled_role_ : focused ? focused_role_ : normal_role_);
    detail::draw_input_surface(painter, Size{bounds().width, bounds().height}, presentation_, normal, enabled && focused);
    const Rect content = content_bounds();
    if (content.empty()) return;
    auto clipped = painter.clipped(content);
    if (!editable()) {
        const Rect value = value_bounds();
        clipped.draw_text(Point{value.x, value.y}, text::clip_to_width_view(text_, value.width), normal);
    }
    const int arrow_x = content.right() - 1;
    const bool hover = hover_position_ && content.contains(*hover_position_) && hover_position_->x == arrow_x;
    const Style accessory = context().theme->resolve(!enabled ? disabled_role_ : hover ? accessory_hovered_role_ : accessory_role_);
    if (presentation_ != InputPresentation::Flat && content.width > 1)
        clipped.draw_text(Point{arrow_x - 1, 0}, "│", normal);
    clipped.draw_text(Point{arrow_x, 0}, dropdown_open() ? "▴" : "▾", accessory);
}

}  // namespace ckv::widgets
