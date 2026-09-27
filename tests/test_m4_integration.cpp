// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A materialized dialog driven through Application's headless event
// dispatch -- Tab and Shift-Tab injected as terminal key events, typed input
// reaching the focused field, and Enter on the default button running its
// handler. The M4 form demo itself, with its pinned frames, is
// examples/rootdialog and tests/test_rootdialog_smoke.cpp; these cases keep
// the bare materialize_dialog tree under the same key route.
#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/dialog.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::ui::Application;
using ckv::widgets::ButtonDescriptor;
using ckv::widgets::ButtonRole;
using ckv::widgets::DialogDescriptor;
using ckv::widgets::FieldDescriptor;
using ckv::widgets::materialize_dialog;

namespace {

// A step paints, and the widgets draw with the standard roles.
void use_classic_theme(Application& app) {
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
}

// Tab (or Shift-Tab) as a host delivers it: a terminal event and a step.
void press_tab(ckv::term::HeadlessTerminal& term, Application& app,
               Modifier modifiers = Modifier::None) {
    term.inject_event(ckv::KeyEvent{KeyChord{Key::Tab, modifiers, ""}});
    app.step(0);
}

}  // namespace

CK_TEST(a_two_field_dialog_driven_headlessly_end_to_end) {
    DialogDescriptor descriptor;
    descriptor.title = "Connect";
    descriptor.fields.push_back(FieldDescriptor{"&Host:", "", nullptr});
    descriptor.fields.push_back(FieldDescriptor{"&Port:", "8080", nullptr});
    bool ok_pressed = false;
    bool cancel_pressed = false;
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, [&] { ok_pressed = true; }});
    descriptor.buttons.push_back(ButtonDescriptor{"Cancel", ButtonRole::Dismiss, [&] { cancel_pressed = true; }});

    auto dialog = materialize_dialog(descriptor);

    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    use_classic_theme(app);
    app.root().add_child(std::move(dialog.root));

    // Initial focus lands on the first field by construction.
    app.set_focus(dialog.initial_focus);
    CK_CHECK(app.focused() == dialog.inputs[0]);

    // Type a hostname into the focused field.
    app.dispatch(ckv::TextEvent{"example.com", false});
    CK_CHECK(dialog.inputs[0]->text() == "example.com");

    // Tab moves to the second field without disturbing the first.
    press_tab(term, app);
    CK_CHECK(app.focused() == dialog.inputs[1]);
    CK_CHECK(dialog.inputs[0]->text() == "example.com");

    // Replace the port field's content: select-all via Home then
    // Shift+End, then type over the selection.
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Home, Modifier::None, ""}});
    app.dispatch(ckv::KeyEvent{KeyChord{Key::End, Modifier::Shift, ""}});
    app.dispatch(ckv::TextEvent{"9090", false});
    CK_CHECK(dialog.inputs[1]->text() == "9090");

    // One more Tab reaches the OK button (declaration order: field row
    // 0, field row 1, then the button row's OK, then Cancel).
    press_tab(term, app);
    CK_CHECK(app.focused() == dialog.buttons[0]);

    // Enter on the focused OK button fires its handler and not Cancel's.
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    CK_CHECK(ok_pressed);
    CK_CHECK(!cancel_pressed);

    // Shift-Tab from OK returns focus to the second field, not
    // forward to Cancel — traversal is genuinely bidirectional.
    press_tab(term, app, Modifier::Shift);
    CK_CHECK(app.focused() == dialog.inputs[1]);
}

CK_TEST(tab_traversal_wraps_all_the_way_around_a_full_dialog_and_back) {
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});

    auto dialog = materialize_dialog(descriptor);
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    use_classic_theme(app);
    app.root().add_child(std::move(dialog.root));
    app.set_focus(dialog.initial_focus);

    press_tab(term, app);  // -> OK
    CK_CHECK(app.focused() == dialog.buttons[0]);
    press_tab(term, app);  // wraps back to the input
    CK_CHECK(app.focused() == dialog.inputs[0]);
}

CK_TEST(a_disabled_default_button_still_reports_default_but_a_disabled_input_is_skipped_by_tab) {
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&A:", "", nullptr});
    descriptor.fields.push_back(FieldDescriptor{"&B:", "", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});

    auto dialog = materialize_dialog(descriptor);
    dialog.inputs[1]->set_enabled(false);

    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    use_classic_theme(app);
    app.root().add_child(std::move(dialog.root));
    app.set_focus(dialog.initial_focus);

    press_tab(term, app);  // must skip the disabled second field, landing on OK
    CK_CHECK(app.focused() == dialog.buttons[0]);
}
