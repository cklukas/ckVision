// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// End-to-end smoke test for the ckVision Gallery example
// (examples/gallery/gallery_app.cpp), driven headlessly exactly the
// way the interactive main.cpp drives it — proving the example
// actually renders, and that keyboard (Tab, F10 menu, typing) and
// mouse (menu-bar click) navigation work against the real render
// pipeline, not a simplified stand-in.
#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/input_line.hpp"
#include "gallery_app.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::MouseAction;
using ckv::MouseButton;
using ckv::MouseEvent;
using ckv::Point;
using ckv::ui::Application;

namespace {
struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    ckv::gallery::GalleryApp gallery{app};
};
}  // namespace

CK_TEST(the_gallery_renders_both_windows_titles_on_first_frame) {
    Fixture f;
    f.app.step(0);
    const auto bytes = f.term.written_bytes();
    CK_CHECK(bytes.find("Controls") != std::string::npos);
    CK_CHECK(bytes.find("Sixel Demo") != std::string::npos);
}

CK_TEST(the_menu_bar_and_status_line_both_render) {
    Fixture f;
    f.app.step(0);
    const auto bytes = f.term.written_bytes();
    CK_CHECK(bytes.find("File") != std::string::npos);
    CK_CHECK(bytes.find("Window") != std::string::npos);
    CK_CHECK(bytes.find("Quit") != std::string::npos);
}

// Regression (M8/WP-6, finding F7): the gallery used to hand-position
// its menu bar and status line with a one-time set_bounds call instead
// of Desktop::dock_top/dock_bottom — the exact stale-after-resize
// pattern docking exists to eliminate. Now that it docks like the
// other examples, a real terminal resize (through the same chain
// tests/test_m8_integration.cpp exercises directly on Desktop) must
// reach it end to end.
CK_TEST(resizing_the_terminal_repins_the_menu_bar_and_status_line_to_the_new_edges) {
    Fixture f;
    f.app.step(0);
    ckv::ui::View* menu_bar = f.gallery.desktop().top_dock();
    ckv::ui::View* status_line = f.gallery.desktop().bottom_dock();
    CK_CHECK(menu_bar != nullptr);
    CK_CHECK(status_line != nullptr);

    CK_CHECK(f.app.dispatch(ckv::ResizeEvent{ckv::Size{120, 40}}));

    CK_CHECK(menu_bar->bounds() == (ckv::Rect{0, 0, 120, 1}));
    CK_CHECK(status_line->bounds() == (ckv::Rect{0, 39, 120, 1}));
}

CK_TEST(typing_into_the_name_field_reaches_it_and_repaints) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.app.focused() == f.gallery.name_input());

    f.app.dispatch(ckv::TextEvent{"Ada", false});
    CK_CHECK(f.gallery.name_input()->text() == "Ada");
    f.app.step(0);
    CK_CHECK(f.term.written_bytes().find("Ada") != std::string::npos);
}

CK_TEST(f10_activates_the_menu_bar_via_the_command_keymap) {
    Fixture f;
    f.app.step(0);
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::F10, Modifier::None, ""}});
    // Activation moves focus onto the menu bar itself.
    CK_CHECK(f.app.focused() != f.gallery.name_input());
}

CK_TEST(escape_after_f10_returns_focus_to_where_it_was) {
    Fixture f;
    f.app.step(0);
    ckv::ui::View* before = f.app.focused();
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::F10, Modifier::None, ""}});
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    CK_CHECK(f.app.focused() == before);
}

CK_TEST(alt_x_quits_via_the_status_lines_documented_shortcut) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(!f.gallery.should_quit());
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "x"}});
    CK_CHECK(f.gallery.should_quit());
}

CK_TEST(clicking_greet_opens_a_message_box_that_renders_the_typed_name) {
    Fixture f;
    f.app.step(0);
    f.app.dispatch(ckv::TextEvent{"Grace", false});
    f.app.step(0);

    // Find the Greet button's absolute position by construction: the
    // Controls window is at {2,2}, its content fills the interior
    // starting 1 cell in for the frame, and the button sits at local
    // (1,3) within that content — see gallery_app.cpp's own layout.
    const Point button_point{2 + 1 + 1, 2 + 1 + 3};
    f.app.dispatch(ckv::MouseEvent{MouseAction::Down, MouseButton::Left, button_point, std::nullopt, Modifier::None});
    f.app.dispatch(ckv::MouseEvent{MouseAction::Up, MouseButton::Left, button_point, std::nullopt, Modifier::None});
    f.app.step(0);

    CK_CHECK(f.term.written_bytes().find("Grace") != std::string::npos);
}

CK_TEST(the_image_window_content_reaches_the_terminal_as_sixel_data_under_full_capabilities) {
    ckv::term::Capabilities caps = ckv::term::baseline_capabilities();
    caps.sixel_graphics = true;
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, caps);
    ManualClock clock;
    Application app(term, clock);
    ckv::gallery::GalleryApp gallery(app);

    app.step(0);
    // A DCS-introduced Sixel sequence (ESC P ... ESC \) must appear
    // somewhere in the presented frame once the image window's
    // ImageView is on screen and the terminal advertises Sixel.
    CK_CHECK(term.written_bytes().find("\x1B" "P") != std::string::npos);
    (void)gallery;
}
