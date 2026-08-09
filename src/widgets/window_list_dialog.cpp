// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/window_list_dialog.hpp"

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/list_view.hpp"

namespace ckv::widgets {

namespace {
using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;
}  // namespace

WindowHandle make_window_list_dialog(Desktop& desktop, const ui::StandardRoles& roles, ui::Application& app,
                                      ui::View* restore_focus_to, const StandardStrings& strings) {
    auto window = std::make_unique<Window>(strings.window_list_title);
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                               roles.dialog_background);
    Window* window_ptr = window.get();
    const detail::DialogFocusRestore focus_restore{restore_focus_to};
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();

    auto column = std::make_unique<Column>();
    column->set_spacing(1);

    auto list = std::make_unique<ListView>(/*multi_select=*/false);
    std::vector<std::string> titles;
    for (Window* w : desktop.windows()) titles.push_back(w->title());
    list->set_items(titles);
    auto* list_ptr =
        static_cast<ListView*>(column->add_item(std::move(list), LayoutSpec{SizePolicy::Expanding, 1}));

    auto button_row = std::make_unique<Row>();
    auto close_button = std::make_unique<Button>(strings.close);
    close_button->set_default(true);
    auto* close_ptr =
        static_cast<Button*>(button_row->add_item(std::move(close_button), LayoutSpec{SizePolicy::Fixed, 1}));
    column->add_item(std::move(button_row), LayoutSpec{SizePolicy::Fixed, 1});

    window->set_content(std::move(column));

    // Activating a row (Enter while the list has focus, or a second
    // click on the already-current row — see ListView's own doc) both
    // switches to that window on the Desktop AND closes this dialog.
    Desktop* desktop_ptr = &desktop;
    list_ptr->on_activate = [desktop_ptr, window_ptr](std::size_t index) {
        const auto& windows = desktop_ptr->windows();
        if (index < windows.size()) desktop_ptr->activate(windows[index]);
        window_ptr->close();
    };
    close_ptr->on_press = [window_ptr]() { window_ptr->close(); };
    // Enter anywhere else in the dialog (not consumed by the list
    // itself) falls through to the default Close button, per the same
    // "Enter -> default button" convention as materialize_dialog.
    window->accept_request = [close_ptr]() {
        if (close_ptr->on_press) close_ptr->on_press();
    };
    window->cancel_request = [window_ptr]() { window_ptr->close(); };

    window->on_closed = [&app, focus_restore, window_ptr, window_liveness]() {
        const detail::DialogFocusRestore held_focus_restore = focus_restore;
        const std::weak_ptr<void> held_window_liveness = window_liveness;
        Window* const held_window = window_ptr;
        held_focus_restore.restore(app);
        if (!held_window_liveness.expired()) schedule_self_detach(*held_window, app);
    };

    return WindowHandle{std::move(window), list_ptr};
}

WindowListDialogPresentation present_window_list_dialog(Desktop& desktop, ui::Application& app,
                                                         const ui::StandardRoles& roles,
                                                         const StandardStrings& strings) {
    using Access = detail::DialogPresentationAccess<WindowListDialogResult>;
    auto parts = Access::make();
    auto handle = make_window_list_dialog(desktop, roles, app, app.focused(), strings);
    auto previous_on_detached = std::move(handle.window->on_detached);
    handle.window->on_detached = [previous = std::move(previous_on_detached), state = parts.state]() {
        if (previous) previous();
        Access::finish(state, WindowListDialogResult::Closed);
    };
    desktop.present_modal(std::move(handle), app);
    return std::move(parts.presentation);
}

}  // namespace ckv::widgets
