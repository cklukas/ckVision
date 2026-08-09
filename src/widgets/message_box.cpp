// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/message_box.hpp"

#include <optional>
#include <vector>

#include "cvision/core/assert.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/static_text.hpp"

namespace ckv::widgets {

namespace {

using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;

std::vector<std::pair<MessageBoxResult, std::string>> button_specs(MessageBoxButtons buttons,
                                                                    const StandardStrings& strings) {
    switch (buttons) {
        case MessageBoxButtons::Ok:
            return {{MessageBoxResult::Ok, strings.ok}};
        case MessageBoxButtons::OkCancel:
            return {{MessageBoxResult::Ok, strings.ok}, {MessageBoxResult::Cancel, strings.cancel}};
        case MessageBoxButtons::YesNo:
            return {{MessageBoxResult::Yes, strings.yes}, {MessageBoxResult::No, strings.no}};
        case MessageBoxButtons::YesNoCancel:
            return {{MessageBoxResult::Yes, strings.yes},
                    {MessageBoxResult::No, strings.no},
                    {MessageBoxResult::Cancel, strings.cancel}};
    }
    CKV_ASSERT(false);  // unreachable: every enumerator handled above
    return {};
}

// The result Esc maps to: the button set's own "negative" choice
// (Cancel where present, otherwise No), or — for a bare Ok box, which
// has neither — Ok itself, matching "Esc dismisses an alert" convention.
MessageBoxResult escape_result(MessageBoxButtons buttons) {
    switch (buttons) {
        case MessageBoxButtons::Ok:
            return MessageBoxResult::Ok;
        case MessageBoxButtons::OkCancel:
        case MessageBoxButtons::YesNoCancel:
            return MessageBoxResult::Cancel;
        case MessageBoxButtons::YesNo:
            return MessageBoxResult::No;
    }
    CKV_ASSERT(false);
    return MessageBoxResult::Ok;
}

ui::RoleId message_role(MessageBoxKind kind, const ui::StandardRoles& roles) noexcept {
    switch (kind) {
        case MessageBoxKind::Info:
            return roles.message_info_text;
        case MessageBoxKind::Warning:
            return roles.message_warning_text;
        case MessageBoxKind::Error:
            return roles.message_error_text;
        case MessageBoxKind::Confirm:
            return roles.message_confirm_text;
    }
    CKV_ASSERT(false);
    return roles.message_info_text;
}

// Button and cancel handlers may be destroyed by the result callback itself:
// an embedding application is allowed to detach and destroy the dialog from
// that callback. Keep completion state independent of the Window, and make
// the state single-shot before entering application code.
struct MessageBoxCompletion {
    std::weak_ptr<void> window_liveness;
    std::function<void(MessageBoxResult)> on_result;
    bool delivered = false;

    void report(MessageBoxResult result, Window* window) {
        if (delivered) return;
        delivered = true;
        if (on_result) on_result(result);
        if (!window_liveness.expired()) window->close();
    }
};

}  // namespace

WindowHandle make_message_box(const MessageBoxDescriptor& descriptor, const ui::StandardRoles& roles,
                               ui::Application& app, ui::View* restore_focus_to,
                               std::function<void(MessageBoxResult)> on_result,
                               const StandardStrings& strings) {
    auto window = std::make_unique<Window>(descriptor.title);
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                               roles.dialog_background);
    Window* window_ptr = window.get();
    const detail::DialogFocusRestore focus_restore{restore_focus_to};
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();
    auto completion = std::make_shared<MessageBoxCompletion>(
        MessageBoxCompletion{window_ptr->lifetime_token(), std::move(on_result)});

    auto column = std::make_unique<Column>();
    column->set_spacing(1);
    auto message = std::make_unique<StaticText>(descriptor.message);
    message->set_role_override(message_role(descriptor.kind, roles));
    column->add_item(std::move(message), LayoutSpec{SizePolicy::Expanding, 1});

    auto button_row = std::make_unique<Row>();
    button_row->set_spacing(2);
    Button* default_button = nullptr;
    for (const auto& [result, label] : button_specs(descriptor.buttons, strings)) {
        auto button = std::make_unique<Button>(label);
        const bool is_default = default_button == nullptr;  // the first button in the set
        button->set_default(is_default);
        auto* raw = static_cast<Button*>(button_row->add_item(std::move(button), LayoutSpec{SizePolicy::Fixed, 1}));
        raw->on_press = [window_ptr, completion, result]() {
            // Locals survive even if on_result destroys the running Button
            // callback and its owning Window.
            Window* const window = window_ptr;
            const std::shared_ptr<MessageBoxCompletion> held_completion = completion;
            held_completion->report(result, window);
        };
        if (is_default) default_button = raw;
    }
    column->add_item(std::move(button_row), LayoutSpec{SizePolicy::Fixed, 1});

    window->set_content(std::move(column));

    // Delegates to the default button's OWN on_press (which already
    // closes) rather than closing again itself — avoids the double-
    // close a naive "run handler, then close" would cause when the
    // default button is ALSO clicked directly.
    window->accept_request = [default_button]() {
        if (default_button != nullptr && default_button->on_press) default_button->on_press();
    };

    const MessageBoxResult cancel = escape_result(descriptor.buttons);
    window->cancel_request = [window_ptr, completion, cancel]() {
        Window* const window = window_ptr;
        const std::shared_ptr<MessageBoxCompletion> held_completion = completion;
        held_completion->report(cancel, window);
    };

    window->on_closed = [&app, focus_restore, window_ptr, window_liveness]() {
        const detail::DialogFocusRestore held_focus_restore = focus_restore;
        const std::weak_ptr<void> held_window_liveness = window_liveness;
        Window* const held_window = window_ptr;
        held_focus_restore.restore(app);
        if (!held_window_liveness.expired()) schedule_self_detach(*held_window, app);
    };

    return WindowHandle{std::move(window), default_button};
}

MessageBoxPresentation present_message_box(ui::Application& app, Desktop& desktop,
                                            const ui::StandardRoles& roles,
                                            const MessageBoxDescriptor& descriptor,
                                            const StandardStrings& strings) {
    using Access = detail::DialogPresentationAccess<MessageBoxResult>;
    auto parts = Access::make();
    auto handle = make_message_box(descriptor, roles, app, app.focused(),
                                   [state = parts.state](MessageBoxResult result) { Access::record(state, result); },
                                   strings);
    const MessageBoxResult fallback = escape_result(descriptor.buttons);
    auto previous_on_detached = std::move(handle.window->on_detached);
    handle.window->on_detached = [previous = std::move(previous_on_detached), state = parts.state, fallback]() {
        if (previous) previous();
        Access::finish(state, fallback);
    };
    desktop.present_modal(std::move(handle), app);
    return std::move(parts.presentation);
}

MessageBoxResult exec_message_box(ui::Application& app, Desktop& desktop,
                                   const ui::StandardRoles& roles,
                                   const MessageBoxDescriptor& descriptor,
                                   const StandardStrings& strings) {
    std::optional<MessageBoxResult> result;
    auto on_result = [&result](MessageBoxResult r) { result = r; };
    auto handle = make_message_box(descriptor, roles, app, app.focused(), on_result, strings);
    desktop.exec_modal(app, std::move(handle));
    // on_result always fires before the box's own close() call, on
    // every dismissal path (every button, and cancel_request/Esc) — the
    // only way exec_modal's wait ends without a result already
    // captured is a quit request arriving mid-modal (e.g. Ctrl+C);
    // escape_result is the same "the user walked away" fallback Esc
    // itself already maps to.
    return result.value_or(escape_result(descriptor.buttons));
}

}  // namespace ckv::widgets
