// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/status_line.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::MouseAction;
using ckv::MouseButton;
using ckv::MouseEvent;
using ckv::Point;
using ckv::Rect;
using ckv::Size;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::term::HeadlessTerminal;
using ckv::ui::Application;
using ckv::ui::FocusPolicy;
using ckv::ui::StandardRoles;
using ckv::ui::View;
namespace ui = ckv::ui;
namespace widgets = ckv::widgets;

namespace {

// The framework's own commands, by name. A test names the concept and
// asks the registry that assigned the ids, exactly as application code
// does — no test knows or states a command's number.
const ckv::ui::StandardCommands& standard(const ckv::ui::Application& app) {
    return app.commands().standard();
}
std::string row_text(const Surface& surface) {
    std::string out;
    for (int x = 0; x < surface.size().width; ++x)
        out += surface.at(Point{x, 0}).grapheme();
    return out;
}

struct AppFixture {
    HeadlessTerminal terminal{Size{80, 24}};
    ManualClock clock;
    Application app{terminal, clock};
    StandardRoles roles = ui::intern_standard_roles(app.roles());

    AppFixture() { app.theme() = ui::make_classic_theme(app.roles(), roles); }
};
}  // namespace

CK_TEST(command_contexts_activate_through_explicit_scopes_and_focus_ancestry) {
    AppFixture f;
    int ran = 0;
    const ui::CommandId scoped = f.app.commands().declare({.key = "test.scoped",
                                                           .title = "Scoped",
                                                           .category = "Test",
                                                           .context = "editor",
                                                           .handler = [&] { ++ran; }});

    CK_CHECK(!f.app.execute_command(scoped));
    const auto scope = f.app.commands().push_context("editor");
    CK_CHECK(f.app.execute_command(scoped));
    CK_CHECK(ran == 1);
    CK_CHECK(f.app.commands().pop_context(scope));
    CK_CHECK(!f.app.commands().pop_context(scope));

    auto* owner = f.app.root().add(std::make_unique<View>());
    auto* child = owner->add(std::make_unique<View>());
    owner->set_command_context("editor");
    child->set_focus_policy(FocusPolicy::TabStop);
    f.app.set_focus(child);

    CK_CHECK(f.app.execute_command(scoped));
    CK_CHECK(ran == 2);
}

CK_TEST(modal_dispatch_allows_modal_focus_context_commands_but_not_unscoped_background_commands) {
    AppFixture f;
    int scoped = 0;
    int background = 0;
    f.app.commands().declare({.key = "test.scoped",
                              .title = "Scoped",
                              .category = "Test",
                              .context = "editor",
                              .chord = "Ctrl+E",
                              .handler = [&] { ++scoped; }});
    f.app.commands().declare({.key = "test.background",
                              .title = "Background",
                              .category = "Test",
                              .chord = "Ctrl+B",
                              .handler = [&] { ++background; }});

    auto* modal = f.app.root().add(std::make_unique<View>(Rect{0, 0, 20, 5}));
    auto* field = modal->add(std::make_unique<View>(Rect{1, 1, 4, 1}));
    field->set_focus_policy(FocusPolicy::TabStop);
    field->set_command_context("editor");
    f.app.set_focus(field);
    f.app.push_modal(*modal);

    CK_CHECK(f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Ctrl, "e"}}));
    CK_CHECK(scoped == 1);
    CK_CHECK(!f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Ctrl, "b"}}));
    CK_CHECK(background == 0);
}

CK_TEST(command_withdrawal_removes_metadata_handler_enablement_and_key_binding) {
    ui::CommandRegistry registry;
    bool ran = false;
    const ui::CommandId quit = registry.declare({.key = "test.quit",
                                                 .title = "Quit",
                                                 .category = "Test",
                                                 .chord = "Alt+X",
                                                 .handler = [&] { ran = true; }});
    registry.set_enabled_predicate(quit, [] { return true; });

    CK_CHECK(registry.find(quit) != nullptr);
    CK_CHECK(registry.command_for_key(*KeyChord::parse("Alt+X")) == quit);
    CK_CHECK(registry.execute(quit));
    CK_CHECK(ran);

    registry.withdraw(quit);
    ran = false;
    CK_CHECK(registry.find(quit) == nullptr);
    CK_CHECK(!registry.command_for_key(*KeyChord::parse("Alt+X")).has_value());
    CK_CHECK(!registry.execute(quit));
    CK_CHECK(!ran);
}

CK_TEST(menu_and_status_present_one_command_with_surface_specific_labels_and_same_handler) {
    AppFixture f;
    int quit_count = 0;
    const ui::CommandId quit = f.app.commands().declare({.key = "test.quit",
                                                         .title = "&Quit",
                                                         .category = "Test",
                                                         .chord = "Alt+X",
                                                         .handler = [&] { ++quit_count; }});

    widgets::DropdownMenu menu(
        {widgets::MenuItem::command(widgets::CommandPresentation{quit, "E&xit"})});
    menu.set_context(ui::Context{&f.app.theme(), &f.app.roles(), &f.app});
    menu.set_bounds(Rect{0, 0, menu.horizontal_size_hint().preferred, 1});
    Surface menu_surface(Size{20, 1});
    Painter menu_painter(menu_surface, Rect{0, 0, 20, 1});
    menu.draw(menu_painter);
    CK_CHECK(row_text(menu_surface).find("Exit") != std::string::npos);
    CK_CHECK(menu.on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}}));

    widgets::StatusLine status;
    status.set_context(ui::Context{&f.app.theme(), &f.app.roles(), &f.app});
    status.set_bounds(Rect{0, 0, 20, 1});
    status.set_items({widgets::StatusLineItem{widgets::CommandPresentation{quit, "&Quit"}}});
    Surface status_surface(Size{20, 1});
    Painter status_painter(status_surface, Rect{0, 0, 20, 1});
    status.draw(status_painter);
    CK_CHECK(row_text(status_surface).find("Quit") != std::string::npos);
    CK_CHECK(status.on_mouse(
        MouseEvent{MouseAction::Down, MouseButton::Left, Point{1, 0}, std::nullopt, Modifier::None}));
    CK_CHECK(status.on_mouse(
        MouseEvent{MouseAction::Up, MouseButton::Left, Point{1, 0}, std::nullopt, Modifier::None}));

    CK_CHECK(quit_count == 2);
}

CK_TEST(menu_bar_replaces_live_structure_and_closes_an_open_dropdown) {
    AppFixture f;
    auto* desktop = f.app.root().add(std::make_unique<widgets::Desktop>(f.app.root().bounds()));
    auto* bar = desktop->dock_top(std::make_unique<widgets::MenuBar>(
        std::vector<widgets::MenuBarItem>{{"&File",
                                           {widgets::MenuItem::command(widgets::CommandPresentation{standard(f.app).quit})}}}));
    bar->activate();
    CK_CHECK(bar->on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(desktop->popups().size() == 1);

    bar->set_menus({{"&Tools",
                     {widgets::MenuItem::command(widgets::CommandPresentation{standard(f.app).menu})}}});
    CK_CHECK(desktop->popups().empty());
    CK_CHECK(bar->menus().size() == 1);
    CK_CHECK(bar->menus().front().label == "&Tools");
}

CK_TEST(status_line_renders_disabled_command_entries_with_disabled_role) {
    AppFixture f;
    const ui::CommandId quit = f.app.commands().declare({.key = "test.quit",
                                                         .title = "&Quit",
                                                         .category = "Test",
                                                         .chord = "Alt+X",
                                                         .handler = [] {}});
    f.app.commands().set_enabled_predicate(quit, [] { return false; });

    widgets::StatusLine status;
    status.set_context(ui::Context{&f.app.theme(), &f.app.roles(), &f.app});
    status.set_bounds(Rect{0, 0, 20, 1});
    status.set_items({widgets::StatusLineItem{widgets::CommandPresentation{quit, "&Quit"}}});

    Surface surface(Size{20, 1});
    Painter painter(surface, Rect{0, 0, 20, 1});
    status.draw(painter);
    // Column 0 is the leading padding; the item's own text starts at 1.
    CK_CHECK(surface.at(Point{1, 0}).style() ==
             f.app.theme().resolve(f.roles.status_line_disabled));
}

CK_TEST(application_shell_constructs_chrome_without_owning_loop_or_process_state) {
    AppFixture f;
    f.app.commands().set_handler(standard(f.app).quit, [] {});
    widgets::ApplicationShell shell(
        f.app, {.theme = ui::make_classic_theme(f.app.roles(), f.roles),
                .menus = {{"&File",
                           {widgets::MenuItem::command(widgets::CommandPresentation{standard(f.app).quit, "E&xit"})}}},
                .status_items = {widgets::StatusLineItem{
                    widgets::CommandPresentation{standard(f.app).quit, "&Quit"}}}});

    CK_CHECK(shell.menu_bar() != nullptr);
    CK_CHECK(shell.status_line() != nullptr);
    CK_CHECK(f.app.root().children().size() == 1);
    CK_CHECK(f.app.root().children().front().get() == &shell.desktop());
    CK_CHECK(!f.app.quit_requested());
}

CK_TEST(application_shell_can_detach_its_desktop_before_the_application_ends) {
    AppFixture f;
    widgets::ApplicationShell shell(
        f.app, {.theme = ui::make_classic_theme(f.app.roles(), f.roles)});
    CK_CHECK(f.app.root().children().size() == 1);
    CK_CHECK(f.app.root().children().front().get() == &shell.desktop());

    shell.detach_desktop();
    CK_CHECK(f.app.root().children().empty());
    shell.detach_desktop();
    f.app.step(0);
    CK_CHECK((f.app.current_frame().size() == Size{80, 24}));
}

CK_TEST(about_help_ignores_a_desktop_that_was_explicitly_detached) {
    AppFixture f;
    widgets::ApplicationShell shell(
        f.app, {.theme = ui::make_classic_theme(f.app.roles(), f.roles)});
    widgets::install_about_help(f.app, shell.desktop(), f.roles, "Teardown test", "Detached Desktop");

    shell.detach_desktop();
    CK_CHECK(f.app.execute_command(standard(f.app).help));
    f.app.step(0);
    CK_CHECK(f.app.root().children().empty());
}

CK_TEST(application_shell_docks_an_empty_status_line_only_when_the_caller_asks_for_one) {
    // An application whose status items come from live state has nothing to
    // pass at construction, but still needs the bar. Without the explicit
    // request the old rule stands: no items, no bar.
    AppFixture f;
    widgets::ApplicationShell implicit(f.app, {.theme = ui::make_classic_theme(f.app.roles(), f.roles)});
    CK_CHECK(implicit.status_line() == nullptr);

    AppFixture requested;
    widgets::ApplicationShell shell(
        requested.app, {.theme = ui::make_classic_theme(requested.app.roles(), requested.roles),
                        .always_dock_status_line = true});
    CK_CHECK(shell.status_line() != nullptr);
    CK_CHECK(shell.status_line()->items().empty());
    CK_CHECK(shell.desktop().bottom_dock() == shell.status_line());
    // The bar is real chrome from the start: it takes its row out of the
    // area windows may occupy, so an application that fills it later never
    // sees the desktop reflow underneath its windows.
    CK_CHECK(shell.desktop().content_area().height < shell.desktop().bounds().height);

    shell.status_line()->set_items({widgets::StatusLineItem{"F1 Help"}});
    CK_CHECK(shell.status_line()->items().size() == 1U);
}

CK_TEST(theme_override_rethemes_an_existing_subtree_and_clear_restores_the_parent_theme) {
    AppFixture f;
    auto* window_scope = f.app.root().add(std::make_unique<View>());
    auto* child = window_scope->add(std::make_unique<View>());
    const auto classic = f.app.theme().resolve(f.roles.desktop_background);
    const auto dark_theme = ui::make_dark_theme(f.app.roles(), f.roles);
    const auto dark = dark_theme.resolve(f.roles.desktop_background);
    CK_CHECK(classic != dark);

    window_scope->set_theme_override(dark_theme);
    CK_CHECK(child->context().theme->resolve(f.roles.desktop_background) == dark);

    window_scope->clear_theme_override();
    CK_CHECK(child->context().theme->resolve(f.roles.desktop_background) == classic);
}
