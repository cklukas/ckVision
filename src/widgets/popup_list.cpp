// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/popup_list.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/desktop.hpp"

namespace ckv::widgets {

namespace {
// One blank cell either side of an item, as a dropdown menu pads its own.
constexpr int kItemPadding = 1;
constexpr int kFrame = 1;
}  // namespace

PopupList::PopupList(std::vector<std::string> items, std::optional<std::size_t> selected)
    : items_(std::move(items)), selected_(selected) {
    set_focus_policy(ui::FocusPolicy::None);
    auto list = std::make_unique<ListView>();
    list->set_items(items_);
    // A bar only where the list is longer than the room it got: in a popup
    // sized to its own items, that is the exception rather than the rule, and
    // a bar with nothing to scroll would take a column from every item.
    list->set_scrollbar_policy(ScrollbarPolicy::Auto);
    list_ = static_cast<ListView*>(add_child(std::move(list)));
    // Enter, and a row that was clicked, are the same statement.
    list_->on_activate = [this](std::size_t index) { choose(index); };
}

void PopupList::on_attached() {
    if (frame_role_ == ui::kInvalidRole) frame_role_ = context().roles->find("ckv.menu.dropdown.normal");
    // A dropdown is a dropdown: the list inside it wears the menu's colours
    // rather than a list widget's, so the two popups do not read as two
    // different kinds of thing.
    list_->set_role_override(frame_role_, context().roles->find("ckv.menu.dropdown.highlighted"));
    list_->set_selected_inactive_role_override(context().roles->find("ckv.menu.dropdown.highlighted"));
    if (selected_) list_->set_cursor(*selected_);
}

ui::SizeHint PopupList::horizontal_size_hint() const {
    int widest = 0;
    for (const std::string& item : items_) widest = std::max(widest, text::text_width(item));
    const int width = widest + 2 * kItemPadding + 2 * kFrame;
    return ui::SizeHint{2 * kFrame + 1, width, ui::kUnboundedExtent};
}

ui::SizeHint PopupList::vertical_size_hint() const {
    const int height = static_cast<int>(items_.size()) + 2 * kFrame;
    return ui::SizeHint{2 * kFrame + 1, height, ui::kUnboundedExtent};
}

void PopupList::on_resized() {
    list_->set_bounds(Rect{kFrame + kItemPadding, kFrame,
                           std::max(0, bounds().width - 2 * kFrame - kItemPadding),
                           std::max(0, bounds().height - 2 * kFrame)});
}

void PopupList::draw(scene::Painter& painter) {
    const Style style = context().theme->resolve(frame_role_);
    const Rect all{0, 0, bounds().width, bounds().height};
    painter.fill(all, Cell::from_grapheme(" ", style));
    painter.draw_box(all, scene::LineStyle::Single, style);
}

// Both endings copy the callback off this object before running it: the
// callback closes the popup, which destroys the object the callback was
// stored in.
void PopupList::choose(std::size_t index) {
    if (finished_) return;
    finished_ = true;
    const std::function<void(std::size_t)> chosen = on_choose;
    if (chosen) chosen(index);
}

void PopupList::dismiss() {
    if (finished_) return;
    finished_ = true;
    const std::function<void()> dismissed = on_dismiss;
    if (dismissed) dismissed();
}

bool PopupList::on_key(const KeyEvent& event) {
    if (event.action != KeyAction::Press) return false;
    if (event.chord.key == Key::Escape) {
        dismiss();
        return true;
    }
    return list_->on_key(event);
}

bool PopupList::on_mouse(const MouseEvent& event) {
    const Rect abs = absolute_bounds();
    const bool inside = event.cell.x >= abs.x && event.cell.x < abs.right() && event.cell.y >= abs.y &&
                        event.cell.y < abs.bottom();
    if (!inside) {
        // Light dismissal, as every other popup here: a press beside it is
        // the reader saying they wanted none of it.
        if (event.action == MouseAction::Down) dismiss();
        return false;
    }
    if (event.action != MouseAction::Down) return true;
    const Rect list_abs = list_->absolute_bounds();
    if (event.cell.y < list_abs.y || event.cell.y >= list_abs.bottom()) return true;  // the frame
    // One press picks. A list inside a window wants a second click before it
    // acts, because there the first one is how a row is merely selected; a
    // list that exists only to be picked from has no such distinction.
    list_->on_mouse(event);
    if (list_->cursor() >= 0) choose(static_cast<std::size_t>(list_->cursor()));
    return true;
}

PopupList* show_popup_list(Rect anchor_absolute, std::vector<std::string> items,
                           std::optional<std::size_t> selected, ui::Application& app, Desktop& desktop,
                           std::function<void(std::size_t)> on_choose, std::function<void()> on_dismiss) {
    const Rect desktop_abs = desktop.absolute_bounds();
    auto* raw = desktop.add_popup(std::make_unique<PopupList>(std::move(items), std::move(selected)));
    const ui::SizeHint w = raw->horizontal_size_hint();
    const ui::SizeHint h = raw->vertical_size_hint();
    // At least as wide as what dropped it, so the list lines up with the
    // control rather than sitting inside its own smaller box.
    const int width = std::min(std::max(w.preferred, anchor_absolute.width), desktop.bounds().width);
    const int below = desktop_abs.bottom() - anchor_absolute.bottom();
    const int above = anchor_absolute.y - desktop_abs.y;
    // Hung below where it fits, and above where it does not -- a list that
    // ran off the bottom would be clamped up over the control that opened it.
    const bool drop_up = h.preferred > below && above > below;
    const int height = std::min(h.preferred, std::max(1, drop_up ? above : below));
    const int x = std::clamp(anchor_absolute.x - desktop_abs.x, 0, std::max(0, desktop.bounds().width - width));
    const int y = drop_up ? anchor_absolute.y - desktop_abs.y - height : anchor_absolute.bottom() - desktop_abs.y;
    raw->set_bounds(Rect{x, std::clamp(y, 0, std::max(0, desktop.bounds().height - height)), width, height});

    const ui::Application::ModalScopeId scope = app.push_modal(*raw);
    ui::View* const restore_focus = app.focused();
    // What held the mouse before this popup took it. A list opened from
    // inside another popup has to hand the mouse BACK when it closes, not
    // merely let go: clearing it leaves the popup underneath with a modal
    // scope and nothing listening, so its own light dismissal stops working
    // and every click in the application goes nowhere. Application drops a
    // capture that is no longer usable, so restoring one that has since gone
    // is safe.
    ui::View* const restore_capture = app.input_capture();
    const auto close = [&app, &desktop, raw, scope, restore_focus, restore_capture] {
        if (app.input_capture() == raw) {
            if (restore_capture != nullptr)
                app.set_input_capture(restore_capture);
            else
                app.clear_input_capture();
        }
        app.pop_modal(scope);
        if (restore_focus != nullptr) app.set_focus(restore_focus);
        desktop.remove_popup(raw);  // discards ownership -> destroys this view
    };
    // Closing first, answering second: the caller may open something else in
    // its answer, and this popup should be gone by the time it does.
    raw->on_dismiss = [close, dismissed = std::move(on_dismiss)] {
        close();
        if (dismissed) dismissed();
    };
    raw->on_choose = [close, chosen = std::move(on_choose)](std::size_t index) {
        close();
        if (chosen) chosen(index);
    };
    app.set_input_capture(raw);
    app.set_focus(&raw->list());
    return raw;
}

}  // namespace ckv::widgets
