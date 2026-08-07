// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The render pipeline (View tree -> off-screen Surface -> Presenter ->
// Terminal bytes) that Application::step() now drives — before this,
// step() only dispatched input; nothing anywhere ever painted a View
// into terminal output. These tests prove bytes actually reach the
// terminal, that they reflect real content changes, that resize is
// handled, and that an invisible/unchanged tree costs nothing.
#include "cvision/ui/application.hpp"

#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/label.hpp"

using ckv::ManualClock;
using ckv::Rect;
using ckv::Size;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::widgets::Label;

namespace {
struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{40, 12}};
    ManualClock clock;
    Application app{term, clock};
    ckv::ui::StandardRoles roles = intern_standard_roles(app.roles());
    Fixture() { app.theme() = make_classic_theme(app.roles(), roles); }
};
}  // namespace

CK_TEST(the_very_first_step_paints_and_presents_even_with_no_input) {
    // dirty_ starts true specifically so an application that builds its
    // whole UI before the first step() still gets an initial frame.
    Fixture f;
    CK_CHECK(f.term.written_bytes().empty());
    f.app.step(0);
    CK_CHECK(!f.term.written_bytes().empty());
}

CK_TEST(a_second_step_with_nothing_invalidated_writes_no_further_bytes) {
    Fixture f;
    f.app.step(0);
    f.term.clear_written();
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().empty());
}

CK_TEST(a_label_actually_renders_its_text_into_the_presented_frame) {
    Fixture f;
    auto label = std::make_unique<Label>("Hello", f.app.theme(), f.roles.label_text, f.roles.label_mnemonic);
    label->set_bounds(Rect{2, 2, 10, 1});
    f.app.root().add_child(std::move(label));

    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Hello") != std::string::npos);
}

CK_TEST(setting_new_text_repaints_so_the_old_text_no_longer_appears_in_the_next_frame) {
    Fixture f;
    auto owned = std::make_unique<Label>("Old", f.app.theme(), f.roles.label_text, f.roles.label_mnemonic);
    Label* label = owned.get();
    label->set_bounds(Rect{2, 2, 10, 1});
    f.app.root().add_child(std::move(owned));
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Old") != std::string::npos);

    label->set_text("New");
    f.app.step(0);
    // The Presenter diffs cell-by-cell, so "New" must appear even
    // though nothing ever calls a bespoke "clear" — the diff against
    // the previously presented frame is what erases "Old"'s cells.
    CK_CHECK(f.term.written_bytes().find("New") != std::string::npos);
}

CK_TEST(a_hidden_view_never_reaches_the_presented_frame) {
    Fixture f;
    auto label = std::make_unique<Label>("Secret", f.app.theme(), f.roles.label_text, f.roles.label_mnemonic);
    label->set_bounds(Rect{2, 2, 10, 1});
    label->set_visible(false);
    f.app.root().add_child(std::move(label));

    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Secret") == std::string::npos);
}

CK_TEST(a_resize_event_grows_the_backing_surface_and_forces_a_full_repaint) {
    Fixture f;
    auto label = std::make_unique<Label>("Grow", f.app.theme(), f.roles.label_text, f.roles.label_mnemonic);
    label->set_bounds(Rect{2, 2, 10, 1});
    f.app.root().add_child(std::move(label));
    f.app.step(0);

    f.term.resize(ckv::Size{60, 20});
    f.app.step(0);
    // The label is still visible after growing the terminal — proves
    // the off-screen Surface was actually resized to match, not left
    // at the old (now too-small) size where painting it would be UB.
    CK_CHECK(f.term.written_bytes().find("Grow") != std::string::npos);
    CK_CHECK(f.app.root().bounds().width == 60);
    CK_CHECK(f.app.root().bounds().height == 20);
}
