// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Focus is one question with one answer (the architecture §5 "Focus and
// traversal", D-065): a view asks its Application whether it is focused, and
// the arrow keys a focused control does not use walk the window it sits in.
#include "cvision/testing/cktest.hpp"
#include "cvision/core/clock.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/layout.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/window.hpp"

#include <memory>

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Rect;
using ckv::ui::Application;
using ckv::ui::Column;
using ckv::ui::Row;
using ckv::widgets::Button;
using ckv::widgets::Desktop;
using ckv::widgets::InputLine;
using ckv::widgets::Window;

namespace {

ckv::KeyEvent key(Key k, Modifier modifiers = Modifier::None) {
    return ckv::KeyEvent{KeyChord{k, modifiers, ""}};
}

struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;

    Fixture() {
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        desktop = static_cast<Desktop*>(app.root().add_child(std::make_unique<Desktop>(Rect{0, 0, 80, 24})));
    }
};

// A window holding a text field above a row of two buttons -- the shape of
// every form and every message box.
struct Form {
    Window* window = nullptr;
    InputLine* input = nullptr;
    Button* ok = nullptr;
    Button* cancel = nullptr;
};

Form add_form(Fixture& f, const std::string& title, Rect bounds) {
    Form form;
    auto window = std::make_unique<Window>(title);
    window->set_bounds(bounds);
    auto column = std::make_unique<Column>();
    auto input = std::make_unique<InputLine>();
    input->set_text("hello");
    form.input = static_cast<InputLine*>(column->add_item(std::move(input)));
    auto row = std::make_unique<Row>();
    form.ok = static_cast<Button*>(row->add_item(std::make_unique<Button>("OK")));
    form.cancel = static_cast<Button*>(row->add_item(std::make_unique<Button>("Cancel")));
    column->add_item(std::move(row));
    window->set_content(std::move(column));
    form.window = f.desktop->add_window(std::move(window));
    return form;
}

}  // namespace

CK_TEST(a_view_asks_its_application_whether_it_has_focus) {
    Fixture f;
    const Form form = add_form(f, "Form", Rect{2, 2, 40, 8});
    CK_CHECK(!form.ok->has_focus());
    f.app.set_focus(form.ok);
    CK_CHECK(form.ok->has_focus());
    CK_CHECK(!form.cancel->has_focus());
    f.app.set_focus(form.cancel);
    CK_CHECK(!form.ok->has_focus());
    CK_CHECK(form.cancel->has_focus());
    f.app.set_focus(nullptr);
    CK_CHECK(!form.cancel->has_focus());
}

CK_TEST(a_view_with_no_application_in_its_context_is_never_focused) {
    // The notification alone confers nothing: a standalone view told it
    // gained focus has nobody to have given it, and says so.
    Button button("OK");
    button.on_focus(ckv::FocusEvent{true});
    CK_CHECK(!button.has_focus());
}

CK_TEST(inside_its_own_focus_out_a_view_already_answers_unfocused) {
    // What a view leaves on screen when focus goes is decided inside
    // on_focus(false); the Application has moved on by then, and the view's
    // question must get the settled answer, not the previous one.
    class Probe : public ckv::ui::View {
    public:
        Probe() : View(Rect{}, ckv::ui::FocusPolicy::TabStop) {}
        void on_focus(const ckv::FocusEvent& event) override {
            if (!event.gained) still_focused_on_loss = has_focus();
            if (event.gained) focused_on_gain = has_focus();
        }
        bool still_focused_on_loss = true;
        bool focused_on_gain = false;
    };
    Fixture f;
    auto owned = std::make_unique<Probe>();
    Probe* const probe = owned.get();
    f.desktop->add_child(std::move(owned));
    f.app.set_focus(probe);
    CK_CHECK(probe->focused_on_gain);
    f.app.set_focus(nullptr);
    CK_CHECK(!probe->still_focused_on_loss);
}

CK_TEST(a_view_detached_while_focused_is_not_focused_when_it_comes_back) {
    // The case a cached flag got wrong: the window leaves with its button
    // focused and no focus-out is delivered (the sink may not call into a
    // view that could be mid-destruction). Put back, the button used to go on
    // drawing itself focused beside whatever really had the focus.
    Fixture f;
    const Form form = add_form(f, "Form", Rect{2, 2, 40, 8});
    f.app.set_focus(form.ok);
    CK_CHECK(form.ok->has_focus());
    std::unique_ptr<Window> owned = f.desktop->remove_window(form.window);
    CK_CHECK(owned != nullptr);
    CK_CHECK(f.app.focused() == nullptr);
    CK_CHECK(!form.ok->has_focus());
    f.desktop->add_window(std::move(owned));
    CK_CHECK(!form.ok->has_focus());
}

CK_TEST(arrow_keys_walk_a_windows_controls_when_the_focused_control_declines_them) {
    Fixture f;
    const Form form = add_form(f, "Form", Rect{2, 2, 40, 8});
    f.app.set_focus(form.ok);

    // A button means nothing by an arrow, so Right and Left move along the
    // row, Down and Up along the column; the walk wraps within the window.
    CK_CHECK(f.app.dispatch(key(Key::Right)));
    CK_CHECK(f.app.focused() == form.cancel);
    CK_CHECK(f.app.dispatch(key(Key::Left)));
    CK_CHECK(f.app.focused() == form.ok);
    CK_CHECK(f.app.dispatch(key(Key::Up)));
    CK_CHECK(f.app.focused() == form.input);
    CK_CHECK(f.app.dispatch(key(Key::Down)));
    CK_CHECK(f.app.focused() == form.ok);
    CK_CHECK(f.app.dispatch(key(Key::Down)));
    CK_CHECK(f.app.dispatch(key(Key::Down)));
    CK_CHECK(f.app.focused() == form.input);  // wrapped past Cancel

    // A control that uses the arrow keeps it: Left in a text field moves the
    // caret, and the field stays focused. Up, which the field has no use for,
    // walks on as before.
    CK_CHECK(f.app.dispatch(key(Key::End)));
    const std::size_t at_end = form.input->cursor();
    CK_CHECK(at_end > 0);
    CK_CHECK(f.app.dispatch(key(Key::Left)));
    CK_CHECK(f.app.focused() == form.input);
    CK_CHECK(form.input->cursor() + 1 == at_end);
    CK_CHECK(f.app.dispatch(key(Key::Up)));
    CK_CHECK(f.app.focused() == form.cancel);
}

CK_TEST(arrow_traversal_stays_inside_the_window_the_focus_is_in) {
    Fixture f;
    const Form first = add_form(f, "First", Rect{2, 2, 36, 8});
    const Form second = add_form(f, "Second", Rect{40, 2, 36, 8});
    f.app.set_focus(first.cancel);
    // Right from the last control wraps to the first control of the SAME
    // window; the other window's controls are not part of this walk.
    CK_CHECK(f.app.dispatch(key(Key::Right)));
    CK_CHECK(f.app.focused() == first.input);
    CK_CHECK(f.app.focused() != second.input);
}

CK_TEST(a_modified_arrow_is_left_to_the_focused_control) {
    Fixture f;
    const Form form = add_form(f, "Form", Rect{2, 2, 40, 8});
    f.app.set_focus(form.ok);
    // Shift+Right is a selection gesture somewhere, not a walk anywhere: a
    // button declines it and so does its window.
    CK_CHECK(!f.app.dispatch(key(Key::Right, Modifier::Shift)));
    CK_CHECK(f.app.focused() == form.ok);
}
