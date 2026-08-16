// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/dialog.hpp"

#include <optional>

#include "cvision/core/assert.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog_presentation.hpp"

namespace ckv::widgets {

namespace {

using ui::Column;
using ui::LayoutSpec;
using ui::Row;
using ui::SizePolicy;

ui::View* find_first_focusable(ui::View& view) {
    if (view.focusable()) return &view;
    for (const auto& child : view.children())
        if (ui::View* found = find_first_focusable(*child)) return found;
    return nullptr;
}

bool validate_inputs(const std::vector<InputLine*>& inputs, const DialogDescriptor& descriptor,
                      ui::Application& app) {
    CKV_ASSERT(inputs.size() == descriptor.fields.size());
    InputLine* first_invalid = nullptr;
    bool all_valid = true;
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        const auto& validate = descriptor.fields[i].validate;
        const bool valid = !validate || validate(inputs[i]->text());
        inputs[i]->set_valid(valid);
        if (!valid) {
            all_valid = false;
            if (first_invalid == nullptr) first_invalid = inputs[i];
        }
    }
    if (!all_valid) {
        app.set_focus(first_invalid);
        return false;
    }
    return true;
}

struct DescriptorDialogCompletion {
    std::weak_ptr<void> window_liveness;
    std::optional<DialogResult> selected_result;
    bool closed = false;
};

struct BuiltDescriptorDialog {
    WindowHandle handle;
    std::shared_ptr<DescriptorDialogCompletion> completion;
};

DialogResult accepted_result(const std::vector<InputLine*>& inputs) {
    DialogResult result;
    result.accepted = true;
    result.values.reserve(inputs.size());
    for (const InputLine* input : inputs) result.values.push_back(input->text());
    return result;
}

BuiltDescriptorDialog build_descriptor_dialog(DialogDescriptor descriptor, const ui::StandardRoles& roles,
                                              ui::Application& app, ui::View* restore_focus_to) {
    auto retained_descriptor = std::make_shared<DialogDescriptor>(std::move(descriptor));
    MaterializedDialog materialized = materialize_dialog(*retained_descriptor);
    std::vector<InputLine*> inputs = materialized.inputs;
    Button* const default_button = materialized.default_button;
    ui::View* const initial_focus = materialized.initial_focus;

    auto window = std::make_unique<Window>(retained_descriptor->title);
    window->set_role_override(roles.dialog_frame, roles.dialog_background, roles.dialog_frame,
                              roles.dialog_background);
    if (retained_descriptor->minimum_window_size.width > 0 ||
        retained_descriptor->minimum_window_size.height > 0)
        window->set_min_size(retained_descriptor->minimum_window_size);
    // A descriptor dialog is held off its frame like any other. Previously
    // it had no margin at all, which left fields and buttons against the
    // border and made each application add padding of its own -- the same
    // decision taken repeatedly, differently.
    window->set_content_margin(1, 1);
    Window* const window_ptr = window.get();
    auto completion = std::make_shared<DescriptorDialogCompletion>(
        DescriptorDialogCompletion{window_ptr->lifetime_token(), std::nullopt, false});
    const detail::DialogFocusRestore focus_restore{restore_focus_to};

    window->set_content(std::move(materialized.root));
    window->set_resizable(retained_descriptor->resizable);
    window->accept_request = [window_ptr, completion, retained_descriptor, inputs, default_button, &app]() {
        // This closure is owned by Window itself. Retain everything needed
        // after the descriptor callback before entering user code: that
        // callback is allowed to detach and destroy its own Window, which
        // destroys this std::function while it is executing.
        const std::shared_ptr<DescriptorDialogCompletion> held_completion = completion;
        Window* const held_window = window_ptr;
        if (!validate_inputs(inputs, *retained_descriptor, app)) return;
        // Record before application code runs: a descriptor button handler
        // may detach or destroy this Window synchronously, but its successful
        // acceptance still has one stable typed result after detachment.
        held_completion->selected_result = accepted_result(inputs);
        if (default_button != nullptr && default_button->on_press) default_button->on_press();
        if (!held_completion->closed && !held_completion->window_liveness.expired()) held_window->close();
    };
    window->cancel_request = [window_ptr, completion]() {
        const std::shared_ptr<DescriptorDialogCompletion> held_completion = completion;
        Window* const held_window = window_ptr;
        if (!held_completion->window_liveness.expired()) held_window->close();
    };
    window->on_closed = [&app, focus_restore, window_ptr, completion]() {
        const std::shared_ptr<DescriptorDialogCompletion> held_completion = completion;
        const detail::DialogFocusRestore held_focus_restore = focus_restore;
        Window* const held_window = window_ptr;
        held_completion->closed = true;
        held_focus_restore.restore(app);
        if (!held_completion->window_liveness.expired()) schedule_self_detach(*held_window, app);
    };
    return BuiltDescriptorDialog{WindowHandle{std::move(window), initial_focus}, std::move(completion)};
}

}  // namespace

MaterializedDialog materialize_dialog(const DialogDescriptor& descriptor) {
    CKV_ASSERT(!descriptor.fields.empty() || !descriptor.buttons.empty());

    MaterializedDialog result;
    auto column = std::make_unique<Column>();
    column->set_spacing(1);

    for (const FieldDescriptor& field : descriptor.fields) {
        auto row = std::make_unique<Row>();
        row->set_spacing(1);

        Label* label_ptr = nullptr;
        if (!field.label.empty()) {
            auto label = std::make_unique<Label>(field.label);
            label_ptr = static_cast<Label*>(row->add_item(std::move(label), LayoutSpec{SizePolicy::Fixed, 1}));
        }

        auto input = std::make_unique<InputLine>();
        input->set_text(field.initial_text);
        input->set_password_echo(field.password_echo, field.password_echo_char);
        auto* input_ptr =
            static_cast<InputLine*>(row->add_item(std::move(input), LayoutSpec{SizePolicy::Expanding, 1}));

        if (label_ptr != nullptr) label_ptr->set_buddy(input_ptr);
        result.labels.push_back(label_ptr);
        result.inputs.push_back(input_ptr);

        column->add_item(std::move(row), LayoutSpec{SizePolicy::Fixed, 1});
    }

    if (!descriptor.buttons.empty()) {
        if (descriptor.anchor_buttons_to_bottom) {
            auto spacer = std::make_unique<ui::View>();
            spacer->set_preferred_size(Size{0, 0});
            column->add_item(std::move(spacer), LayoutSpec{SizePolicy::Expanding, 1});
        }
        auto button_row = std::make_unique<Row>();
        button_row->set_spacing(2);
        for (const ButtonDescriptor& bd : descriptor.buttons) {
            auto button = std::make_unique<Button>(bd.label);
            button->set_default(bd.is_default);
            button->on_press = bd.on_press;
            auto* button_ptr =
                static_cast<Button*>(button_row->add_item(std::move(button), LayoutSpec{SizePolicy::Fixed, 1}));
            result.buttons.push_back(button_ptr);
            if (bd.is_default) {
                CKV_ASSERT(result.default_button == nullptr);  // at most one default button
                result.default_button = button_ptr;
            }
        }
        column->add_item(std::move(button_row),
                         LayoutSpec{SizePolicy::Fixed, 1, descriptor.button_alignment});
    }

    result.initial_focus = find_first_focusable(*column);
    result.root = std::move(column);
    return result;
}

bool validate_dialog(MaterializedDialog& dialog, const DialogDescriptor& descriptor, ui::Application& app) {
    return validate_inputs(dialog.inputs, descriptor, app);
}

void wire_dialog_window(Window& window, MaterializedDialog dialog, const DialogDescriptor& descriptor,
                         ui::Application& app, ui::View* restore_focus_to) {
    // Copy what accept_request needs before dialog.root (which owns
    // these views) is moved into the window's content — the raw
    // pointers stay valid afterward (set_content only reparents).
    // Captured by value (plain vectors/pointers, all copyable) rather
    // than the whole MaterializedDialog, which holds a unique_ptr and
    // so cannot be captured into a std::function at all.
    std::vector<InputLine*> inputs = dialog.inputs;
    Button* default_button = dialog.default_button;

    window.set_content(std::move(dialog.root));
    window.set_resizable(descriptor.resizable);

    // A default-button callback may close, detach, or destroy its hosting
    // window. Keep the close state and per-instance lifetime identity outside
    // that Window so the accept path neither double-closes it nor dereferences
    // a destroyed object after application code returns.
    struct CloseState {
        std::weak_ptr<void> window_liveness;
        bool closed = false;
    };
    auto close_state = std::make_shared<CloseState>(CloseState{window.lifetime_token(), false});
    const detail::DialogFocusRestore focus_restore{restore_focus_to};

    window.accept_request = [&window, &app, inputs, &descriptor, default_button, close_state]() {
        const std::shared_ptr<CloseState> held_close_state = close_state;
        Window* const held_window = &window;
        if (!validate_inputs(inputs, descriptor, app)) return;  // veto: invalid field already took focus
        if (default_button != nullptr && default_button->on_press) default_button->on_press();
        if (!held_close_state->closed && !held_close_state->window_liveness.expired()) held_window->close();
    };
    window.cancel_request = [&window]() { window.close(); };  // Esc bypasses validation by definition

    auto previous_on_closed = window.on_closed;
    window.on_closed = [&app, focus_restore, previous_on_closed, close_state]() {
        const std::shared_ptr<CloseState> held_close_state = close_state;
        const detail::DialogFocusRestore held_focus_restore = focus_restore;
        const std::function<void()> held_previous_on_closed = previous_on_closed;
        held_close_state->closed = true;
        held_focus_restore.restore(app);
        if (held_previous_on_closed) held_previous_on_closed();
    };
}

DescriptorDialogPresentation present_dialog(DialogDescriptor descriptor, ui::Application& app,
                                             Desktop& desktop, const ui::StandardRoles& roles) {
    using Access = detail::DialogPresentationAccess<DialogResult>;
    auto parts = Access::make();
    BuiltDescriptorDialog built = build_descriptor_dialog(std::move(descriptor), roles, app, app.focused());
    auto previous_on_detached = std::move(built.handle.window->on_detached);
    built.handle.window->on_detached = [previous = std::move(previous_on_detached), state = parts.state,
                                        completion = built.completion]() {
        if (previous) previous();
        if (completion->selected_result) Access::record(state, *completion->selected_result);
        Access::finish(state, DialogResult{});
    };
    desktop.present_modal(std::move(built.handle), app);
    return std::move(parts.presentation);
}

DialogResult exec_dialog(DialogDescriptor descriptor, ui::Application& app,
                         Desktop& desktop, const ui::StandardRoles& roles) {
    BuiltDescriptorDialog built = build_descriptor_dialog(std::move(descriptor), roles, app, app.focused());
    const std::shared_ptr<DescriptorDialogCompletion> completion = built.completion;
    desktop.exec_modal(app, std::move(built.handle));
    return completion->selected_result.value_or(DialogResult{});
}

}  // namespace ckv::widgets
