// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/dialog.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include "cktest.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::ButtonDescriptor;
using ckv::widgets::DialogDescriptor;
using ckv::widgets::FieldDescriptor;
using ckv::widgets::materialize_dialog;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};
}  // namespace

CK_TEST(materializing_an_empty_descriptor_aborts) {
    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        Fixture f;
        DialogDescriptor descriptor;  // no fields, no buttons
        materialize_dialog(descriptor, f.theme, f.roles);
        ::_exit(1);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
}

CK_TEST(materializing_two_default_buttons_aborts) {
    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        Fixture f;
        DialogDescriptor descriptor;
        descriptor.buttons.push_back(ButtonDescriptor{"OK", true, nullptr});
        descriptor.buttons.push_back(ButtonDescriptor{"Apply", true, nullptr});  // second default
        materialize_dialog(descriptor, f.theme, f.roles);
        ::_exit(1);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
}

CK_TEST(one_field_and_one_button_produce_one_input_and_one_button_in_order) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&Name:", "initial"});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", true, nullptr});
    auto dialog = materialize_dialog(descriptor, f.theme, f.roles);

    CK_CHECK(dialog.root != nullptr);
    CK_CHECK(dialog.labels.size() == 1);
    CK_CHECK(dialog.inputs.size() == 1);
    CK_CHECK(dialog.buttons.size() == 1);
    CK_CHECK(dialog.labels[0] != nullptr);
    CK_CHECK(dialog.inputs[0]->text() == "initial");
    CK_CHECK(dialog.buttons[0]->text() == "OK");
}

CK_TEST(a_field_with_an_empty_label_produces_a_null_label_pointer_but_a_real_input) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"", "value"});
    auto dialog = materialize_dialog(descriptor, f.theme, f.roles);
    CK_CHECK(dialog.labels.size() == 1);
    CK_CHECK(dialog.labels[0] == nullptr);
    CK_CHECK(dialog.inputs[0]->text() == "value");
}

CK_TEST(a_labeled_fields_mnemonic_buddy_is_wired_to_its_own_input) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&Path:", ""});
    auto dialog = materialize_dialog(descriptor, f.theme, f.roles);
    CK_CHECK(dialog.labels[0]->buddy() == dialog.inputs[0]);
}

CK_TEST(the_button_marked_default_is_reported_as_the_default_button) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.buttons.push_back(ButtonDescriptor{"Cancel", false, nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", true, nullptr});
    auto dialog = materialize_dialog(descriptor, f.theme, f.roles);
    CK_CHECK(dialog.default_button == dialog.buttons[1]);
    CK_CHECK(dialog.buttons[1]->is_default());
    CK_CHECK(!dialog.buttons[0]->is_default());
}

CK_TEST(no_button_marked_default_reports_a_null_default_button) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.buttons.push_back(ButtonDescriptor{"Close", false, nullptr});
    auto dialog = materialize_dialog(descriptor, f.theme, f.roles);
    CK_CHECK(dialog.default_button == nullptr);
}

CK_TEST(initial_focus_is_the_first_input_when_the_dialog_has_fields) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&First:", ""});
    descriptor.fields.push_back(FieldDescriptor{"&Second:", ""});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", true, nullptr});
    auto dialog = materialize_dialog(descriptor, f.theme, f.roles);
    CK_CHECK(dialog.initial_focus == dialog.inputs[0]);
}

CK_TEST(initial_focus_falls_back_to_the_first_button_when_the_dialog_has_no_fields) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.buttons.push_back(ButtonDescriptor{"Close", true, nullptr});
    auto dialog = materialize_dialog(descriptor, f.theme, f.roles);
    CK_CHECK(dialog.initial_focus == dialog.buttons[0]);
}

CK_TEST(a_button_press_handler_provided_in_the_descriptor_fires_through_the_materialized_button) {
    Fixture f;
    DialogDescriptor descriptor;
    bool pressed = false;
    descriptor.buttons.push_back(ButtonDescriptor{"OK", true, [&] { pressed = true; }});
    auto dialog = materialize_dialog(descriptor, f.theme, f.roles);
    dialog.buttons[0]->on_press();
    CK_CHECK(pressed);
}

CK_TEST(field_and_button_order_in_the_descriptor_is_preserved_in_the_materialized_arrays) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "a"});
    descriptor.fields.push_back(FieldDescriptor{"&B:", "b"});
    descriptor.buttons.push_back(ButtonDescriptor{"First", false, nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"Second", false, nullptr});
    auto dialog = materialize_dialog(descriptor, f.theme, f.roles);
    CK_CHECK(dialog.inputs[0]->text() == "a");
    CK_CHECK(dialog.inputs[1]->text() == "b");
    CK_CHECK(dialog.buttons[0]->text() == "First");
    CK_CHECK(dialog.buttons[1]->text() == "Second");
}

CK_TEST(the_materialized_root_owns_the_entire_tree_and_destroying_it_is_safe) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&Name:", ""});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", true, nullptr});
    auto dialog = materialize_dialog(descriptor, f.theme, f.roles);
    dialog.root.reset();  // must not crash or leak (ASan build verifies the latter)
    CK_CHECK(true);
}
