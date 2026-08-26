// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/splitter.hpp"
#include "layouts_app.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::ui::Application;

namespace {
struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    ckv::layouts::LayoutsApp layouts{app};
};
}  // namespace

CK_TEST(layouts_about_dialog_carries_the_project_copyright) {
    Fixture f;
    CK_CHECK(f.app.execute_command(f.app.commands().standard().help));
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find(
                 "Copyright (c) 2026 C. Klukas. All rights reserved.") != std::string::npos);
}

CK_TEST(layouts_example_renders_each_layout_family) {
    Fixture f;
    f.app.step(0);
    const auto bytes = f.term.written_bytes();
    CK_CHECK(bytes.find("Layouts") != std::string::npos);
    CK_CHECK(bytes.find("Row") != std::string::npos);
    CK_CHECK(bytes.find("Column") != std::string::npos);
    CK_CHECK(bytes.find("Grid") != std::string::npos);
    CK_CHECK(bytes.find("Dock") != std::string::npos);
    CK_CHECK(bytes.find("Overlay") != std::string::npos);
    CK_CHECK(bytes.find("Anchored") != std::string::npos);
}

CK_TEST(layouts_splitter_is_keyboard_adjustable_through_the_example_graph) {
    Fixture f;
    f.app.step(0);
    const int before = f.layouts.splitter()->split_position();
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    CK_CHECK(f.layouts.splitter()->split_position() == before + 1);
}

CK_TEST(layouts_example_reflows_docked_chrome_and_anchored_content_after_terminal_resize) {
    Fixture f;
    f.app.step(0);
    const ckv::Rect anchored_before = f.layouts.anchored_label()->bounds();

    f.term.resize(ckv::Size{100, 30});
    CK_CHECK(f.app.step(0));

    CK_CHECK(f.layouts.desktop().top_dock()->bounds() == (ckv::Rect{0, 0, 100, 1}));
    CK_CHECK(f.layouts.desktop().bottom_dock()->bounds() == (ckv::Rect{0, 29, 100, 1}));
    CK_CHECK(f.layouts.anchored_label()->bounds().x > anchored_before.x);
}

CK_TEST(alt_x_quits_from_the_layouts_example) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(!f.app.quit_requested());
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}});
    CK_CHECK(f.app.quit_requested());
}
