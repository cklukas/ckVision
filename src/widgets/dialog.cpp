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
                                          roles.button_default);
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

}  // namespace ckv::widgets
