// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/dialog.hpp"

#include "cvision/core/assert.hpp"
#include "cvision/ui/layout.hpp"

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

}  // namespace

MaterializedDialog materialize_dialog(const DialogDescriptor& descriptor, const ui::Theme& theme,
                                       const ui::StandardRoles& roles) {
    CKV_ASSERT(!descriptor.fields.empty() || !descriptor.buttons.empty());

    MaterializedDialog result;
    auto column = std::make_unique<Column>();
    column->set_spacing(1);

    for (const FieldDescriptor& field : descriptor.fields) {
        auto row = std::make_unique<Row>();
        row->set_spacing(1);

        Label* label_ptr = nullptr;
        if (!field.label.empty()) {
            auto label = std::make_unique<Label>(field.label, theme, roles.label_text, roles.label_mnemonic);
            label_ptr = static_cast<Label*>(row->add_item(std::move(label), LayoutSpec{SizePolicy::Fixed, 1}));
        }

        auto input = std::make_unique<InputLine>(theme, roles.input_normal, roles.input_focused,
                                                   roles.input_invalid);
        input->set_text(field.initial_text);
        auto* input_ptr =
            static_cast<InputLine*>(row->add_item(std::move(input), LayoutSpec{SizePolicy::Expanding, 1}));

        if (label_ptr != nullptr) label_ptr->set_buddy(input_ptr);
        result.labels.push_back(label_ptr);
        result.inputs.push_back(input_ptr);

        column->add_item(std::move(row), LayoutSpec{SizePolicy::Fixed, 1});
    }

    if (!descriptor.buttons.empty()) {
        auto button_row = std::make_unique<Row>();
        button_row->set_spacing(2);
        for (const ButtonDescriptor& bd : descriptor.buttons) {
            auto button =
                std::make_unique<Button>(bd.label, theme, roles.button_normal, roles.button_focused,
                                          roles.button_default, roles.button_shadow);
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
        column->add_item(std::move(button_row), LayoutSpec{SizePolicy::Fixed, 1});
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

    // Guards against the default button's OWN on_press closing the
    // window itself (a natural pattern: "the OK button closes the
    // dialog") — without this, accept_request's own window.close()
    // afterward would double-fire on_closed (or, if the caller's
    // on_press somehow destroyed the Window outright rather than just
    // closing it, be a genuine use-after-free). on_closed already
    // fires synchronously from close(), so by the time accept_request
    // checks *alive after on_press returns, it correctly reflects
    // whether on_press already closed the window.
    auto alive = std::make_shared<bool>(true);

    window.accept_request = [&window, &app, inputs, &descriptor, default_button, alive]() {
        if (!validate_inputs(inputs, descriptor, app)) return;  // veto: invalid field already took focus
        if (default_button != nullptr && default_button->on_press) default_button->on_press();
        if (*alive) window.close();
    };
    window.cancel_request = [&window]() { window.close(); };  // Esc bypasses validation by definition

    auto previous_on_closed = window.on_closed;
    window.on_closed = [&app, restore_focus_to, previous_on_closed, alive]() {
        *alive = false;
        if (restore_focus_to != nullptr && restore_focus_to->focusable()) app.set_focus(restore_focus_to);
        if (previous_on_closed) previous_on_closed();
    };
}

}  // namespace ckv::widgets
