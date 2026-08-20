// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// POSIX integration coverage for the runnable contained-terminal example.
// The example intentionally launches real private PTYs, so this remains a
// platform suite rather than pretending the shell process is portable.
#if !defined(_WIN32)

#include <array>

#include <chrono>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/terminal_view.hpp"
#include "terminal_app.hpp"

using ckv::ManualClock;
using ckv::Size;
using ckv::ui::Application;

namespace {

// The framework's own commands, by name. A test names the concept and
// asks the registry that assigned the ids, exactly as application code
// does — no test knows or states a command's number.
const ckv::ui::StandardCommands& standard(const ckv::ui::Application& app) {
    return app.commands().standard();
}

struct Fixture {
    // Graphics-capable, because the point of this example is graphics: a
    // terminal without them draws every raster's fallback instead, which
    // makes a test unable to tell a working picture from a missing one.
    ckv::term::HeadlessTerminal terminal{Size{100, 30}, ckv::term::headless_sixel_profile()};
    ManualClock clock;
    Application app{terminal, clock};
    ckv::terminal_example::TerminalApp example{app};
};

bool menu_has_command(const ckv::widgets::MenuBarItem& menu, ckv::ui::CommandId command) {
    for (const ckv::widgets::MenuItem& item : menu.items)
        if (item.command() == command) return true;
    return false;
}

}  // namespace

CK_TEST(terminal_example_exposes_new_terminal_and_window_arrangement_commands) {
    Fixture f;
    f.app.step(0);

    auto* const menu_bar = dynamic_cast<ckv::widgets::MenuBar*>(f.example.desktop().top_dock());
    CK_CHECK(menu_bar != nullptr);
    CK_CHECK(menu_bar->menus().size() == 3U);
    CK_CHECK(menu_has_command(menu_bar->menus()[0], f.example.new_terminal_command()));
    CK_CHECK(menu_has_command(menu_bar->menus()[0], f.example.new_sixel_demo_command()));
    CK_CHECK(menu_has_command(menu_bar->menus()[1], standard(f.app).next_window));
    CK_CHECK(menu_has_command(menu_bar->menus()[1], standard(f.app).previous_window));
    CK_CHECK(menu_has_command(menu_bar->menus()[1], standard(f.app).tile));
    CK_CHECK(menu_has_command(menu_bar->menus()[1], standard(f.app).cascade));
    CK_CHECK(menu_has_command(menu_bar->menus()[2], standard(f.app).help));
}

CK_TEST(terminal_example_opens_switches_arranges_and_closes_independent_terminal_windows) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.example.desktop().windows().size() == 1U);
    CK_CHECK(f.example.desktop().windows()[0]->title() == "Terminal 1");

    CK_CHECK(f.app.commands().execute(f.example.new_terminal_command()));
    CK_CHECK(f.example.desktop().windows().size() == 2U);
    ckv::widgets::Window* const second = f.example.desktop().active_window();
    CK_CHECK(second != nullptr);
    CK_CHECK(second->title() == "Terminal 2");

    CK_CHECK(f.app.commands().execute(standard(f.app).previous_window));
    CK_CHECK(f.example.desktop().active_window()->title() == "Terminal 1");
    CK_CHECK(f.app.commands().execute(standard(f.app).next_window));
    CK_CHECK(f.example.desktop().active_window() == second);

    CK_CHECK(dynamic_cast<ckv::widgets::TerminalView*>(second->content()) != nullptr);
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl | ckv::Modifier::Alt, " "}});
    CK_CHECK(f.app.focused() == f.example.desktop().top_dock());
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F6, ckv::Modifier::None, ""}});
    CK_CHECK(f.example.desktop().active_window()->title() == "Terminal 1");
    CK_CHECK(f.app.commands().execute(standard(f.app).next_window));
    CK_CHECK(f.example.desktop().active_window() == second);

    CK_CHECK(f.app.commands().execute(standard(f.app).tile));
    CK_CHECK(f.example.desktop().windows()[0]->bounds().width == 50);
    CK_CHECK(f.example.desktop().windows()[1]->bounds().width == 50);
    CK_CHECK(f.app.commands().execute(standard(f.app).cascade));
    CK_CHECK(f.example.desktop().windows()[0]->bounds().x < f.example.desktop().windows()[1]->bounds().x);

    auto* const second_view = dynamic_cast<ckv::widgets::TerminalView*>(second->content());
    CK_CHECK(second_view != nullptr);
    second->set_bounds(ckv::Rect{4, 3, 42, 15});
    f.app.step(0);
    const ckv::Rect resized_content = second->content_rect();
    CK_CHECK(second_view->session().snapshot().cells ==
             (ckv::Size{resized_content.width, resized_content.height}));

    CK_CHECK(second->close());
    f.app.step(0);
    CK_CHECK(f.example.desktop().windows().size() == 1U);
}

CK_TEST(terminal_example_public_mouse_menu_path_opens_a_new_terminal) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.example.desktop().windows().size() == 1U);

    // File is the first top-level label. The popup's first selectable row is
    // one cell below its framed top border.
    CK_CHECK(f.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                            ckv::Point{1, 0}, std::nullopt, ckv::Modifier::None}));
    CK_CHECK(f.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                            ckv::Point{2, 2}, std::nullopt, ckv::Modifier::None}));
    CK_CHECK(f.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left,
                                            ckv::Point{2, 2}, std::nullopt, ckv::Modifier::None}));
    CK_CHECK(f.example.desktop().windows().size() == 2U);
    CK_CHECK(f.example.desktop().windows()[1]->title() == "Terminal 2");
}

CK_TEST(terminal_example_opens_a_contained_sixel_demo_in_an_independent_window) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.app.commands().execute(f.example.new_sixel_demo_command()));
    CK_CHECK(f.example.desktop().windows().size() == 2U);
    CK_CHECK(f.example.desktop().active_window()->title() == "Sixel Demo 2");
    CK_CHECK(dynamic_cast<ckv::widgets::TerminalView*>(f.example.desktop().active_window()->content()) != nullptr);
}

CK_TEST(terminal_example_public_menu_path_survives_all_builtin_themes) {
    using ThemeFactory = ckv::ui::Theme (*)(const ckv::ui::RoleRegistry&,
                                             const ckv::ui::StandardRoles&);
    constexpr std::array<ThemeFactory, 4> factories{
        ckv::ui::make_classic_theme,
        ckv::ui::make_dark_theme,
        ckv::ui::make_light_theme,
        ckv::ui::make_mono_theme,
    };

    for (const ThemeFactory factory : factories) {
        Fixture f;
        const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(f.app.roles());
        f.app.theme() = factory(f.app.roles(), roles);
        f.app.step(0);
        CK_CHECK(f.example.desktop().windows().size() == 1U);

        f.app.set_focus(f.example.desktop().top_dock());
        CK_CHECK(f.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                                ckv::Point{1, 0}, std::nullopt, ckv::Modifier::None}));
        CK_CHECK(f.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                                ckv::Point{2, 2}, std::nullopt, ckv::Modifier::None}));
        CK_CHECK(f.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left,
                                                ckv::Point{2, 2}, std::nullopt, ckv::Modifier::None}));
        f.app.step(0);
        CK_CHECK(f.example.desktop().windows().size() == 2U);
    }
}

#endif

CK_TEST(the_sixel_demo_child_actually_delivers_a_picture_of_useful_size) {
    // The point of the demo is that a child's graphics arrive as a scene
    // raster. It used to emit a solid rectangle four cells wide, which a
    // reader could not tell from a rendering fault -- and then buried it
    // under a shell prompt.
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.app.commands().execute(f.example.new_sixel_demo_command()));
    auto* view = dynamic_cast<ckv::widgets::TerminalView*>(f.example.desktop().active_window()->content());
    CK_CHECK(view != nullptr);
    // Waited out in real time rather than in iterations: a child process
    // needs the same fraction of a second to start and draw however fast the
    // loop asking about it runs, and an optimised build spends 400 turns of
    // this loop in less time than the fork takes.
    bool sized = false;
    const auto sized_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!sized && std::chrono::steady_clock::now() < sized_deadline) {
        f.app.step(0);
        for (const auto& raster : view->session().snapshot().rasters)
            if (raster.cell_extent.width >= 40 && raster.cell_extent.height >= 8) sized = true;
    }
    CK_CHECK(sized);

    // ...and the child got it through intact. A picture too large for the
    // subsession's output budget is discarded whole, which presents as an
    // empty window with the reason recorded only in a diagnostic. Checking
    // the raster alone does not catch that: the sizes still look right on
    // whichever fragment did arrive.
    for (const auto& diagnostic : view->session().snapshot().diagnostics)
        CK_CHECK(diagnostic.kind != ckv::core::TerminalDiagnostic::Kind::LimitExceeded);

    // ...and it reaches the screen. Asking the emulator whether it holds a
    // raster answers a different question from whether anything is drawn:
    // the last two attempts at this both passed while the window stayed
    // blank. A raster the view declines to draw is worth nothing.
    for (const auto& raster : view->session().snapshot().rasters)
        CK_CHECK(raster.id != 0);  // TerminalView skips id 0 outright
    bool on_screen = false;
    const auto screen_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!on_screen && std::chrono::steady_clock::now() < screen_deadline) {
        f.app.step(0);
        on_screen = f.terminal.display().has_raster_pixels();
    }
    CK_CHECK(on_screen);
}

CK_TEST(the_help_command_answers_with_an_about_box) {
    // A focused terminal forwards F1 to its child, as a terminal should, so
    // this application reaches About through its Help menu instead. The
    // command is the same one F1 runs everywhere else.
    Fixture f;
    f.app.step(0);
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.app.commands().execute(standard(f.app).help));
    f.app.step(0);
    CK_CHECK(f.app.is_modal());
}

CK_TEST(the_clock_sits_at_the_right_end_of_the_menu_bar_and_stays_there_on_resize) {
    // Right-aligned means recomputed, not remembered: after a resize the
    // right edge is somewhere else, and the clock has to be there too.
    Fixture f;
    f.app.step(0);
    auto* const menu_bar = dynamic_cast<ckv::widgets::MenuBar*>(f.example.desktop().top_dock());
    CK_CHECK(menu_bar != nullptr);
    auto* const clock = dynamic_cast<ckv::widgets::ClockView*>(menu_bar->trailing_view());
    CK_CHECK(clock != nullptr);
    CK_CHECK(clock->bounds().right() == menu_bar->bounds().width);
    CK_CHECK(clock->bounds().width > 0);

    f.terminal.resize(Size{140, 40});
    CK_CHECK(f.app.step(0));
    CK_CHECK(menu_bar->bounds().width == 140);
    CK_CHECK(clock->bounds().right() == menu_bar->bounds().width);
}

CK_TEST(clicking_the_clock_drops_a_calendar_out_of_it) {
    Fixture f;
    f.app.step(0);
    auto* const menu_bar = dynamic_cast<ckv::widgets::MenuBar*>(f.example.desktop().top_dock());
    auto* const clock = dynamic_cast<ckv::widgets::ClockView*>(menu_bar->trailing_view());
    CK_CHECK(clock != nullptr);
    CK_CHECK(f.app.input_capture() == nullptr);

    // A completed click, since the clock acts on release like every other
    // command-like control here.
    const ckv::Rect at = clock->absolute_bounds();
    CK_CHECK(clock->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                              ckv::Point{at.x, at.y}, std::nullopt, ckv::Modifier::None}));
    CK_CHECK(clock->on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left,
                                              ckv::Point{at.x, at.y}, std::nullopt, ckv::Modifier::None}));
    f.app.step(0);
    CK_CHECK(f.app.input_capture() != nullptr);  // the calendar took capture
}
