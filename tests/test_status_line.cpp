// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/desktop.hpp"

#include "cvision/core/text.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/application_shell.hpp"

using ckv::ManualClock;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::Application;
using ckv::ui::CommandId;
using ckv::ui::FocusPolicy;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::ui::View;
using ckv::widgets::StatusLine;
using ckv::widgets::StatusLineItem;
namespace ui = ckv::ui;

namespace {

// The framework's own commands, by name. A test names the concept and
// asks the registry that assigned the ids, exactly as application code
// does — no test knows or states a command's number.
const ckv::ui::StandardCommands& standard(const ckv::ui::Application& app) {
    return app.commands().standard();
}
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

std::string row_text(const Surface& s, int y) {
    std::string out;
    for (int x = 0; x < s.size().width; ++x) out += s.at(ckv::Point{x, y}).grapheme();
    return out;
}
}  // namespace

// --- Hint text ------------------------------------------------------------

CK_TEST(no_hint_provider_reports_an_empty_current_hint) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    CK_CHECK(status.current_hint().empty());
}

CK_TEST(no_focused_view_reports_an_empty_hint_even_with_a_provider_installed) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_hint_provider([](const std::string&) { return "should not appear"; });
    CK_CHECK(status.current_hint().empty());
}

CK_TEST(focused_view_with_no_help_context_key_reports_an_empty_hint) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_hint_provider([](const std::string&) { return "should not appear"; });
    auto* view = app.root().add_child(std::make_unique<View>());
    view->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(view);
    CK_CHECK(status.current_hint().empty());
}

CK_TEST(hint_resolves_through_the_focused_views_help_context_key) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_hint_provider([](const std::string& key) { return "Hint for " + key; });
    auto* view = app.root().add_child(std::make_unique<View>());
    view->set_focus_policy(FocusPolicy::TabStop);
    view->set_help_context_key("topic.a");
    app.set_focus(view);
    CK_CHECK(status.current_hint() == "Hint for topic.a");
}

CK_TEST(hint_updates_when_focus_moves_to_a_different_view) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_hint_provider([](const std::string& key) { return key; });
    auto* a = app.root().add_child(std::make_unique<View>());
    a->set_focus_policy(FocusPolicy::TabStop);
    a->set_help_context_key("a");
    auto* b = app.root().add_child(std::make_unique<View>());
    b->set_focus_policy(FocusPolicy::TabStop);
    b->set_help_context_key("b");

    app.set_focus(a);
    CK_CHECK(status.current_hint() == "a");
    app.set_focus(b);
    CK_CHECK(status.current_hint() == "b");
}

CK_TEST(with_nothing_focused_the_hint_resolves_from_the_roots_help_context_key) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_hint_provider([](const std::string& key) { return "Hint for " + key; });
    // An empty desktop: the root's key stands for the application as a whole.
    app.root().set_help_context_key("desktop");
    CK_CHECK(app.focused() == nullptr);
    CK_CHECK(status.current_hint() == "Hint for desktop");
    // A focused view with a key of its own outranks the root's.
    auto* view = app.root().add_child(std::make_unique<View>());
    view->set_focus_policy(FocusPolicy::TabStop);
    view->set_help_context_key("topic.a");
    app.set_focus(view);
    CK_CHECK(status.current_hint() == "Hint for topic.a");
    // A focused view without one inherits the root's, as any ancestor's.
    auto* plain = app.root().add_child(std::make_unique<View>());
    plain->set_focus_policy(FocusPolicy::TabStop);
    app.set_focus(plain);
    CK_CHECK(status.current_hint() == "Hint for desktop");
}

// --- Items -----------------------------------------------------------

CK_TEST(the_items_follow_the_focused_views_command_context) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 60, 1});
    const ui::CommandId open = app.commands().declare({.key = "test.open", .title = "Open"});
    const ui::CommandId save = app.commands().declare({.key = "test.save", .title = "Save"});
    const ui::CommandId refresh = app.commands().declare({.key = "test.refresh", .title = "Refresh"});
    bool refreshed = false;
    app.set_command_handler(refresh, [&] { refreshed = true; });
    status.set_items({StatusLineItem{"Open", open, 0}});
    status.set_context_items("editor", {StatusLineItem{"Save", save, 0}});
    status.set_context_items("preview", {StatusLineItem{"Refresh", refresh, 0}});

    auto* editor_pane = app.root().add_child(std::make_unique<View>());
    editor_pane->set_command_context("editor");
    auto* editor = editor_pane->add_child(std::make_unique<View>());
    editor->set_focus_policy(FocusPolicy::TabStop);
    auto* preview = app.root().add_child(std::make_unique<View>());
    preview->set_command_context("preview");
    preview->set_focus_policy(FocusPolicy::TabStop);
    auto* plain = app.root().add_child(std::make_unique<View>());
    plain->set_focus_policy(FocusPolicy::TabStop);
    auto* other = app.root().add_child(std::make_unique<View>());
    other->set_command_context("tree");
    other->set_focus_policy(FocusPolicy::TabStop);

    CK_CHECK(status.shown_items().front().command == open);  // no focus: the ordinary items
    app.set_focus(editor);                                     // context found on an ancestor
    CK_CHECK(status.shown_items().front().command == save);
    app.set_focus(preview);
    CK_CHECK(status.shown_items().front().command == refresh);
    // A click reaches the item that is on the line now.
    CK_CHECK(status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 0},
                                              std::nullopt, Modifier::None}));
    CK_CHECK(status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{2, 0},
                                              std::nullopt, Modifier::None}));
    CK_CHECK(refreshed);
    app.set_focus(plain);                                      // no context
    CK_CHECK(status.shown_items().front().command == open);
    app.set_focus(other);                                      // a context without a set of its own
    CK_CHECK(status.shown_items().front().command == open);
    status.set_context_items("preview", {});                  // removed
    app.set_focus(preview);
    CK_CHECK(status.shown_items().front().command == open);
    app.set_focus(editor);
    status.clear_context_items();
    CK_CHECK(status.shown_items().front().command == open);
}


CK_TEST(clicking_an_items_label_fires_its_command) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    const ui::CommandId help_command = app.commands().declare(
        {.key = "test.help", .title = "Help"});
    bool ran = false;
    app.set_command_handler(help_command, [&] { ran = true; });
    status.set_items({StatusLineItem{"Help", help_command, 0}});

    CK_CHECK(status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 0},
                                              std::nullopt, Modifier::None}));
    CK_CHECK(!ran);  // the press shows itself first
    CK_CHECK(status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{2, 0},
                                              std::nullopt, Modifier::None}));
    CK_CHECK(ran);
}

CK_TEST(clicking_outside_any_item_is_unhandled) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    status.set_items({StatusLineItem{"Help", ui::kInvalidCommand, 0}});
    CK_CHECK(!status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{30, 0},
                                               std::nullopt, Modifier::None}));
}

CK_TEST(a_disabled_commands_item_still_fires_execute_command_which_itself_declines) {
    // StatusLine doesn't special-case enablement — execute_command
    // already refuses a disabled command, so clicking a disabled
    // item's label is a harmless no-op rather than StatusLine needing
    // its own enablement check.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    const ui::CommandId help_command = app.commands().declare(
        {.key = "test.help", .title = "Help"});
    app.commands().set_enabled_predicate(help_command, [] { return false; });
    bool ran = false;
    app.set_command_handler(help_command, [&] { ran = true; });
    status.set_items({StatusLineItem{"Help", help_command, 0}});

    CK_CHECK(status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 0},
                                              std::nullopt, Modifier::None}));
    CK_CHECK(!ran);
}

CK_TEST(second_item_is_positioned_after_the_first_across_their_shared_padding) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    const CommandId first_command = app.commands().declare({.key = "test.first", .title = "First"});
    const CommandId second_command =
        app.commands().declare({.key = "test.second", .title = "Second"});
    bool second_ran = false;
    app.set_command_handler(second_command, [&] { second_ran = true; });
    // An item referencing a command renders the command's registered
    // title, not its own label (M9/WP-11) — these hand-set labels
    // ("AB", "Second") are therefore never shown; the actual rendered
    // text is "First" / "Second" from the registrations above.
    status.set_items({StatusLineItem{"AB", first_command, 0}, StatusLineItem{"Second", second_command, 0}});

    // One cell of leading padding, so "First" occupies columns 1-5, the
    // two-cell gap 6-7, and "Second" starts at 8.
    CK_CHECK(status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                              ckv::Point{10, 0}, std::nullopt, Modifier::None}));
    CK_CHECK(status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left,
                                              ckv::Point{10, 0}, std::nullopt, Modifier::None}));
    CK_CHECK(second_ran);
}

CK_TEST(draw_renders_the_hint_and_items_without_crashing) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    status.set_hint_provider([](const std::string& key) { return key; });
    auto* view = app.root().add_child(std::make_unique<View>());
    view->set_focus_policy(FocusPolicy::TabStop);
    view->set_help_context_key("Ready");
    app.set_focus(view);
    status.set_items({StatusLineItem{"Help", ui::kInvalidCommand, 0}});

    Surface s(ckv::Size{40, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 40, 1});
    status.draw(painter);
    // Items first, then the hint: the actionable legend keeps its cells and
    // the explanation takes what is left.
    const std::string rendered = row_text(s, 0);
    CK_CHECK(rendered.substr(0, 5) == " Help");  // one cell of leading padding
    CK_CHECK(rendered.find("Ready") != std::string::npos);
    CK_CHECK(rendered.find("Help") < rendered.find("Ready"));
}

CK_TEST(command_chords_automatically_use_the_shared_hotkey_accent) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    status.set_items({StatusLineItem{ckv::widgets::CommandPresentation{standard(app).help}}});

    Surface s(ckv::Size{40, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 40, 1});
    status.draw(painter);

    CK_CHECK(row_text(s, 0).substr(0, 8) == " F1 Help");
    CK_CHECK(s.at(Point{1, 0}).style().fg == f.theme.resolve(f.roles.hotkey).fg);
    CK_CHECK(s.at(Point{3, 0}).style().fg == f.theme.resolve(f.roles.status_line_normal).fg);
}

CK_TEST(draw_stops_placing_items_that_would_start_past_the_available_width_without_crashing) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 5, 1});  // very narrow
    status.set_items({StatusLineItem{"This label is far too long to fit", ui::kInvalidCommand, 0}});

    Surface s(ckv::Size{5, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 5, 1});
    status.draw(painter);  // must not crash even though the item overflows the bounds
    CK_CHECK(true);
}

CK_TEST(equal_priority_status_items_keep_a_visible_prefix_of_the_final_item) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 12, 1});
    status.set_items({StatusLineItem{"First"}, StatusLineItem{"Second"}});

    Surface s(ckv::Size{12, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 12, 1});
    status.draw(painter);

    // Items are set apart by the gap alone — no rule between them.
    CK_CHECK(row_text(s, 0) == " First  Seco");
}

CK_TEST(narrow_status_line_keeps_higher_priority_items_visible_and_clickable) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 8, 1});
    const CommandId low_command = app.commands().declare({.key = "test.low", .title = "Verbose"});
    const CommandId high_command = app.commands().declare({.key = "test.high", .title = "Quit"});
    bool high_ran = false;
    app.set_command_handler(high_command, [&] { high_ran = true; });
    status.set_items({StatusLineItem{"", low_command, 0}, StatusLineItem{"", high_command, 10}});

    Surface s(ckv::Size{8, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 8, 1});
    status.draw(painter);
    const std::string row = row_text(s, 0);
    CK_CHECK(row.find("Quit") != std::string::npos);
    CK_CHECK(row.find("Verbose") == std::string::npos);

    CK_CHECK(status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                              ckv::Point{1, 0}, std::nullopt, Modifier::None}));
    CK_CHECK(status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left,
                                              ckv::Point{1, 0}, std::nullopt, Modifier::None}));
    CK_CHECK(high_ran);
}

// --- Item padding and press feedback ----------------------------------

CK_TEST(the_first_status_item_is_inset_from_the_screen_edge) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    status.set_items({StatusLineItem{"Help", ui::kInvalidCommand, 0}});

    Surface s(ckv::Size{40, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 40, 1});
    status.draw(painter);
    // Text begins one cell in: the padding is the item's, not the screen's.
    CK_CHECK(s.at(Point{0, 0}).grapheme() == " ");
    CK_CHECK(s.at(Point{1, 0}).grapheme() == "H");
}

CK_TEST(only_the_hint_is_fenced_off_never_one_item_from_the_next) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    status.set_items({StatusLineItem{"Help"}, StatusLineItem{"More"}});
    status.set_transient_hint("Explains it");

    Surface s(ckv::Size{40, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 40, 1});
    status.draw(painter);

    // "Help" occupies 1-4 and "More" 7-10: the cells between them are the
    // two items' own padding and stay blank. A rule there would divide
    // things that are alike.
    CK_CHECK(s.at(Point{5, 0}).grapheme() == " ");
    CK_CHECK(s.at(Point{6, 0}).grapheme() == " ");
    // The one real boundary is drawn, one cell clear of the last item's
    // padding so a pressed item keeps that cell.
    CK_CHECK(s.at(Point{11, 0}).grapheme() == " ");
    CK_CHECK(s.at(Point{12, 0}).grapheme() == "\u2502");
    CK_CHECK(s.at(Point{14, 0}).grapheme() == "E");
}

CK_TEST(a_pressed_item_wears_the_themes_selected_colours_chord_accent_included) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId first_command = app.commands().declare(
        {.key = "test.first", .title = "Help browser..."});
    app.commands().bind_key(*ckv::KeyChord::parse("F1"), first_command);
    app.set_command_handler(first_command, [] {});
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    status.on_attached();
    status.set_items({StatusLineItem{"", first_command, 0}});

    status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{3, 0},
                                     std::nullopt, Modifier::None});
    Surface s(ckv::Size{40, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 40, 1});
    status.draw(painter);

    const ckv::Style selected = f.theme.resolve(f.roles.status_line_selected);
    const ckv::Style selected_hotkey = f.theme.resolve(f.roles.status_line_selected_hotkey);
    // The padding carries the press too, which is what makes it a button.
    CK_CHECK(s.at(Point{0, 0}).style().bg == selected.bg);
    // "F1" is the chord: it keeps its own accent against the selected
    // background rather than dissolving into the label.
    CK_CHECK(s.at(Point{1, 0}).style().fg == selected_hotkey.fg);
    CK_CHECK(s.at(Point{1, 0}).style().bg == selected.bg);
    // ...and the label itself takes the plain selected colour.
    CK_CHECK(s.at(Point{4, 0}).style().fg == selected.fg);
    CK_CHECK(s.at(Point{4, 0}).style().bg == selected.bg);
}

CK_TEST(the_classic_status_pressed_palette_matches_the_project_specification) {
    // The ckVision classic-look specification defines pressed text as black
    // on green, with a red chord accent. These roles remain independent.
    Fixture f;
    const ckv::Style selected = f.theme.resolve(f.roles.status_line_selected);
    const ckv::Style selected_hotkey = f.theme.resolve(f.roles.status_line_selected_hotkey);
    CK_CHECK(selected.bg == ckv::Color::rgb(0, 170, 0));
    CK_CHECK(selected.fg == ckv::Color::rgb(0, 0, 0));
    CK_CHECK(selected_hotkey.bg == ckv::Color::rgb(0, 170, 0));
    CK_CHECK(selected_hotkey.fg == ckv::Color::rgb(170, 0, 0));
}

CK_TEST(a_status_item_acts_on_release_and_can_be_taken_back) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    const CommandId run_command = app.commands().declare({.key = "test.run", .title = "Run"});
    int runs = 0;
    app.set_command_handler(run_command, [&] { ++runs; });
    status.set_items({StatusLineItem{"Run", run_command, 0}});

    const auto at = [](int x, ckv::MouseAction action) {
        return ckv::MouseEvent{action, ckv::MouseButton::Left, Point{x, 0}, std::nullopt,
                               Modifier::None};
    };
    // Pressing shows the press; it does not act yet.
    CK_CHECK(status.on_mouse(at(2, ckv::MouseAction::Down)));
    CK_CHECK(runs == 0);
    CK_CHECK(status.on_mouse(at(2, ckv::MouseAction::Up)));
    CK_CHECK(runs == 1);

    // Pressed here, released elsewhere: the press is taken back.
    status.on_mouse(at(2, ckv::MouseAction::Down));
    status.on_mouse(at(38, ckv::MouseAction::Move));
    status.on_mouse(at(38, ckv::MouseAction::Up));
    CK_CHECK(runs == 1);
}

CK_TEST(a_surface_stated_chord_replaces_the_registrys_and_keeps_its_accent) {
    // An application whose command is reached by something the keymap cannot
    // hold — a multi-key sequence — states the spelling it means. Without
    // this the bar renders the registry chord AND the application's own
    // wording, telling the reader two different things at once.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId open_command = app.commands().declare(
        {.key = "test.open", .title = "Window List"});
    app.commands().bind_key(*ckv::KeyChord::parse("F6"), open_command);
    app.set_command_handler(open_command, [] {});

    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    status.on_attached();
    status.set_items({StatusLineItem{ckv::widgets::CommandPresentation{open_command, "windows", "^B w"}}});

    Surface s(ckv::Size{40, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 40, 1});
    status.draw(painter);
    const std::string row = row_text(s, 0);
    CK_CHECK(row.find("^B w windows") != std::string::npos);
    // The registry's own chord is not advertised here...
    CK_CHECK(row.find("F6") == std::string::npos);
    // ...and it still works everywhere else: this is presentation only.
    CK_CHECK(app.commands().command_for_key(*ckv::KeyChord::parse("F6")) == open_command);

    // The stated chord is what receives the hotkey accent, not the label.
    const ckv::Style hotkey = f.theme.resolve(f.roles.hotkey);
    CK_CHECK(s.at(Point{1, 0}).style().fg == hotkey.fg);   // '^' of "^B w"
    CK_CHECK(s.at(Point{6, 0}).style().fg != hotkey.fg);   // 'w' of "windows"
}

CK_TEST(an_item_without_a_stated_chord_still_asks_the_registry) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    const ui::CommandId open_command = app.commands().declare(
        {.key = "test.open", .title = "Window List"});
    app.commands().bind_key(*ckv::KeyChord::parse("F6"), open_command);
    app.set_command_handler(open_command, [] {});

    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    status.on_attached();
    status.set_items({StatusLineItem{ckv::widgets::CommandPresentation{open_command, "windows"}}});

    Surface s(ckv::Size{40, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 40, 1});
    status.draw(painter);
    CK_CHECK(row_text(s, 0).find("F6 windows") != std::string::npos);
}

CK_TEST(a_scripted_status_line_runs_its_command_by_click_and_by_chord_in_an_application_shell) {
    // Application-level script: the status line is docked by an
    // ApplicationShell, and the chord and the click reach it through
    // dispatch, as a reader's would; step() shows what it offers.
    ckv::term::HeadlessTerminal term(ckv::Size{40, 10});
    ManualClock clock;
    Application app(term, clock);
    RoleRegistry& registry = app.roles();
    const StandardRoles roles = intern_standard_roles(registry);
    int runs = 0;
    const CommandId run_command = app.commands().declare(
        {.key = "test.run", .title = "Run", .chord = "F2", .handler = [&] { ++runs; }});
    ckv::widgets::ApplicationShell shell(
        app, ckv::widgets::ApplicationShellOptions{.theme = make_classic_theme(registry, roles),
                                                   .status_items = {StatusLineItem{"Run", run_command, 0}}});
    app.step(0);
    const auto bottom = [&] {
        std::string out;
        for (int x = 0; x < 40; ++x) out += app.composed_surface().at(Point{x, 9}).grapheme();
        return out;
    };
    const std::size_t label = bottom().find("Run");
    CK_CHECK(label != std::string::npos);
    CK_CHECK(bottom().find("F2") != std::string::npos);

    CK_CHECK(app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F2, Modifier::None, ""}}));
    CK_CHECK(runs == 1);
    const int x = static_cast<int>(ckv::text::text_width(bottom().substr(0, label)));
    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, Point{x, 9}, std::nullopt,
                                 Modifier::None});
    CK_CHECK(runs == 1);  // a press alone decides nothing
    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, Point{x, 9}, std::nullopt,
                                 Modifier::None});
    app.step(0);
    CK_CHECK(runs == 2);
    CK_CHECK(shell.status_line() != nullptr);
}

// Regression: a runtime rebind changed what an attached status line would
// draw, yet nothing repainted it, so the line went on advertising the old
// chord until something unrelated happened to invalidate it. Application now
// repaints whenever CommandRegistry::revision() moves.
CK_TEST(a_runtime_rebind_repaints_an_attached_status_line_on_the_next_step) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    const StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = static_cast<ckv::widgets::Desktop*>(
        app.root().add_child(std::make_unique<ckv::widgets::Desktop>(app.root().bounds())));
    auto owned = std::make_unique<StatusLine>();
    owned->set_items({StatusLineItem{ckv::widgets::CommandPresentation{app.commands().standard().quit}}});
    desktop->dock_bottom(std::move(owned));
    const auto bottom_row = [&term] {
        std::string text;
        const ckv::FrameView frame = term.display().frame();
        for (int x = 0; x < frame.size().width; ++x) text += frame.at(ckv::Point{x, 23}).grapheme();
        return text;
    };
    app.step(0);
    CK_CHECK(bottom_row().find("Alt+X Quit") != std::string::npos);

    app.commands().unbind_key(ckv::KeyChord{ckv::Key::Char, Modifier::Alt, "x"});
    app.commands().bind_key(ckv::KeyChord{ckv::Key::Char, Modifier::Ctrl, "q"}, app.commands().standard().quit);
    app.step(0);
    CK_CHECK(bottom_row().find("Ctrl+Q Quit") != std::string::npos);
    CK_CHECK(bottom_row().find("Alt+X") == std::string::npos);
}


CK_TEST(grouped_status_separates_logical_groups_without_bracketing_or_extra_rules) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 18, 1});
    StatusLineItem a{"A", ui::kInvalidCommand, 10};
    StatusLineItem b{"B", ui::kInvalidCommand, 0};
    StatusLineItem c{"C", ui::kInvalidCommand, 10};
    a.group_break_before = true; // No leading rule.
    b.group_break_before = true;
    status.set_items({a, b, c});
    Surface surface(ckv::Size{18, 1});
    Painter painter(surface, Rect{0, 0, 18, 1});
    status.draw(painter);
    CK_CHECK(row_text(surface, 0).find(" A  B  C") == 0);
    status.set_presentation(ckv::widgets::StatusLinePresentation::Grouped);
    status.draw(painter);
    CK_CHECK(row_text(surface, 0).find(" A │ B  C") == 0);
    CK_CHECK(!status.pointer_shape_at(Point{3, 0}));
    CK_CHECK(!status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, Point{3, 0}, std::nullopt}));
    status.set_bounds(Rect{0, 0, 6, 1}); // Drop B, retaining C's logical group.
    Surface narrow(ckv::Size{6, 1});
    Painter clipped(narrow, Rect{0, 0, 6, 1});
    status.draw(clipped);
    CK_CHECK(row_text(narrow, 0) == " A │ C");
    CK_CHECK(!status.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, Point{6, 0}, std::nullopt}));
}

CK_TEST(status_chord_accents_and_command_hits_survive_grouped_geometry) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 40, 1});
    const auto run = app.commands().declare({.key = "group.run", .title = "Run"});
    int runs = 0;
    app.set_command_handler(run, [&] { ++runs; });
    StatusLineItem command{ckv::widgets::CommandPresentation{run, "Run", "F2"}};
    command.group_break_before = true;
    status.set_items({StatusLineItem{"Info"}, command});
    status.set_presentation(ckv::widgets::StatusLinePresentation::Grouped);
    status.set_transient_hint("Ready");
    Surface surface(ckv::Size{40, 1});
    Painter painter(surface, Rect{0, 0, 40, 1});
    status.draw(painter);
    CK_CHECK(row_text(surface, 0).find(" Info │ F2 Run │ Ready") == 0);
    CK_CHECK(surface.at(Point{8, 0}).style().fg == f.theme.resolve(f.roles.hotkey).fg);
    CK_CHECK(status.pointer_shape_at(Point{8, 0}) == ckv::PointerShape::Pointer);
    CK_CHECK(!status.pointer_shape_at(Point{6, 0}));
    CK_CHECK(!status.pointer_shape_at(Point{18, 0}));
    const auto at = [](ckv::MouseAction action) { return ckv::MouseEvent{action, ckv::MouseButton::Left, Point{8, 0}, std::nullopt}; };
    status.on_mouse(at(ckv::MouseAction::Down));
    status.on_mouse(at(ckv::MouseAction::Up));
    CK_CHECK(runs == 1);
    status.set_enabled(false);
    CK_CHECK(status.pointer_shape_at(Point{8, 0}) == ckv::PointerShape::NotAllowed);
    CK_CHECK(!status.on_mouse(at(ckv::MouseAction::Down)));
}

CK_TEST(status_reconfiguration_and_focus_context_changes_cancel_held_commands) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app(term, clock);
    const auto roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* status = app.root().make<StatusLine>();
    status->set_bounds(Rect{0, 0, 40, 1});
    const auto run = app.commands().declare({.key = "cancel.run", .title = "Run"});
    int runs = 0;
    app.set_command_handler(run, [&] { ++runs; });
    status->set_items({StatusLineItem{"", run}});
    const auto at = [](ckv::MouseAction action) { return ckv::MouseEvent{action, ckv::MouseButton::Left, Point{2, 0}, std::nullopt}; };
    status->on_mouse(at(ckv::MouseAction::Down));
    status->set_presentation(ckv::widgets::StatusLinePresentation::Grouped);
    status->on_mouse(at(ckv::MouseAction::Up));
    CK_CHECK(runs == 0);
    status->on_mouse(at(ckv::MouseAction::Down));
    status->set_bounds(Rect{0, 0, 30, 1});
    status->on_mouse(at(ckv::MouseAction::Up));
    CK_CHECK(runs == 0);
    status->set_context_items("other", {StatusLineItem{"", run}});
    status->on_mouse(at(ckv::MouseAction::Down));
    auto* document = app.root().make<View>();
    document->set_focus_policy(FocusPolicy::TabStop);
    document->set_command_context("other");
    app.set_focus(document);
    CK_CHECK(status->on_mouse(at(ckv::MouseAction::Up)));
    CK_CHECK(runs == 0);
    status->on_mouse(at(ckv::MouseAction::Down));
    app.commands().bind_key(ckv::KeyChord{ckv::Key::F12, Modifier::None, ""}, run);
    CK_CHECK(status->on_mouse(at(ckv::MouseAction::Up)));
    CK_CHECK(runs == 0);
    status->on_mouse(at(ckv::MouseAction::Down));
    app.set_focus(nullptr); // Same commands, different origin: cancellation.
    CK_CHECK(status->on_mouse(at(ckv::MouseAction::Up)));
    CK_CHECK(runs == 0);
}


CK_TEST(status_hint_preparation_refreshes_keys_rebinds_and_announced_mapping_changes) {
    ckv::term::HeadlessTerminal terminal{ckv::Size{80, 24}};
    ManualClock clock;
    Application app(terminal, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    status.set_bounds(Rect{0, 0, 60, 1});
    app.root().set_help_context_key("topic");
    int calls = 0;
    std::string answer = "First mapping";
    status.set_hint_provider([&](const std::string&) { ++calls; return answer; });
    Surface surface(ckv::Size{60, 1});
    Painter painter(surface, Rect{0, 0, 60, 1});
    status.draw(painter);
    status.draw(painter);
    CK_CHECK(calls == 1);
    CK_CHECK(status.current_hint() == answer);
    answer = "Changed mapping";
    status.refresh_hint();
    status.draw(painter);
    CK_CHECK(calls == 2);
    CK_CHECK(row_text(surface, 0).find(answer) == 0);
    app.root().set_help_context_key("other");
    status.draw(painter);
    CK_CHECK(calls == 3);
    app.commands().bind_key(ckv::KeyChord{ckv::Key::F12, Modifier::None, ""}, standard(app).help);
    status.draw(painter);
    CK_CHECK(calls == 4);
    status.set_transient_hint("Temporary");
    status.draw(painter);
    CK_CHECK(calls == 4);
    CK_CHECK(status.current_hint() == "Temporary");
    status.set_transient_hint({});
    CK_CHECK(status.current_hint() == answer);
}


CK_TEST(status_tiny_bounds_clip_chord_accents_and_zero_height_paints_nothing) {
    ckv::term::HeadlessTerminal terminal{ckv::Size{80, 24}};
    ManualClock clock;
    Application app(terminal, clock);
    Fixture f;
    StatusLine status;
    status.set_context(ui::Context{&f.theme, &f.registry, &app});
    const auto run = app.commands().declare({.key = "tiny.run", .title = "Run"});
    app.set_command_handler(run, [] {});
    status.set_items({StatusLineItem{ckv::widgets::CommandPresentation{run, "Run", "Ctrl+Shift+F12"}}});
    for (const auto presentation : {ckv::widgets::StatusLinePresentation::Plain, ckv::widgets::StatusLinePresentation::Grouped}) {
        status.set_presentation(presentation);
        status.set_bounds(Rect{0, 0, 3, 1});
        Surface surface(ckv::Size{20, 2}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter painter(surface, Rect{0, 0, 20, 2});
        status.draw(painter);
        CK_CHECK(surface.at(Point{3, 0}).grapheme() == ".");
        CK_CHECK(surface.at(Point{1, 1}).grapheme() == ".");
        status.set_bounds(Rect{0, 0, 3, 0});
        Surface empty(ckv::Size{20, 2}, ckv::Cell::from_grapheme(".", ckv::Style{}));
        Painter empty_painter(empty, Rect{0, 0, 20, 2});
        status.draw(empty_painter);
        CK_CHECK(empty.at(Point{0, 0}).grapheme() == ".");
    }
}
