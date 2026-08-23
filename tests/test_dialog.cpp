// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/dialog.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/scroll_viewport.hpp"
#include "cvision/widgets/window.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Rect;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Button;
using ckv::widgets::ButtonDescriptor;
using ckv::widgets::ButtonRole;
using ckv::widgets::Desktop;
using ckv::widgets::DialogDescriptor;
using ckv::widgets::FieldDescriptor;
using ckv::widgets::materialize_dialog;
using ckv::widgets::ScrollViewport;
using ckv::widgets::validate_dialog;
using ckv::widgets::wire_dialog_window;
using ckv::widgets::Window;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};
}  // namespace

CK_TEST(materializing_an_empty_descriptor_aborts) {
    CK_EXPECT_ABORT({
        Fixture f;
        DialogDescriptor descriptor;  // no fields, no buttons
        materialize_dialog(descriptor);
    });
}

CK_TEST(materializing_two_accepting_buttons_aborts) {
    CK_EXPECT_ABORT({
        Fixture f;
        DialogDescriptor descriptor;
        descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
        descriptor.buttons.push_back(ButtonDescriptor{"Apply", ButtonRole::Accept, nullptr});  // a second accepting button
        materialize_dialog(descriptor);
    });
}

CK_TEST(one_field_and_one_button_produce_one_input_and_one_button_in_order) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&Name:", "initial", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);

    CK_CHECK(dialog.root != nullptr);
    CK_CHECK(dialog.labels.size() == 1);
    CK_CHECK(dialog.inputs.size() == 1);
    CK_CHECK(dialog.buttons.size() == 1);
    CK_CHECK(dialog.labels[0] != nullptr);
    CK_CHECK(dialog.inputs[0]->text() == "initial");
    CK_CHECK(dialog.buttons[0]->text() == "OK");
}

CK_TEST(classic_buttons_keep_a_ten_cell_footprint_and_can_be_widened) {
    Button button{"&OK"};
    CK_CHECK(button.horizontal_size_hint().min == Button::kClassicMinimumWidth);
    CK_CHECK(button.horizontal_size_hint().preferred == Button::kClassicMinimumWidth);
    CK_CHECK(button.horizontal_size_hint().max == Button::kClassicMinimumWidth);
    button.set_minimum_width(14);
    CK_CHECK(button.minimum_width() == 14);
    CK_CHECK(button.horizontal_size_hint().min == 14);
    button.set_minimum_width(1);
    CK_CHECK(button.minimum_width() == 3);
    CK_CHECK(button.horizontal_size_hint().min == 6);  // label plus chrome still wins
}

CK_TEST(a_password_field_materializes_an_echoed_input_without_changing_its_value) {
    Fixture f;
    DialogDescriptor descriptor;
    FieldDescriptor field{"&API key:", "secret", nullptr};
    field.password_echo = true;
    field.password_echo_char = '#';
    descriptor.fields.push_back(std::move(field));
    auto dialog = materialize_dialog(descriptor);
    CK_CHECK(dialog.inputs[0]->text() == "secret");
    CK_CHECK(dialog.inputs[0]->password_echo());
    CK_CHECK(dialog.inputs[0]->password_echo_char() == '#');
}

CK_TEST(a_field_with_an_empty_label_produces_a_null_label_pointer_but_a_real_input) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"", "value", nullptr});
    auto dialog = materialize_dialog(descriptor);
    CK_CHECK(dialog.labels.size() == 1);
    CK_CHECK(dialog.labels[0] == nullptr);
    CK_CHECK(dialog.inputs[0]->text() == "value");
}

CK_TEST(a_labeled_fields_mnemonic_buddy_is_wired_to_its_own_input) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&Path:", "", nullptr});
    auto dialog = materialize_dialog(descriptor);
    CK_CHECK(dialog.labels[0]->buddy() == dialog.inputs[0]);
}

CK_TEST(the_button_marked_default_is_reported_as_the_default_button) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.buttons.push_back(ButtonDescriptor{"Cancel", ButtonRole::Dismiss, nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);
    CK_CHECK(dialog.default_button == dialog.buttons[1]);
    CK_CHECK(dialog.buttons[1]->is_default());
    CK_CHECK(!dialog.buttons[0]->is_default());
}

CK_TEST(no_button_marked_default_reports_a_null_default_button) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.buttons.push_back(ButtonDescriptor{"Close", ButtonRole::Dismiss, nullptr});
    auto dialog = materialize_dialog(descriptor);
    CK_CHECK(dialog.default_button == nullptr);
}

CK_TEST(initial_focus_is_the_first_input_when_the_dialog_has_fields) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&First:", "", nullptr});
    descriptor.fields.push_back(FieldDescriptor{"&Second:", "", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);
    CK_CHECK(dialog.initial_focus == dialog.inputs[0]);
}

CK_TEST(initial_focus_falls_back_to_the_first_button_when_the_dialog_has_no_fields) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.buttons.push_back(ButtonDescriptor{"Close", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);
    CK_CHECK(dialog.initial_focus == dialog.buttons[0]);
}

CK_TEST(a_button_press_handler_provided_in_the_descriptor_fires_through_the_materialized_button) {
    Fixture f;
    DialogDescriptor descriptor;
    bool pressed = false;
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, [&] { pressed = true; }});
    auto dialog = materialize_dialog(descriptor);
    dialog.buttons[0]->on_press();
    CK_CHECK(pressed);
}

CK_TEST(field_and_button_order_in_the_descriptor_is_preserved_in_the_materialized_arrays) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "a", nullptr});
    descriptor.fields.push_back(FieldDescriptor{"&B:", "b", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"First", ButtonRole::Neutral, nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"Second", ButtonRole::Neutral, nullptr});
    auto dialog = materialize_dialog(descriptor);
    CK_CHECK(dialog.inputs[0]->text() == "a");
    CK_CHECK(dialog.inputs[1]->text() == "b");
    CK_CHECK(dialog.buttons[0]->text() == "First");
    CK_CHECK(dialog.buttons[1]->text() == "Second");
}

CK_TEST(the_materialized_root_owns_the_entire_tree_and_destroying_it_is_safe) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&Name:", "", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);
    dialog.root.reset();  // must not crash or leak (ASan build verifies the latter)
    CK_CHECK(true);
}

// --- validate_dialog / wire_dialog_window (accept veto, Esc-cancel,
// focus restore) -----------------------------------------------------

namespace {
Window make_window(Fixture&) { return Window("Dialog"); }
}  // namespace

CK_TEST(validate_dialog_with_all_valid_fields_returns_true_and_touches_no_focus) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "ok", [](const std::string& s) { return !s.empty(); }});
    auto dialog = materialize_dialog(descriptor);
    CK_CHECK(validate_dialog(dialog, descriptor, app));
    CK_CHECK(dialog.inputs[0]->valid());
}

CK_TEST(validate_dialog_with_an_invalid_field_vetoes_marks_it_invalid_and_focuses_it) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "", [](const std::string& s) { return !s.empty(); }});
    auto dialog = materialize_dialog(descriptor);
    CK_CHECK(!validate_dialog(dialog, descriptor, app));
    CK_CHECK(!dialog.inputs[0]->valid());
    CK_CHECK(app.focused() == dialog.inputs[0]);
}

CK_TEST(validate_dialog_focuses_only_the_first_invalid_field_among_several) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    auto require_nonempty = [](const std::string& s) { return !s.empty(); };
    descriptor.fields.push_back(FieldDescriptor{"&A:", "", require_nonempty});
    descriptor.fields.push_back(FieldDescriptor{"&B:", "", require_nonempty});
    auto dialog = materialize_dialog(descriptor);
    CK_CHECK(!validate_dialog(dialog, descriptor, app));
    CK_CHECK(app.focused() == dialog.inputs[0]);
    CK_CHECK(!dialog.inputs[0]->valid());
    CK_CHECK(!dialog.inputs[1]->valid());  // still marked invalid even though focus went to the first
}

CK_TEST(revalidating_after_fixing_the_field_clears_the_invalid_marker) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "", [](const std::string& s) { return !s.empty(); }});
    auto dialog = materialize_dialog(descriptor);
    validate_dialog(dialog, descriptor, app);
    CK_CHECK(!dialog.inputs[0]->valid());
    dialog.inputs[0]->set_text("now filled in");
    CK_CHECK(validate_dialog(dialog, descriptor, app));
    CK_CHECK(dialog.inputs[0]->valid());
}

CK_TEST(accept_with_valid_fields_runs_the_default_buttons_handler_and_closes_the_window) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    bool ok_ran = false;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "ok", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, [&] { ok_ran = true; }});
    auto dialog = materialize_dialog(descriptor);

    Window window = make_window(f);
    bool closed = false;
    window.on_closed = [&] { closed = true; };
    wire_dialog_window(window, std::move(dialog), descriptor, app, nullptr);

    CK_CHECK(window.on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(ok_ran);
    CK_CHECK(closed);
}

CK_TEST(accept_with_an_invalid_field_vetoes_the_close_and_never_runs_the_default_handler) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    bool ok_ran = false;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "", [](const std::string& s) { return !s.empty(); }});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, [&] { ok_ran = true; }});
    auto dialog = materialize_dialog(descriptor);
    auto* input = dialog.inputs[0];

    Window window = make_window(f);
    bool closed = false;
    window.on_closed = [&] { closed = true; };
    wire_dialog_window(window, std::move(dialog), descriptor, app, nullptr);

    CK_CHECK(window.on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(!ok_ran);
    CK_CHECK(!closed);
    CK_CHECK(!input->valid());
}

CK_TEST(escape_closes_the_window_without_running_validation_at_all) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "", [](const std::string& s) { return !s.empty(); }});
    auto dialog = materialize_dialog(descriptor);
    auto* input = dialog.inputs[0];

    Window window = make_window(f);
    bool closed = false;
    window.on_closed = [&] { closed = true; };
    wire_dialog_window(window, std::move(dialog), descriptor, app, nullptr);

    CK_CHECK(window.on_key(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}}));
    CK_CHECK(closed);
    CK_CHECK(input->valid());  // never touched — Esc bypasses validation entirely
}

CK_TEST(a_dismissing_button_closes_the_window_the_way_escape_does) {
    // The bug this pins: a button that was merely "not the default" carried
    // nothing but its own handler, so a Cancel with nothing of its own to do
    // was an inert control — the reader pressed it and the dialog stayed
    // exactly where it was.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    bool cancel_ran = false;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "", [](const std::string& s) { return !s.empty(); }});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back(
        ButtonDescriptor{"Cancel", ButtonRole::Dismiss, [&] { cancel_ran = true; }});
    auto dialog = materialize_dialog(descriptor);
    auto* input = dialog.inputs[0];
    Button* const cancel = dialog.buttons[1];

    Window window = make_window(f);
    bool closed = false;
    window.on_closed = [&] { closed = true; };
    wire_dialog_window(window, std::move(dialog), descriptor, app, nullptr);

    cancel->on_press();
    CK_CHECK(cancel_ran);      // its own handler first,
    CK_CHECK(closed);          // then the dialog is gone,
    CK_CHECK(input->valid());  // and the empty field was never validated.
}

CK_TEST(a_neutral_button_runs_its_handler_and_leaves_the_dialog_up) {
    // Apply, Browse..., Reset: the role that exists because not every button
    // in an action row is an answer to the dialog.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    int applied = 0;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "value", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"Apply", ButtonRole::Neutral, [&] { ++applied; }});
    auto dialog = materialize_dialog(descriptor);
    Button* const apply = dialog.buttons[1];

    Window window = make_window(f);
    bool closed = false;
    window.on_closed = [&] { closed = true; };
    wire_dialog_window(window, std::move(dialog), descriptor, app, nullptr);

    apply->on_press();
    apply->on_press();
    CK_CHECK(applied == 2);
    CK_CHECK(!closed);
}

CK_TEST(closing_the_dialog_restores_focus_to_the_view_that_invoked_it) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    ckv::ui::View* invoker = app.root().add_child(std::make_unique<ckv::ui::View>());
    invoker->set_focus_policy(ckv::ui::FocusPolicy::TabStop);

    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "ok", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);

    Window window = make_window(f);
    wire_dialog_window(window, std::move(dialog), descriptor, app, invoker);

    CK_CHECK(window.on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(app.focused() == invoker);
}

CK_TEST(wire_dialog_window_composes_with_a_previously_installed_on_closed_handler) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);

    Window window = make_window(f);
    bool preexisting_ran = false;
    window.on_closed = [&] { preexisting_ran = true; };
    wire_dialog_window(window, std::move(dialog), descriptor, app, nullptr);

    CK_CHECK(window.on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));
    CK_CHECK(preexisting_ran);  // the original handler still runs, not overwritten
}

// --- Resizable defaults (M10/WP-21) --------------------------------------

CK_TEST(wire_dialog_window_makes_the_window_non_resizable_by_default) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;  // resizable defaults to false
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);

    Window window = make_window(f);
    CK_CHECK(window.resizable());  // a plain Window defaults to resizable

    wire_dialog_window(window, std::move(dialog), descriptor, app, nullptr);

    CK_CHECK(!window.resizable());
}

CK_TEST(wire_dialog_window_honors_an_explicit_resizable_opt_in) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.resizable = true;
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);

    Window window = make_window(f);
    wire_dialog_window(window, std::move(dialog), descriptor, app, nullptr);

    CK_CHECK(window.resizable());
}

// --- Check fields (the internal plans U1-d, Check slice) -------------------------

CK_TEST(a_check_field_materializes_a_checkbox_carrying_its_own_label) {
    // A yes/no question is one sentence — "[X] Start terminals as login
    // shells" — so the box owns the text. There is no separate Label to put
    // in a caption column, and the text field slot stays empty.
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(
        FieldDescriptor{.label = "&Ask every time", .kind = ckv::widgets::FieldKind::Check});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);

    CK_CHECK(dialog.checks.size() == 1U);
    CK_CHECK(dialog.checks[0] != nullptr);
    CK_CHECK(dialog.inputs.size() == 1U);
    CK_CHECK(dialog.inputs[0] == nullptr);
    CK_CHECK(dialog.labels[0] == nullptr);
}

CK_TEST(a_check_field_starts_in_the_state_the_descriptor_asked_for) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(
        FieldDescriptor{.label = "On", .kind = ckv::widgets::FieldKind::Check, .initial_checked = true});
    descriptor.fields.push_back(
        FieldDescriptor{.label = "Off", .kind = ckv::widgets::FieldKind::Check, .initial_checked = false});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);

    CK_CHECK(dialog.checks[0]->checked(0));
    CK_CHECK(!dialog.checks[1]->checked(0));
}

CK_TEST(a_text_field_beside_a_check_field_keeps_its_own_index_in_both_arrays) {
    // The parallel arrays are the contract: field i's answer is at index i
    // whatever kind field i is, so a caller never has to count kinds to find
    // its own value.
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&Name:", "typed", nullptr});
    descriptor.fields.push_back(
        FieldDescriptor{.label = "&Flag", .kind = ckv::widgets::FieldKind::Check, .initial_checked = true});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);

    CK_CHECK(dialog.inputs[0] != nullptr && dialog.checks[0] == nullptr);
    CK_CHECK(dialog.inputs[1] == nullptr && dialog.checks[1] != nullptr);
    CK_CHECK(dialog.labels[0] != nullptr);  // the text field's caption
    CK_CHECK(dialog.labels[1] == nullptr);  // the box carries its own
}

CK_TEST(a_validator_on_a_check_field_is_not_asked_about_text_that_does_not_exist) {
    // validate_dialog walks the fields by index; a checkbox has no text and
    // no invalid marker, and must not be dereferenced as though it had.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    bool validator_ran = false;
    descriptor.fields.push_back(FieldDescriptor{.label = "&Flag",
                                                .validate =
                                                    [&](const std::string&) {
                                                        validator_ran = true;
                                                        return false;
                                                    },
                                                .kind = ckv::widgets::FieldKind::Check});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);

    CK_CHECK(validate_dialog(dialog, descriptor, app));
    CK_CHECK(!validator_ran);
}

CK_TEST(a_note_field_is_text_the_form_says_rather_than_a_field_it_asks) {
    // A settings dialog has to be able to explain itself. The note is a
    // Label: no input, no box, nothing to Tab to, and an empty answer.
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{
        .label = "Applies to terminals opened from now on.", .kind = ckv::widgets::FieldKind::Note});
    descriptor.fields.push_back(
        FieldDescriptor{.label = "&Enabled", .kind = ckv::widgets::FieldKind::Check});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);

    CK_CHECK(dialog.labels[0] != nullptr);
    CK_CHECK(dialog.inputs[0] == nullptr);
    CK_CHECK(dialog.checks[0] == nullptr);
    // The checkbox below it is what the keyboard lands on, not the prose.
    CK_CHECK(dialog.initial_focus == dialog.checks[1]);
}

// --- U4-g: a dialog too tall for its space scrolls, buttons pinned ------
//
// The numbers below are arithmetic, not measurement: a text field's row is
// one cell tall (Label and InputLine both prefer one), the field Column
// spaces by one, and a classic Button prefers two (its own row plus the
// shadow row under it, which the hosting Window's bottom margin absorbs).

namespace {

DialogDescriptor form_with(int fields) {
    DialogDescriptor descriptor;
    descriptor.title = "Form";
    for (int i = 0; i < fields; ++i)
        descriptor.fields.push_back(FieldDescriptor{"&F" + std::to_string(i) + ":", "", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"Save", ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"Cancel", ButtonRole::Dismiss, nullptr});
    return descriptor;
}

ScrollViewport* find_viewport(ckv::ui::View& view) {
    if (auto* viewport = dynamic_cast<ScrollViewport*>(&view)) return viewport;
    for (const auto& child : view.children())
        if (ScrollViewport* found = find_viewport(*child)) return found;
    return nullptr;
}

Button* find_button(ckv::ui::View& view) {
    if (auto* button = dynamic_cast<Button*>(&view)) return button;
    for (const auto& child : view.children())
        if (Button* found = find_button(*child)) return found;
    return nullptr;
}

}  // namespace

CK_TEST(a_dialog_with_room_to_spare_is_laid_out_exactly_where_it_always_was) {
    Fixture f;
    DialogDescriptor descriptor = form_with(3);
    auto dialog = materialize_dialog(descriptor);
    dialog.root->set_bounds(Rect{0, 0, 40, 20});  // 20 rows for the 8 it wants

    CK_CHECK(dialog.content_viewport != nullptr);
    // Three one-row fields with a blank row between them: five rows, and the
    // viewport is exactly that tall rather than claiming the leftover space.
    CK_CHECK(dialog.content_viewport->bounds() == (Rect{0, 0, 40, 5}));
    // No bar at all, so no column taken off the fields either.
    CK_CHECK(!dialog.content_viewport->can_scroll_vertically());
    CK_CHECK(dialog.content_viewport->content()->bounds() == (Rect{0, 0, 40, 5}));
    CK_CHECK(dialog.inputs[0]->parent()->bounds().y == 0);
    CK_CHECK(dialog.inputs[1]->parent()->bounds().y == 2);
    CK_CHECK(dialog.inputs[2]->parent()->bounds().y == 4);
    // One blank row under the last field, then the buttons — where a plain
    // Column of Fixed items put them before any of this existed.
    CK_CHECK(dialog.buttons[0]->parent()->bounds().y == 6);
    CK_CHECK(dialog.buttons[0]->parent()->bounds().height == 2);
}

CK_TEST(a_dialog_that_fits_leaves_the_arrow_keys_to_whatever_else_wants_them) {
    Fixture f;
    auto dialog = materialize_dialog(form_with(3));
    dialog.root->set_bounds(Rect{0, 0, 40, 20});
    CK_CHECK(!dialog.content_viewport->on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(dialog.content_viewport->scroll_y() == 0);
}

CK_TEST(a_dialog_taller_than_its_space_scrolls_and_keeps_its_buttons_on_the_last_rows) {
    Fixture f;
    DialogDescriptor descriptor = form_with(10);  // 19 rows of fields; 22 wanted in all
    auto dialog = materialize_dialog(descriptor);
    dialog.root->set_bounds(Rect{0, 0, 40, 10});

    // 10 - 2 (the button row) - 1 (the blank row above it) = 7 rows of form.
    CK_CHECK(dialog.content_viewport->bounds() == (Rect{0, 0, 40, 7}));
    // Rows 8 and 9: the pane's own last two, which is what "pinned" means.
    CK_CHECK(dialog.buttons[0]->parent()->bounds().y == 8);
    CK_CHECK(dialog.buttons[0]->parent()->bounds().height == 2);
    // The bar takes the rightmost column; the form keeps all 19 of its rows
    // and is looked at through seven of them.
    CK_CHECK(dialog.content_viewport->can_scroll_vertically());
    CK_CHECK(dialog.content_viewport->content()->bounds() == (Rect{0, 0, 39, 19}));
}

CK_TEST(the_scrolled_form_moves_under_the_keyboard_while_the_buttons_stay_put) {
    Fixture f;
    auto dialog = materialize_dialog(form_with(10));
    dialog.root->set_bounds(Rect{0, 0, 40, 10});
    const int buttons_y = dialog.buttons[0]->parent()->bounds().y;

    CK_CHECK(dialog.content_viewport->on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(dialog.content_viewport->scroll_y() == 1);
    CK_CHECK(dialog.content_viewport->content()->bounds().y == -1);
    dialog.content_viewport->on_key(ckv::KeyEvent{KeyChord{Key::End, Modifier::None, ""}});
    CK_CHECK(dialog.content_viewport->scroll_y() == 12);  // 19 rows of form, 7 of them visible
    CK_CHECK(dialog.buttons[0]->parent()->bounds().y == buttons_y);
    CK_CHECK(dialog.content_viewport->bounds().y == 0);
}

CK_TEST(tabbing_to_a_field_below_the_fold_scrolls_it_into_view) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles app_roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), app_roles);

    auto dialog = materialize_dialog(form_with(4));  // rows 0, 2, 4, 6
    dialog.root->set_fills_root(false);
    ckv::ui::View* const pane = app.root().add_child(std::move(dialog.root));
    pane->set_bounds(Rect{0, 0, 40, 6});  // three rows of form, buttons on 4..5
    app.set_focus(dialog.initial_focus);
    CK_CHECK(dialog.content_viewport->bounds().height == 3);
    CK_CHECK(dialog.buttons[0]->parent()->bounds().y == 4);

    const ckv::KeyEvent tab{KeyChord{Key::Tab, Modifier::None, ""}};
    app.dispatch(tab);
    app.step(0);  // the reveal is posted, and posted work runs before the frame
    CK_CHECK(app.focused() == dialog.inputs[1]);  // row 2, the last visible one
    CK_CHECK(dialog.content_viewport->scroll_y() == 0);

    app.dispatch(tab);
    app.step(0);
    CK_CHECK(app.focused() == dialog.inputs[2]);  // row 4: one row past the fold
    CK_CHECK(dialog.content_viewport->scroll_y() == 2);

    app.dispatch(tab);
    app.step(0);
    CK_CHECK(app.focused() == dialog.inputs[3]);  // row 6, and the end of the form
    CK_CHECK(dialog.content_viewport->scroll_y() == 4);

    // Back up again: the near edge wins, so the form scrolls up rather than
    // leaving the field it just focused hanging off the top.
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Tab, Modifier::Shift, ""}});
    app.step(0);
    CK_CHECK(app.focused() == dialog.inputs[2]);
    CK_CHECK(dialog.content_viewport->scroll_y() == 4);  // row 4 is already in the band
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Tab, Modifier::Shift, ""}});
    app.step(0);
    CK_CHECK(app.focused() == dialog.inputs[1]);
    CK_CHECK(dialog.content_viewport->scroll_y() == 2);  // row 2 back at the top of the band
}

CK_TEST(an_accept_veto_scrolls_the_invalid_field_into_view_as_it_focuses_it) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    DialogDescriptor descriptor;
    for (int i = 0; i < 4; ++i)
        descriptor.fields.push_back(FieldDescriptor{"&F:", "filled in", nullptr});
    descriptor.fields.push_back(
        FieldDescriptor{"&Last:", "", [](const std::string& s) { return !s.empty(); }});
    descriptor.buttons.push_back(ButtonDescriptor{"Save", ButtonRole::Accept, nullptr});
    auto dialog = materialize_dialog(descriptor);
    dialog.root->set_bounds(Rect{0, 0, 40, 6});  // five fields at rows 0..8, three rows visible

    CK_CHECK(!validate_dialog(dialog, descriptor, app));
    CK_CHECK(app.focused() == dialog.inputs[4]);
    // A veto that focuses a field the reader cannot see is a dialog that
    // refuses to close without saying why.
    CK_CHECK(dialog.content_viewport->scroll_y() == 6);
}

CK_TEST(a_presented_dialog_scrolls_when_the_terminal_shrinks_and_recovers_when_it_grows) {
    ckv::term::HeadlessTerminal term(ckv::Size{60, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles app_roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), app_roles);
    auto* desktop =
        static_cast<Desktop*>(app.root().add_child(std::make_unique<Desktop>(Rect{0, 0, 60, 24})));

    // Eight one-row fields (15 rows with their gaps), the blank row, the
    // two-row button row, and three rows of window chrome: 21.
    auto presentation = ckv::widgets::present_dialog(form_with(8), app, *desktop, app_roles);
    CK_CHECK(desktop->windows().size() == 1);
    Window* const window = desktop->windows().back();
    ScrollViewport* const viewport = find_viewport(*window);
    Button* const save = find_button(*window);
    CK_CHECK(viewport != nullptr);
    CK_CHECK(save != nullptr);

    CK_CHECK(window->bounds().height == 21);
    CK_CHECK(!viewport->can_scroll_vertically());
    // 1 border + 1 content margin + 15 rows of form + 1 blank row.
    CK_CHECK(save->absolute_bounds().y - window->absolute_bounds().y == 18);

    term.resize(ckv::Size{60, 16});
    CK_CHECK(app.step(0));
    CK_CHECK(window->bounds().height == 16);
    CK_CHECK(viewport->can_scroll_vertically());
    CK_CHECK(viewport->bounds().height == 10);  // 16 - 3 chrome - 2 buttons - 1 gap
    // Two rows above the bottom border, which is where the buttons live in a
    // dialog this size and every size below it.
    CK_CHECK(save->absolute_bounds().y - window->absolute_bounds().y == 13);
    CK_CHECK(save->absolute_bounds().y - window->absolute_bounds().y == window->bounds().height - 3);

    term.resize(ckv::Size{60, 24});
    CK_CHECK(app.step(0));
    CK_CHECK(window->bounds().height == 21);  // grown back to what it recommends
    CK_CHECK(!viewport->can_scroll_vertically());
    CK_CHECK(viewport->bounds().height == 15);
    CK_CHECK(save->absolute_bounds().y - window->absolute_bounds().y == 18);
    (void)presentation;
}
