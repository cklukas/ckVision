// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/message_box.hpp"

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

std::vector<std::pair<MessageBoxResult, std::string>> button_specs(MessageBoxButtons buttons) {
    switch (buttons) {
        case MessageBoxButtons::Ok:
            return {{MessageBoxResult::Ok, "OK"}};
        case MessageBoxButtons::OkCancel:
            return {{MessageBoxResult::Ok, "OK"}, {MessageBoxResult::Cancel, "Cancel"}};
        case MessageBoxButtons::YesNo:
            return {{MessageBoxResult::Yes, "Yes"}, {MessageBoxResult::No, "No"}};
        case MessageBoxButtons::YesNoCancel:
            return {{MessageBoxResult::Yes, "Yes"},
                    {MessageBoxResult::No, "No"},
                    {MessageBoxResult::Cancel, "Cancel"}};
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

}  // namespace

WindowHandle make_message_box(const MessageBoxDescriptor& descriptor, const ui::StandardRoles& roles,
                               ui::Application& app, ui::View* restore_focus_to,
                               std::function<void(MessageBoxResult)> on_result) {
    auto window = std::make_unique<Window>(descriptor.title);
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                               roles.dialog_background);
    Window* window_ptr = window.get();

    auto column = std::make_unique<Column>();
    column->set_spacing(1);
    column->add_item(std::make_unique<StaticText>(descriptor.message), LayoutSpec{SizePolicy::Expanding, 1});

    auto button_row = std::make_unique<Row>();
    button_row->set_spacing(2);
    Button* default_button = nullptr;
    for (const auto& [result, label] : button_specs(descriptor.buttons)) {
        auto button = std::make_unique<Button>(label);
        const bool is_default = default_button == nullptr;  // the first button in the set
        button->set_default(is_default);
        auto* raw = static_cast<Button*>(button_row->add_item(std::move(button), LayoutSpec{SizePolicy::Fixed, 1}));
        raw->on_press = [window_ptr, result, on_result]() {
            if (on_result) on_result(result);
            window_ptr->close();
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
    window->cancel_request = [window_ptr, cancel, on_result]() {
        if (on_result) on_result(cancel);
        window_ptr->close();
    };

    window->on_closed = [&app, restore_focus_to, window_ptr]() {
        if (restore_focus_to != nullptr && restore_focus_to->focusable()) app.set_focus(restore_focus_to);
        schedule_self_detach(*window_ptr, app);
    };

    return WindowHandle{std::move(window), default_button};
}

}  // namespace ckv::widgets
