// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/message_box.hpp"

#include <algorithm>
#include <optional>
#include <vector>

#include "cvision/core/assert.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/canvas.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/widgets/static_text.hpp"

namespace ckv::widgets {

namespace {

using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;

class MessageBoxContent final : public Column {
public:
    explicit MessageBoxContent(int minimum_width) : minimum_width_(std::max(0, minimum_width)) {}

    ui::SizeHint horizontal_size_hint() const override {
        ui::SizeHint hint = Column::horizontal_size_hint();
        hint.min = std::max(hint.min, minimum_width_);
        hint.preferred = std::max(hint.preferred, minimum_width_);
        if (hint.max != ui::kUnboundedExtent) hint.max = std::max(hint.max, hint.preferred);
        return hint;
    }

private:
    int minimum_width_ = 0;
};

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
    window->set_resizable(false);
    // An alert is a short panel of prose; holding it off the frame is what
    // separates the message from the box drawn around it. The size hints
    // budget this margin, so the alert grows to keep it rather than
    // squeezing its own text.
    // One cell off the frame. The window drops the bottom of that itself
    // when the content ends in a button row, whose shadow is already the
    // gap.
    window->set_content_margin(1, 1);
    Window* window_ptr = window.get();
    const detail::DialogFocusRestore focus_restore{restore_focus_to};
    const std::weak_ptr<void> window_liveness = window_ptr->lifetime_token();
    auto completion = std::make_shared<MessageBoxCompletion>(
        MessageBoxCompletion{window_ptr->lifetime_token(), std::move(on_result)});

    auto column = std::make_unique<MessageBoxContent>(descriptor.minimum_content_width);
    column->set_spacing(1);
    if (descriptor.graphic != nullptr && !descriptor.graphic->empty() &&
        descriptor.graphic_max_cells.width > 0 && descriptor.graphic_max_cells.height > 0) {
        // Fit here rather than reserving the ceiling: the rows the artwork
        // does not need would otherwise become a blank band above the text.
        const Size box = fit_image_cells(
            Size{descriptor.graphic->width(), descriptor.graphic->height()},
            app.terminal_cell_pixels(), descriptor.graphic_max_cells);
        auto graphic = std::make_unique<ImageView>();
        graphic->set_preferred_size(box);
        graphic->set_image(descriptor.graphic);
        column->add_item(std::move(graphic),
                         LayoutSpec{SizePolicy::Fixed, box.height, descriptor.graphic_alignment});
    }
    auto message = std::make_unique<StaticText>(descriptor.message);
    message->set_role_override(message_role(descriptor.kind, roles));
    message->set_alignment(descriptor.message_alignment);
    message->set_emphasized_leading_lines(descriptor.emphasized_leading_lines);
    column->add_item(std::move(message), LayoutSpec{SizePolicy::Expanding, 1, ui::Alignment::Fill});

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
            Window* const report_window = window_ptr;
            const std::shared_ptr<MessageBoxCompletion> held_completion = completion;
            held_completion->report(result, report_window);
        };
        if (is_default) default_button = raw;
    }
    column->add_item(std::move(button_row),
                     LayoutSpec{SizePolicy::Fixed, 1, descriptor.button_alignment});

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
        Window* const report_window = window_ptr;
        const std::shared_ptr<MessageBoxCompletion> held_completion = completion;
        held_completion->report(cancel, report_window);
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

void install_about_help(ui::Application& app, Desktop& desktop, const ui::StandardRoles& roles,
                        std::string title, std::string body) {
    // The handler outlives this call, so everything it needs that a caller
    // might not keep alive is copied: the identity strings, and the roles --
    // which are a handful of ids, and which a caller may well have interned
    // inline at the call site. Application and Desktop are the two things
    // that outlive any handler by construction.
    app.commands().set_handler(
        app.commands().standard().help,
        [&app, &desktop, roles, held_title = std::move(title), held_body = std::move(body)] {
            MessageBoxDescriptor descriptor{MessageBoxKind::Info, "About", held_title + "\n\n" + held_body,
                                            MessageBoxButtons::Ok};
            descriptor.emphasized_leading_lines = 1;
            // The body reads down a straight left edge, not centred. A
            // centred paragraph starts every line at a different column, and
            // the eye looking for the next one has to find where it begins;
            // for the one or two sentences an About carries that costs more
            // than the symmetry is worth. The button keeps its centred
            // placement, since a lone button is not a line of prose.
            descriptor.button_alignment = ui::Alignment::Center;
            auto presentation = present_message_box(app, desktop, roles, descriptor);
            presentation.set_completion_handler([](MessageBoxResult) {});
        });
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
