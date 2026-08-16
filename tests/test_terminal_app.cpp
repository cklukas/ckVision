// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// POSIX integration coverage for the runnable contained-terminal example.
// The example intentionally launches real private PTYs, so this remains a
// platform suite rather than pretending the shell process is portable.
#if !defined(_WIN32)

#include <array>

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

struct Fixture {
    ckv::term::HeadlessTerminal terminal{Size{100, 30}};
    ManualClock clock;
    Application app{terminal, clock};
    ckv::terminal_example::TerminalApp example{app};
};

bool menu_has_command(const ckv::widgets::MenuBarItem& menu, ckv::ui::CommandId command) {
    for (const ckv::widgets::MenuItem& item : menu.items)
        if (item.command == command || item.presentation.command == command) return true;
    return false;
}

}  // namespace

CK_TEST(terminal_example_exposes_new_terminal_and_window_arrangement_commands) {
    Fixture f;
    f.app.step(0);

    auto* const menu_bar = dynamic_cast<ckv::widgets::MenuBar*>(f.example.desktop().top_dock());
    CK_CHECK(menu_bar != nullptr);
    CK_CHECK(menu_bar->menus().size() == 2U);
    CK_CHECK(menu_has_command(menu_bar->menus()[0], ckv::terminal_example::TerminalApp::kNewTerminalCommand));
    CK_CHECK(menu_has_command(menu_bar->menus()[0], ckv::terminal_example::TerminalApp::kNewSixelDemoCommand));
    CK_CHECK(menu_has_command(menu_bar->menus()[1], ckv::ui::std_commands::kNextWindow));
    CK_CHECK(menu_has_command(menu_bar->menus()[1], ckv::ui::std_commands::kPreviousWindow));
    CK_CHECK(menu_has_command(menu_bar->menus()[1], ckv::ui::std_commands::kTile));
    CK_CHECK(menu_has_command(menu_bar->menus()[1], ckv::ui::std_commands::kCascade));
}

CK_TEST(terminal_example_opens_switches_arranges_and_closes_independent_terminal_windows) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(f.example.desktop().windows().size() == 1U);
    CK_CHECK(f.example.desktop().windows()[0]->title() == "Terminal 1");

    CK_CHECK(f.app.commands().execute(ckv::terminal_example::TerminalApp::kNewTerminalCommand));
    CK_CHECK(f.example.desktop().windows().size() == 2U);
    ckv::widgets::Window* const second = f.example.desktop().active_window();
    CK_CHECK(second != nullptr);
    CK_CHECK(second->title() == "Terminal 2");

    CK_CHECK(f.app.commands().execute(ckv::ui::std_commands::kPreviousWindow));
    CK_CHECK(f.example.desktop().active_window()->title() == "Terminal 1");
    CK_CHECK(f.app.commands().execute(ckv::ui::std_commands::kNextWindow));
    CK_CHECK(f.example.desktop().active_window() == second);

    CK_CHECK(dynamic_cast<ckv::widgets::TerminalView*>(second->content()) != nullptr);
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl | ckv::Modifier::Alt, " "}});
    CK_CHECK(f.app.focused() == f.example.desktop().top_dock());
    f.app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F6, ckv::Modifier::None, ""}});
    CK_CHECK(f.example.desktop().active_window()->title() == "Terminal 1");
    CK_CHECK(f.app.commands().execute(ckv::ui::std_commands::kNextWindow));
    CK_CHECK(f.example.desktop().active_window() == second);

    CK_CHECK(f.app.commands().execute(ckv::ui::std_commands::kTile));
    CK_CHECK(f.example.desktop().windows()[0]->bounds().width == 50);
    CK_CHECK(f.example.desktop().windows()[1]->bounds().width == 50);
    CK_CHECK(f.app.commands().execute(ckv::ui::std_commands::kCascade));
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
    CK_CHECK(f.app.commands().execute(ckv::terminal_example::TerminalApp::kNewSixelDemoCommand));
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
