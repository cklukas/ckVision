// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/common_components.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/status_line.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::ui::Application;
using namespace ckv::widgets;

namespace {

// The framework's own commands, by name. A test names the concept and
// asks the registry that assigned the ids, exactly as application code
// does — no test knows or states a command's number.
const ckv::ui::StandardCommands& standard(const ckv::ui::Application& app) {
    return app.commands().standard();
}
ckv::KeyEvent key(Key k) { return ckv::KeyEvent{KeyChord{k, Modifier::None, ""}}; }

struct Standalone {
    ckv::ui::RoleRegistry registry;
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);
    ckv::ui::Context context() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

void attach_standalone(ckv::ui::View& view, Standalone& s) { view.set_context(s.context()); }
}  // namespace

CK_TEST(calendar_selection_is_deterministic_range_aware_and_keyboard_driven) {
    Standalone s;
    CalendarView calendar;
    attach_standalone(calendar, s);
    calendar.set_selected(DateValue{2026, 8, 9});
    calendar.set_today(DateValue{2026, 8, 9});
    calendar.set_range(DateValue{2026, 8, 1}, DateValue{2026, 8, 31});
    int changes = 0;
    calendar.on_select = [&](DateValue) { ++changes; };

    CK_CHECK(calendar.on_key(key(Key::Right)));
    CK_CHECK(calendar.selected() == (DateValue{2026, 8, 10}));
    CK_CHECK(changes == 1);

    calendar.set_disabled_predicate([](DateValue date) { return date.day == 11; });
    CK_CHECK(calendar.on_key(key(Key::Right)));
    CK_CHECK(calendar.selected() == (DateValue{2026, 8, 10}));
}

CK_TEST(date_and_time_pickers_change_only_from_caller_supplied_values) {
    Standalone s;
    DatePicker date;
    TimePicker time;
    attach_standalone(date, s);
    attach_standalone(time, s);
    date.set_value(DateValue{2026, 12, 24});
    time.set_value(TimeValue{23, 58, 59});

    CK_CHECK((date.value() == std::optional<DateValue>{DateValue{2026, 12, 24}}));
    CK_CHECK(time.on_key(key(Key::Up)));
    CK_CHECK(time.value() == (TimeValue{0, 58, 59}));
    CK_CHECK(time.on_key(key(Key::Right)));
    CK_CHECK(time.on_key(key(Key::Up)));
    CK_CHECK(time.value() == (TimeValue{0, 59, 59}));
}

CK_TEST(typed_time_interchange_is_canonical_and_strict) {
    CK_CHECK(format_iso_time(TimeValue{7, 8, 9}) == "07:08:09");
    CK_CHECK(format_iso_time(TimeValue{7, 8, 9}, false) == "07:08");
    CK_CHECK((parse_iso_time("23:59") == std::optional<TimeValue>{TimeValue{23, 59, 0}}));
    CK_CHECK((parse_iso_time("23:59:58") == std::optional<TimeValue>{TimeValue{23, 59, 58}}));
    CK_CHECK(!parse_iso_time("24:00"));
    CK_CHECK(!parse_iso_time("7:08"));
    CK_CHECK(!parse_iso_time("12:60:00"));
}

CK_TEST(date_picker_supports_strict_optional_segmented_editing_without_a_clock) {
    Standalone s;
    DatePicker date;
    attach_standalone(date, s);
    date.set_seed(DateValue{2024, 2, 29});
    CK_CHECK(!date.value());

    CK_CHECK(date.on_key(key(Key::Up)));
    CK_CHECK((date.value() == std::optional<DateValue>{DateValue{2025, 2, 28}}));
    CK_CHECK(date.on_key(key(Key::Right)));
    CK_CHECK(date.on_key(key(Key::Up)));
    CK_CHECK((date.value() == std::optional<DateValue>{DateValue{2025, 3, 28}}));
    CK_CHECK(date.on_key(key(Key::Right)));
    CK_CHECK(date.on_key(key(Key::Down)));
    CK_CHECK((date.value() == std::optional<DateValue>{DateValue{2025, 3, 27}}));
    CK_CHECK(date.on_key(key(Key::Delete)));
    CK_CHECK(!date.value());

    // Required again, an empty field takes the seed -- the caller's today,
    // which no value the reader chose has moved.
    date.set_empty_allowed(false);
    CK_CHECK((date.value() == std::optional<DateValue>{DateValue{2024, 2, 29}}));
    CK_CHECK(date.on_key(key(Key::Delete)));
    CK_CHECK(date.value().has_value());
}

CK_TEST(date_values_have_a_strict_locale_free_iso_boundary) {
    CK_CHECK(format_iso_date(DateValue{2024, 2, 29}) == "2024-02-29");
    CK_CHECK((parse_iso_date("2024-02-29") == std::optional<DateValue>{DateValue{2024, 2, 29}}));
    CK_CHECK(!parse_iso_date("2025-02-29"));
    CK_CHECK(!parse_iso_date("2026-8-09"));
    CK_CHECK(!parse_iso_date("1582-12-31"));
    CK_CHECK(!parse_iso_date("2026-08-09 trailing"));
    CK_CHECK((add_calendar_days(DateValue{2024, 2, 28}, 1) ==
              std::optional<DateValue>{DateValue{2024, 2, 29}}));
    CK_CHECK((add_calendar_days(DateValue{2024, 2, 29}, 1) ==
              std::optional<DateValue>{DateValue{2024, 3, 1}}));
    CK_CHECK((add_calendar_days(DateValue{2026, 12, 31}, 1) ==
              std::optional<DateValue>{DateValue{2027, 1, 1}}));
    CK_CHECK(!add_calendar_days(DateValue{kLastCalendarYear, 12, 31}, 1));
    CK_CHECK(!add_calendar_days(DateValue{2025, 2, 29}, 1));
}

CK_TEST(spinbox_and_slider_clamp_and_respond_to_keyboard_and_mouse) {
    Standalone s;
    SpinBox spin;
    Slider slider;
    attach_standalone(spin, s);
    attach_standalone(slider, s);
    spin.set_range(0, 10);
    spin.set_step(2);
    spin.set_value(9);
    CK_CHECK(spin.on_key(key(Key::Up)));
    CK_CHECK(spin.value() == 10);

    slider.set_bounds(ckv::Rect{0, 0, 11, 1});
    slider.set_range(0, 10);
    CK_CHECK(slider.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                             ckv::Point{5, 0}, std::nullopt, Modifier::None}));
    CK_CHECK(slider.value() == 5);
}

CK_TEST(search_box_and_breadcrumb_emit_client_callbacks) {
    Standalone s;
    SearchBox search;
    BreadcrumbBar crumbs;
    attach_standalone(search, s);
    attach_standalone(crumbs, s);
    std::string query;
    search.on_change = [&](const std::string& value) { query = value; };
    CK_CHECK(search.field().on_text(ckv::TextEvent{"abc", false}));
    CK_CHECK(query == "abc");
    CK_CHECK(search.on_key(key(Key::Escape)));
    CK_CHECK(search.query().empty());

    crumbs.set_bounds(ckv::Rect{0, 0, 30, 1});
    crumbs.set_segments({"root", "src", "ui"});
    std::optional<std::size_t> activated;
    crumbs.on_activate = [&](std::size_t index) { activated = index; };
    CK_CHECK(crumbs.on_key(key(Key::Right)));
    CK_CHECK(crumbs.on_key(key(Key::Enter)));
    CK_CHECK(activated == std::optional<std::size_t>{1});
}

CK_TEST(toolbar_and_command_palette_are_command_registry_surfaces) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app(term, clock);
    int ran = 0;
    const ckv::ui::CommandId command = app.commands().declare(
        ckv::ui::CommandDescriptor{.key = "test.build", .title = "Build", .category = "test",
                                   .handler = [&] { ++ran; }});

    auto toolbar = std::make_unique<ToolBar>();
    toolbar->set_bounds(ckv::Rect{0, 0, 20, 1});
    toolbar->set_items({CommandPresentation{command}});
    ToolBar* toolbar_view = toolbar.get();
    app.root().add(std::move(toolbar));
    // A button acts when the click completes over it, as any button does.
    CK_CHECK(toolbar_view->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                                    ckv::Point{1, 0}, std::nullopt, Modifier::None}));
    CK_CHECK(ran == 0);
    CK_CHECK(toolbar_view->on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left,
                                                    ckv::Point{1, 0}, std::nullopt, Modifier::None}));
    CK_CHECK(ran == 1);

    auto palette = std::make_unique<CommandPalette>();
    CommandPalette* palette_ptr = palette.get();
    app.root().add(std::move(palette));
    // The palette lists exactly what declares itself browsable: this test's
    // own command, and none of the framework's Hidden standard set.
    const auto palette_commands = palette_ptr->filtered_commands();
    CK_CHECK(std::all_of(palette_commands.begin(), palette_commands.end(),
                         [](const ckv::ui::CommandInfo& info) {
                             return info.visibility == ckv::ui::CommandVisibility::Palette;
                         }));
    CK_CHECK(std::none_of(palette_commands.begin(), palette_commands.end(),
                          [&app](const ckv::ui::CommandInfo& info) {
                              return info.id == standard(app).quit;
                          }));
    palette_ptr->set_query("bui");
    CK_CHECK(palette_ptr->highlighted_command() == command);
    CK_CHECK(palette_ptr->on_key(key(Key::Enter)));
    CK_CHECK(ran == 2);
}

CK_TEST(command_palette_lists_and_runs_what_the_place_it_was_opened_from_allows) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app(term, clock);
    int saved = 0;
    const ckv::ui::CommandId save = app.commands().declare(
        ckv::ui::CommandDescriptor{.key = "test.save", .title = "Save", .category = "test",
                                   .scope = {.contexts = {"document"}},
                                   .handler = [&] { ++saved; }});

    auto* document = app.root().add(std::make_unique<ckv::ui::View>());
    document->set_command_context("document");
    document->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
    app.set_focus(document);

    // A palette in a place of its own offers what that place allows.
    auto palette = std::make_unique<CommandPalette>();
    CommandPalette* palette_ptr = palette.get();
    app.root().add(std::move(palette));
    palette_ptr->set_query("save");
    app.set_focus(palette_ptr);
    CK_CHECK(!palette_ptr->highlighted_command().has_value());

    // Told where the reader was, it offers and runs that place's commands.
    palette_ptr->set_invocation_contexts(ckv::ui::command_context_path(document));
    CK_CHECK(palette_ptr->highlighted_command() == save);
    CK_CHECK(palette_ptr->on_key(key(Key::Enter)));
    CK_CHECK(saved == 1);

    // The reader types the title as the row shows it, mnemonic marker and all
    // left out.
    const ckv::ui::CommandId save_as = app.commands().declare(
        ckv::ui::CommandDescriptor{.key = "test.save-as", .title = "Save &as...", .category = "test",
                                   .handler = [] {}});
    palette_ptr->set_query("save as");
    CK_CHECK(palette_ptr->highlighted_command() == save_as);

    // Placed inside the document itself, its own focus path already says so.
    auto nested = std::make_unique<CommandPalette>();
    CommandPalette* nested_ptr = nested.get();
    document->add(std::move(nested));
    nested_ptr->set_query("save");
    app.set_focus(nested_ptr);
    CK_CHECK(nested_ptr->highlighted_command() == save);
    CK_CHECK(nested_ptr->on_key(key(Key::Enter)));
    CK_CHECK(saved == 2);
}

CK_TEST(property_inspector_wizard_notifications_and_tooltip_cover_utility_components) {
    Standalone s;
    PropertyInspector inspector;
    Wizard wizard;
    NotificationCenter notifications;
    Tooltip tooltip{"Helpful"};
    attach_standalone(inspector, s);
    attach_standalone(wizard, s);
    attach_standalone(notifications, s);
    attach_standalone(tooltip, s);

    // A check box needs no editor, and so no Application to hand one the
    // keyboard: Enter toggles it where it stands.
    std::optional<std::string> changed;
    inspector.set_items({PropertyItem{"Visible", "false", true, PropertyKind::Bool}});
    inspector.on_change = [&](std::size_t, std::string value) { changed = std::move(value); };
    CK_CHECK(inspector.on_key(key(Key::Enter)));
    CK_CHECK(changed == std::string{"true"});

    bool valid = false;
    std::optional<WizardOutcome> outcome;
    wizard.set_pages({WizardPage{"Step 1", [&] { return valid; }}, WizardPage{"Step 2", [] { return true; }}});
    wizard.on_complete = [&](WizardOutcome ended) { outcome = ended; };
    CK_CHECK(!wizard.next());
    valid = true;
    CK_CHECK(wizard.next());
    CK_CHECK(wizard.finish());
    CK_CHECK(outcome == WizardOutcome::Finished);

    notifications.add(Notification{NotificationSeverity::Warning, "Saved", true});
    CK_CHECK(notifications.notifications().size() == 1);
    CK_CHECK(notifications.on_key(key(Key::Escape)));
    CK_CHECK(notifications.notifications().empty());

    tooltip.show_at(ckv::Point{3, 4});
    CK_CHECK(tooltip.shown());
    tooltip.hide();
    CK_CHECK(!tooltip.shown());
}

CK_TEST(the_command_palette_accepts_ordinary_typed_characters) {
    // The same regression as the search box's: a terminal reports typed
    // characters as Key::Char events, and the palette's query only ever
    // took TextEvent, so a reader could not type into it.
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app(term, clock);
    static_cast<void>(app.commands().declare(ckv::ui::CommandDescriptor{
        .key = "test.build", .title = "Build", .category = "test", .handler = [] {}}));
    auto palette = std::make_unique<CommandPalette>();
    CommandPalette* palette_ptr = palette.get();
    app.root().add(std::move(palette));
    const auto type = [palette_ptr](const char* text, Modifier modifiers = Modifier::None) {
        return palette_ptr->on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, modifiers, text}});
    };
    CK_CHECK(type("b"));
    CK_CHECK(type("u"));
    CK_CHECK(palette_ptr->query() == "bu");
    CK_CHECK(palette_ptr->highlighted_command().has_value());
    // A chord is the registry's, not the query's.
    CK_CHECK(!type("x", Modifier::Alt));
    CK_CHECK(palette_ptr->query() == "bu");
}

CK_TEST(a_search_box_accepts_ordinary_typed_characters) {
    // Regression: the box only ever appended text from TextEvent, but a
    // terminal reports typed characters as Key::Char key events — TextEvent
    // is for IMEs and paste. The box therefore could not be typed into at
    // all, which is the one thing it exists to do.
    // Driven through the Application, as a reader types: the keys go to the
    // box's field, and what the field leaves unhandled goes on to the box.
    ckv::term::HeadlessTerminal term{ckv::Size{40, 5}};
    ManualClock clock;
    Application app(term, clock);
    auto* box = app.root().add(std::make_unique<ckv::widgets::SearchBox>());
    box->set_bounds(ckv::Rect{0, 0, 30, 1});
    app.set_focus(&box->field());
    std::vector<std::string> observed;
    box->on_change = [&observed](const std::string& query) { observed.push_back(query); };

    const auto press = [&app](ckv::Key k, const char* text = "", ckv::Modifier modifiers = ckv::Modifier::None) {
        const bool handled = app.dispatch(ckv::KeyEvent{ckv::KeyChord{k, modifiers, text}});
        app.step(0);
        return handled;
    };
    CK_CHECK(press(ckv::Key::Char, "v"));
    CK_CHECK(press(ckv::Key::Char, "i"));
    CK_CHECK(press(ckv::Key::Char, "m"));
    CK_CHECK(box->query() == "vim");
    CK_CHECK(observed.size() == 3U);
    CK_CHECK(observed.back() == "vim");

    // Backspace and Esc keep working, and Esc reports the clear.
    CK_CHECK(press(ckv::Key::Backspace));
    CK_CHECK(box->query() == "vi");
    CK_CHECK(press(ckv::Key::Escape));
    CK_CHECK(box->query().empty());
    // With nothing left to clear, Esc is the enclosing dialog's to cancel.
    CK_CHECK(!press(ckv::Key::Escape));

    // A modified character is a chord, not text: command routing must still
    // be able to claim it.
    CK_CHECK(!press(ckv::Key::Char, "q", ckv::Modifier::Ctrl));
    CK_CHECK(box->query().empty());
}

CK_TEST(a_search_box_looks_like_a_field_and_its_clear_control_is_where_it_is_drawn) {
    // Regression: the box drew "Search: <query> [x]" as one run of
    // label-coloured cells, so it read as a caption rather than something to
    // type into — and the "[x]" landed wherever the query happened to end,
    // while only the last three columns answered a click.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    const ckv::ui::Theme& theme = app.theme();
    auto* box = app.root().add(std::make_unique<ckv::widgets::SearchBox>());
    box->set_bounds(ckv::Rect{0, 0, 20, 1});
    app.step(0);
    const auto row = [&app] {
        std::string out;
        for (int x = 0; x < 20; ++x) out += app.composed_surface().at(ckv::Point{x, 0}).grapheme();
        return out;
    };
    const auto press_at = [&app](int x) {
        const bool handled = app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                                          ckv::Point{x, 0}, std::nullopt, ckv::Modifier::None});
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{x, 0}, std::nullopt,
                                     ckv::Modifier::None});
        app.step(0);
        return handled;
    };

    // The field carries the input surface's own background, which is what
    // says "text goes here"; the prompt does not.
    const ckv::Style field = app.composed_surface().at(ckv::Point{10, 0}).style();
    const ckv::Style prompt = app.composed_surface().at(ckv::Point{0, 0}).style();
    CK_CHECK(field.bg == theme.resolve(roles.input_normal).bg);
    CK_CHECK(field.bg != prompt.bg);

    // With no query there is nothing to clear, so no control is offered: a
    // press on the last column is a press in the field.
    CK_CHECK(row().find("[x]") == std::string::npos);
    CK_CHECK(press_at(19));
    CK_CHECK(app.focused() == &box->field());

    // ...and once there is, it is drawn at the right edge, exactly where a
    // click on it is answered.
    box->set_query("vim");
    app.step(0);
    CK_CHECK(row().find("vim") != std::string::npos);
    CK_CHECK(row().rfind("[x]") == 17U);  // the last three columns of a 20-wide box

    CK_CHECK(press_at(18));
    CK_CHECK(box->query().empty());
}

CK_TEST(a_focused_search_box_shows_a_caret_where_typing_will_land) {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app(term, clock);
    app.theme() = ckv::ui::make_classic_theme(app.roles(), ckv::ui::intern_standard_roles(app.roles()));
    auto* box = app.root().add(std::make_unique<ckv::widgets::SearchBox>());
    box->set_bounds(ckv::Rect{0, 0, 20, 1});
    app.step(0);
    // The caret is the field's, drawn as every input line draws it: a
    // reverse-video cell where typing lands. "Search " takes seven columns.
    const auto caret_at = [&app](int x) {
        return ckv::has_attr(app.composed_surface().at(ckv::Point{x, 0}).style().attrs, ckv::Attr::Reverse);
    };
    CK_CHECK(!caret_at(7));  // unfocused: no caret

    app.set_focus(&box->field());
    app.step(0);
    CK_CHECK(caret_at(7));

    box->set_query("vi");
    app.step(0);
    CK_CHECK(!caret_at(7));
    CK_CHECK(caret_at(9));  // follows the text
}

// --- Calendar: today, and a marked span --------------------------------------

namespace {
// The style of the cell holding `day`, for an August 2026 calendar drawn at
// the origin. Mirrors CalendarView::draw's own layout arithmetic.
ckv::Style day_style(ckv::scene::Surface& surface, CalendarView& calendar, int day) {
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 30, 10});
    calendar.set_bounds(ckv::Rect{0, 0, 30, 10});
    calendar.draw(painter);
    // 1 Aug 2026 is a Saturday: Monday-zero index 5.
    const int index = 5 + day - 1;
    return surface.at(ckv::Point{(index % 7) * 3 + 1, 2 + index / 7}).style();
}
}  // namespace

CK_TEST(a_calendar_marks_today_and_stops_when_told_there_is_no_today) {
    Standalone s;
    CalendarView calendar;
    attach_standalone(calendar, s);
    calendar.set_month(DateValue{2026, 8, 1});
    calendar.set_selected(DateValue{2026, 8, 20});
    ckv::scene::Surface surface(ckv::Size{30, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));

    const ckv::Style today_role = s.theme.resolve(s.roles.calendar_today);
    calendar.set_today(DateValue{2026, 8, 9});
    CK_CHECK(day_style(surface, calendar, 9).bg == today_role.bg);

    // std::nullopt is the "do not mark today" option: a picker for a
    // birthday has no use for it.
    calendar.set_today(std::nullopt);
    CK_CHECK(day_style(surface, calendar, 9).bg != today_role.bg);
}

CK_TEST(a_calendar_asks_again_for_today_so_it_can_turn_over_at_midnight) {
    // Left open across midnight a calendar goes on marking yesterday, which
    // is worse than marking nothing: confidently wrong about the one fact it
    // exists to state.
    Standalone s;
    CalendarView calendar;
    attach_standalone(calendar, s);
    calendar.set_month(DateValue{2026, 8, 1});
    calendar.set_selected(DateValue{2026, 8, 20});
    ckv::scene::Surface surface(ckv::Size{30, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));

    DateValue now{2026, 8, 9};
    calendar.set_today_provider([&] { return std::optional<DateValue>{now}; });
    const ckv::Style today_role = s.theme.resolve(s.roles.calendar_today);
    CK_CHECK(day_style(surface, calendar, 9).bg == today_role.bg);

    now = DateValue{2026, 8, 10};  // the day rolls over
    CK_CHECK(day_style(surface, calendar, 10).bg == today_role.bg);
    CK_CHECK(day_style(surface, calendar, 9).bg != today_role.bg);
}

CK_TEST(a_marked_span_is_clipped_to_the_month_on_display) {
    Standalone s;
    CalendarView calendar;
    attach_standalone(calendar, s);
    calendar.set_month(DateValue{2026, 8, 1});
    calendar.set_selected(DateValue{2026, 8, 28});
    calendar.set_today(std::nullopt);
    ckv::scene::Surface surface(ckv::Size{30, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    const ckv::Style marked = s.theme.resolve(s.roles.calendar_marked);

    // A span that starts in July and ends mid-August: only the August part
    // is on display, and it is the part that gets marked.
    calendar.set_marked_span(DateValue{2026, 7, 28}, DateValue{2026, 8, 5});
    CK_CHECK(day_style(surface, calendar, 1).bg == marked.bg);
    CK_CHECK(day_style(surface, calendar, 5).bg == marked.bg);
    CK_CHECK(day_style(surface, calendar, 6).bg != marked.bg);

    // Ends given the wrong way round mark nothing extra, not everything.
    calendar.set_marked_span(DateValue{2026, 8, 5}, DateValue{2026, 8, 1});
    CK_CHECK(day_style(surface, calendar, 3).bg == marked.bg);
    CK_CHECK(day_style(surface, calendar, 6).bg != marked.bg);

    // Cleared.
    calendar.set_marked_span(std::nullopt, std::nullopt);
    CK_CHECK(day_style(surface, calendar, 3).bg != marked.bg);
}

CK_TEST(the_day_a_reader_selected_outranks_today_and_the_marked_span) {
    // Each claim on a cell is stronger than the next: what cannot be chosen,
    // then what the reader chose, then what day it is, then the span.
    Standalone s;
    CalendarView calendar;
    attach_standalone(calendar, s);
    calendar.set_month(DateValue{2026, 8, 1});
    calendar.set_selected(DateValue{2026, 8, 9});
    calendar.set_today(DateValue{2026, 8, 9});
    calendar.set_marked_span(DateValue{2026, 8, 1}, DateValue{2026, 8, 31});
    ckv::scene::Surface surface(ckv::Size{30, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));

    // Without the keyboard the chosen day keeps its place in the muted
    // selection, as a list's does.
    CK_CHECK(day_style(surface, calendar, 9).bg == s.theme.resolve(s.roles.list_selected_inactive).bg);
    // A day that is today AND in the span shows today.
    calendar.set_selected(DateValue{2026, 8, 20});
    CK_CHECK(day_style(surface, calendar, 9).bg == s.theme.resolve(s.roles.calendar_today).bg);
}

CK_TEST(the_chosen_day_wears_the_full_highlight_while_the_calendar_holds_the_keyboard) {
    Standalone s;
    ckv::term::HeadlessTerminal terminal(ckv::Size{30, 10});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    CalendarView calendar;
    calendar.set_context(ckv::ui::Context{&s.theme, &s.registry, &app});
    calendar.on_attached();
    calendar.set_month(DateValue{2026, 8, 1});
    calendar.set_selected(DateValue{2026, 8, 9});
    ckv::scene::Surface surface(ckv::Size{30, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    app.set_focus(&calendar);
    CK_CHECK(day_style(surface, calendar, 9).bg == s.theme.resolve(s.roles.list_selected).bg);
}

// --- Clock -------------------------------------------------------------------

CK_TEST(a_clock_shows_the_time_its_host_supplies_in_the_format_asked_for) {
    Standalone s;
    ClockView clock;
    attach_standalone(clock, s);
    TimeValue now{13, 5, 9};
    clock.set_time_provider([&] { return now; });

    CK_CHECK(clock.text() == std::string("13:05"));
    clock.set_show_seconds(true);
    CK_CHECK(clock.text() == std::string("13:05:09"));

    // Twelve-hour, with the words the host chose -- ckVision carries no
    // locale data of its own.
    clock.set_hour_format(HourFormat::TwelveHour);
    CK_CHECK(clock.text() == std::string("1:05:09 PM"));
    clock.set_meridiem_labels("vorm.", "nachm.");
    CK_CHECK(clock.text() == std::string("1:05:09 nachm."));
    clock.set_meridiem_labels("", "");  // suppress the suffix entirely
    CK_CHECK(clock.text() == std::string("1:05:09"));
}

CK_TEST(midnight_and_noon_read_as_twelve_not_zero) {
    Standalone s;
    ClockView clock;
    attach_standalone(clock, s);
    TimeValue now{0, 30, 0};
    clock.set_time_provider([&] { return now; });
    clock.set_hour_format(HourFormat::TwelveHour);
    CK_CHECK(clock.text() == std::string("12:30 AM"));
    now = TimeValue{12, 30, 0};
    clock.set_show_seconds(false);
    clock.set_hour_format(HourFormat::TwentyFour);
    clock.set_hour_format(HourFormat::TwelveHour);  // force a re-render
    CK_CHECK(clock.text() == std::string("12:30 PM"));
}

CK_TEST(a_blinking_separator_hides_the_colon_and_never_the_digits) {
    // A clock whose numbers flicker is unreadable, and the separator carries
    // no information -- so it is the only thing that blinks.
    Standalone s;
    ClockView clock;
    attach_standalone(clock, s);
    clock.set_time_provider([] { return TimeValue{9, 41, 0}; });
    clock.set_blinking_separator(true);
    CK_CHECK(clock.text() == std::string("09:41"));
    CK_CHECK(clock.text().find("41") != std::string::npos);
}

CK_TEST(a_clock_that_shows_no_seconds_is_sized_for_what_it_shows) {
    Standalone s;
    ClockView clock;
    attach_standalone(clock, s);
    clock.set_time_provider([] { return TimeValue{9, 41, 7}; });
    const int without = clock.horizontal_size_hint().preferred;
    clock.set_show_seconds(true);
    const int with = clock.horizontal_size_hint().preferred;
    CK_CHECK(with == without + 3);  // ":07"
}

CK_TEST(a_clock_tells_its_container_when_it_becomes_a_different_width) {
    // Being sized for what it shows is only half of it. A container places its
    // children once, so a clock that silently grows gets drawn into the space
    // it used to need -- 09:41:0, with the last digit off the end.
    struct Container : ckv::ui::View {
        void on_child_size_hint_changed(ckv::ui::View&) override { ++notifications; }
        int notifications = 0;
    };
    Standalone s;
    Container container;
    attach_standalone(container, s);
    auto* clock = static_cast<ClockView*>(container.add_child(std::make_unique<ClockView>()));
    clock->set_time_provider([] { return TimeValue{9, 41, 7}; });

    const int before = container.notifications;
    clock->set_show_seconds(true);  // "09:41" -> "09:41:07": three cells wider
    CK_CHECK(container.notifications == before + 1);

    // And says nothing when the width is unchanged: a container that relaid
    // out its children every second for no reason would be the other half of
    // this same mistake.
    const int after = container.notifications;
    clock->set_blinking_separator(true);  // a colon becomes a space, same width
    CK_CHECK(container.notifications == after);
}

// --- Calendar: where the week starts ------------------------------------------

CK_TEST(the_week_starts_where_the_reader_expects_it_to) {
    // 1 August 2026 is a Saturday. Counting from Monday it is the sixth
    // column; counting from Sunday, the seventh.
    Standalone s;
    CalendarView calendar;
    attach_standalone(calendar, s);
    calendar.set_month(DateValue{2026, 8, 1});
    calendar.set_selected(DateValue{2026, 8, 1});
    calendar.set_today(std::nullopt);
    calendar.set_bounds(ckv::Rect{0, 0, 30, 10});
    ckv::scene::Surface surface(ckv::Size{30, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));

    const auto column_of_first = [&] {
        ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 30, 10});
        painter.fill(ckv::Rect{0, 0, 30, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
        calendar.draw(painter);
        for (int column = 0; column < 7; ++column)
            if (surface.at(ckv::Point{column * 3 + 1, 2}).grapheme() == "1") return column;
        return -1;
    };

    calendar.set_first_weekday(Weekday::Monday);
    CK_CHECK(column_of_first() == 5);
    calendar.set_first_weekday(Weekday::Sunday);
    CK_CHECK(column_of_first() == 6);
}

CK_TEST(a_clock_repaints_when_the_time_changes_and_not_on_every_tick) {
    // The whole claim: a clock without seconds costs one string comparison a
    // second and one repaint a minute. Measured at the terminal, because
    // that is where the cost lands.
    ckv::term::HeadlessTerminal term(ckv::Size{40, 6});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);

    TimeValue now{9, 41, 0};
    auto* view = static_cast<ClockView*>(app.root().add_child(std::make_unique<ClockView>()));
    view->set_time_provider([&] { return now; });
    view->set_bounds(ckv::Rect{0, 0, 10, 1});
    app.step(0);
    CK_CHECK(!term.written_bytes().empty());  // first frame draws it

    // Ten seconds pass with the minute unchanged: the clock ticks ten times
    // and asks for nothing.
    term.clear_written();
    std::int64_t at = clock.now_nanos();
    for (int i = 0; i < 10; ++i) {
        now.second = i + 1;
        at += 1'000'000'000;
        clock.advance(1'000'000'000);
        app.step(at);
    }
    CK_CHECK(term.written_bytes().empty());

    // The minute turns over: now it repaints.
    now = TimeValue{9, 42, 0};
    at += 1'000'000'000;
    clock.advance(1'000'000'000);
    app.step(at);
    CK_CHECK(!term.written_bytes().empty());
    CK_CHECK(view->text() == std::string("09:42"));
}

CK_TEST(a_calendar_dropdown_hangs_right_aligned_under_its_anchor_and_closes_outside) {
    ckv::term::HeadlessTerminal term(ckv::Size{60, 20});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto* desktop = static_cast<ckv::widgets::Desktop*>(
        app.root().add_child(std::make_unique<ckv::widgets::Desktop>(app.root().bounds())));
    auto* anchor = static_cast<ClockView*>(desktop->add_child(std::make_unique<ClockView>()));
    anchor->set_time_provider([] { return TimeValue{9, 41, 0}; });
    anchor->set_bounds(ckv::Rect{52, 0, 5, 1});  // at the right end, as in a menu bar
    app.step(0);

    CalendarDropdown* dropdown = show_calendar_dropdown(*anchor, app, *desktop);
    app.step(0);
    // Right edges aligned, the way a submenu hangs from the right end.
    CK_CHECK(dropdown->bounds().right() == anchor->bounds().right());
    CK_CHECK(dropdown->bounds().y == anchor->bounds().bottom());
    CK_CHECK(app.input_capture() == dropdown);

    // A press outside closes it, and gives up the capture with it.
    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 10},
                                  std::nullopt, ckv::Modifier::None});
    app.step(0);
    CK_CHECK(app.input_capture() == nullptr);
}

CK_TEST(a_hosted_date_picker_opens_the_calendar_and_tracks_its_typed_selection) {
    ckv::term::HeadlessTerminal term(ckv::Size{60, 20});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto* desktop = static_cast<ckv::widgets::Desktop*>(
        app.root().add_child(std::make_unique<ckv::widgets::Desktop>(app.root().bounds())));
    auto* picker = static_cast<DatePicker*>(desktop->add_child(std::make_unique<DatePicker>()));
    picker->set_bounds(ckv::Rect{4, 2, 14, 1});
    picker->set_seed(DateValue{2026, 8, 25});
    picker->set_calendar_host(app, *desktop);
    app.set_focus(picker);
    app.step(0);

    CK_CHECK(app.dispatch(ckv::KeyEvent{ckv::KeyChord{Key::Char, Modifier::None, " "}}));
    CK_CHECK(dynamic_cast<CalendarDropdown*>(app.input_capture()) != nullptr);
    CK_CHECK(app.dispatch(key(Key::Right)));
    CK_CHECK((picker->value() == std::optional<DateValue>{DateValue{2026, 8, 26}}));
    CK_CHECK(app.dispatch(key(Key::Escape)));
    CK_CHECK(app.input_capture() == nullptr);
}

CK_TEST(a_right_aligned_dropdown_is_pulled_back_when_its_anchor_sits_near_the_left) {
    // Right-alignment can only push a wide calendar off the LEFT edge: under
    // an anchor at the right end it always fits. An anchor near the left is
    // therefore the case worth pinning.
    ckv::term::HeadlessTerminal term(ckv::Size{40, 20});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto* desktop = static_cast<ckv::widgets::Desktop*>(
        app.root().add_child(std::make_unique<ckv::widgets::Desktop>(app.root().bounds())));
    auto* anchor = static_cast<ClockView*>(desktop->add_child(std::make_unique<ClockView>()));
    anchor->set_time_provider([] { return TimeValue{9, 41, 0}; });
    anchor->set_bounds(ckv::Rect{2, 0, 4, 1});
    app.step(0);

    CalendarDropdown* dropdown = show_calendar_dropdown(*anchor, app, *desktop);
    CK_CHECK(dropdown->bounds().x == 0);  // pulled back rather than off-screen
    CK_CHECK(dropdown->bounds().right() <= desktop->bounds().width);
}

namespace {
// A dropped calendar with its own application around it -- the popup scopes
// input, so the fixture has to be a whole application, not a bare view.
struct DroppedCalendar {
    ckv::term::HeadlessTerminal term{ckv::Size{60, 20}};
    ckv::ManualClock clock;
    ckv::ui::Application app{term, clock};
    ckv::widgets::Desktop* desktop = nullptr;
    ClockView* anchor = nullptr;
    CalendarDropdown* dropdown = nullptr;

    DroppedCalendar() {
        const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        desktop = static_cast<ckv::widgets::Desktop*>(
            app.root().add_child(std::make_unique<ckv::widgets::Desktop>(app.root().bounds())));
        anchor = static_cast<ClockView*>(desktop->add_child(std::make_unique<ClockView>()));
        anchor->set_time_provider([] { return TimeValue{9, 41, 0}; });
        anchor->set_bounds(ckv::Rect{52, 0, 5, 1});
        app.step(0);
        dropdown = show_calendar_dropdown(*anchor, app, *desktop);
        dropdown->show_month(DateValue{2026, 8, 1});
        app.step(0);
    }

    void press(ckv::Key k, ckv::Modifier m = ckv::Modifier::None, std::string text = {}) {
        app.dispatch(ckv::KeyEvent{ckv::KeyChord{k, m, std::move(text)}});
        app.step(0);
    }

    void click(ckv::Point cell) {
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, cell, std::nullopt,
                                     ckv::Modifier::None});
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, cell, std::nullopt,
                                     ckv::Modifier::None});
        app.step(0);
    }

    std::string row(int y) const {
        const ckv::Rect abs = dropdown->absolute_bounds();
        std::string out;
        for (int x = abs.x; x < abs.right(); ++x) out += app.composed_surface().at(ckv::Point{x, y}).grapheme();
        return out;
    }
};
}  // namespace

CK_TEST(a_dropped_calendar_opens_on_the_month_it_shows) {
    DroppedCalendar c;
    // Opening on August with nothing chosen would make the reader pick the
    // month they are already looking at.
    CK_CHECK(c.dropdown->month_picker().selected_index() == std::optional<std::size_t>{7});
    CK_CHECK(c.dropdown->year_field().text() == "2026");
}

CK_TEST(tab_walks_the_dropped_calendars_own_controls_and_no_further) {
    DroppedCalendar c;
    // The days are what the reader came for, so that is where focus lands.
    CK_CHECK(c.app.focused() == &c.dropdown->calendar());
    c.press(ckv::Key::Tab);
    CK_CHECK(c.app.focused() == &c.dropdown->month_picker());
    c.press(ckv::Key::Tab);
    CK_CHECK(c.app.focused() == &c.dropdown->year_field());
    c.press(ckv::Key::Tab);
    CK_CHECK(c.app.focused() == &c.dropdown->calendar());  // round, never out
}

CK_TEST(picking_a_month_moves_the_calendar_to_it) {
    DroppedCalendar c;
    c.press(ckv::Key::Tab);   // onto the month picker
    const int height = c.dropdown->bounds().height;
    c.press(ckv::Key::Down);  // open the list
    CK_CHECK(c.dropdown->month_picker().dropdown_open());
    // The list floats above the popup rather than being drawn inside it, so
    // nothing under it moves, hides or resizes.
    CK_CHECK(c.desktop->popups().size() == 2);
    CK_CHECK(c.dropdown->bounds().height == height);
    CK_CHECK(c.dropdown->calendar().visible());
    CK_CHECK(c.dropdown->year_field().visible());
    for (int i = 0; i < 3; ++i) c.press(ckv::Key::Down);  // August -> November
    c.press(ckv::Key::Enter);
    CK_CHECK(c.desktop->popups().size() == 1);  // the list is gone, the calendar is not
    CK_CHECK(c.dropdown->calendar().month().month == 11);
    CK_CHECK(c.dropdown->calendar().month().year == 2026);
}

CK_TEST(the_year_steppers_move_a_year_at_a_time) {
    DroppedCalendar c;
    const ckv::Rect abs = c.dropdown->absolute_bounds();
    // Month, steppers and year share the one control row.
    const ckv::Rect year = c.dropdown->year_field().bounds();
    c.click(ckv::Point{abs.x + year.x - 1, abs.y + year.y});  // "<<", left of the field
    CK_CHECK(c.dropdown->calendar().month().year == 2025);
    CK_CHECK(c.dropdown->year_field().text() == "2025");
    c.click(ckv::Point{abs.x + year.right() + 1, abs.y + year.y});  // ">>", right of it
    CK_CHECK(c.dropdown->calendar().month().year == 2026);
}

CK_TEST(a_typed_year_takes_digits_only_and_moves_the_calendar_on_enter) {
    DroppedCalendar c;
    c.press(ckv::Key::Tab);
    c.press(ckv::Key::Tab);  // onto the year field, its year selected
    c.press(ckv::Key::End);  // kept, the caret after it
    c.press(ckv::Key::Backspace);
    c.press(ckv::Key::Char, ckv::Modifier::None, "x");  // not part of a year
    c.press(ckv::Key::Char, ckv::Modifier::None, "5");
    CK_CHECK(c.dropdown->year_field().text() == "2025");
    c.press(ckv::Key::Enter);
    CK_CHECK(c.dropdown->calendar().month().year == 2025);
}

CK_TEST(leaving_the_year_field_commits_what_was_typed) {
    DroppedCalendar c;
    c.press(ckv::Key::Tab);
    c.press(ckv::Key::Tab);
    c.press(ckv::Key::End);
    c.press(ckv::Key::Backspace);
    c.press(ckv::Key::Char, ckv::Modifier::None, "4");
    c.press(ckv::Key::Tab);  // moving on says the same thing as Enter
    CK_CHECK(c.dropdown->calendar().month().year == 2024);
}

CK_TEST(a_year_that_is_not_one_is_refused_and_the_shown_year_comes_back) {
    DroppedCalendar c;
    c.press(ckv::Key::Tab);
    c.press(ckv::Key::Tab);
    for (int i = 0; i < 4; ++i) c.press(ckv::Key::Backspace);
    c.press(ckv::Key::Enter);  // nothing typed is not a year
    // The row says so, in the one place on it with room for a word.
    CK_CHECK(c.row(c.dropdown->absolute_bounds().y + 1).find("invalid") != std::string::npos);
    CK_CHECK(c.dropdown->calendar().month().year == 2026);  // unmoved
    // While it says that, the field is not a field: it steps aside, and it
    // takes nothing, so no digit is typed into the middle of the answer.
    CK_CHECK(!c.dropdown->year_field().visible());
    c.press(ckv::Key::Char, ckv::Modifier::None, "9");
    CK_CHECK(c.row(c.dropdown->absolute_bounds().y + 1).find("invalid") != std::string::npos);
    c.clock.advance(3'100'000'000);
    c.app.step(0);
    CK_CHECK(c.dropdown->year_field().visible());
    CK_CHECK(c.dropdown->year_field().text() == "2026");
    c.press(ckv::Key::Char, ckv::Modifier::None, "9");  // a field again
    CK_CHECK(c.dropdown->year_field().text() == "20269");
}

CK_TEST(the_steppers_are_buttons_that_arm_on_press_and_can_be_taken_back) {
    DroppedCalendar c;
    const ckv::Rect abs = c.dropdown->absolute_bounds();
    const ckv::Rect year = c.dropdown->year_field().bounds();
    const ckv::Point back{abs.x + year.x - 1, abs.y + year.y};
    c.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, back, std::nullopt,
                                   ckv::Modifier::None});
    c.app.step(0);
    CK_CHECK(c.dropdown->calendar().month().year == 2026);  // pressed is not fired
    // Released somewhere else, the press is taken back -- a stepper is a
    // button, not a hit-tested label.
    const ckv::Point away{back.x, back.y + 3};
    c.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, away, std::nullopt,
                                   ckv::Modifier::None});
    c.app.step(0);
    CK_CHECK(c.dropdown->calendar().month().year == 2026);
    c.click(back);
    CK_CHECK(c.dropdown->calendar().month().year == 2025);
}

CK_TEST(the_month_picker_is_as_wide_as_its_longest_month_and_no_wider) {
    DroppedCalendar c;
    // Space past the longest name is space the year beside it could have had,
    // and the year is on the same row.
    CK_CHECK(c.dropdown->month_picker().bounds().width == 10);  // "September" plus its arrow
    const ckv::Rect month = c.dropdown->month_picker().bounds();
    const ckv::Rect year = c.dropdown->year_field().bounds();
    CK_CHECK(year.y == month.y);
    CK_CHECK(year.width == 4);  // a year is four digits, and the field is four cells
    // A stepper's two cells sit between them, with two blanks before it so
    // the button stands clear of the picker's arrow.
    CK_CHECK(year.x - 2 - month.right() == 2);
    CK_CHECK(year.right() < c.dropdown->bounds().width - 1);  // and one more, inside the frame
}

CK_TEST(a_year_before_the_gregorian_calendar_is_refused) {
    DroppedCalendar c;
    c.press(ckv::Key::Tab);
    c.press(ckv::Key::Tab);  // onto the year field
    for (int i = 0; i < 4; ++i) c.press(ckv::Key::Backspace);
    c.press(ckv::Key::Char, ckv::Modifier::None, "2");
    c.press(ckv::Key::Char, ckv::Modifier::None, "6");
    c.press(ckv::Key::Enter);
    // Year 26 parses, and this widget's arithmetic would draw a confident
    // Gregorian grid for it -- for a year that had a different calendar.
    CK_CHECK(c.row(c.dropdown->absolute_bounds().y + 1).find("invalid") != std::string::npos);
    CK_CHECK(c.dropdown->calendar().month().year == 2026);
    c.clock.advance(3'100'000'000);
    c.app.step(0);

    for (int i = 0; i < 4; ++i) c.press(ckv::Key::Backspace);
    for (const char* d : {"1", "5", "8", "2"}) c.press(ckv::Key::Char, ckv::Modifier::None, d);
    c.press(ckv::Key::Enter);
    // 1582 is refused as well: October of it is missing ten days that never
    // happened, and this calendar draws whole months.
    CK_CHECK(c.dropdown->calendar().month().year == 2026);
    c.clock.advance(3'100'000'000);
    c.app.step(0);

    for (int i = 0; i < 4; ++i) c.press(ckv::Key::Backspace);
    for (const char* d : {"1", "5", "8", "3"}) c.press(ckv::Key::Char, ckv::Modifier::None, d);
    c.press(ckv::Key::Enter);
    CK_CHECK(c.dropdown->calendar().month().year == 1583);  // the first year it can state
}

CK_TEST(stepping_back_stops_at_the_first_year_the_calendar_can_draw) {
    DroppedCalendar c;
    c.dropdown->show_month(DateValue{ckv::widgets::kFirstCalendarYear, 3, 1});
    const ckv::Rect abs = c.dropdown->absolute_bounds();
    const ckv::Rect year = c.dropdown->year_field().bounds();
    c.click(ckv::Point{abs.x + year.x - 1, abs.y + year.y});  // "<<"
    CK_CHECK(c.dropdown->calendar().month().year == ckv::widgets::kFirstCalendarYear);
}

CK_TEST(a_picker_that_takes_the_mouse_does_not_leave_the_calendar_holding_it) {
    DroppedCalendar c;
    const ckv::Rect abs = c.dropdown->absolute_bounds();
    const ckv::Rect month = c.dropdown->month_picker().bounds();
    // Pressing the picker opens its list, and the list takes the mouse -- so
    // the release never comes back here. Left latched, that grab swallowed
    // every later event: the popup could not be dismissed and the
    // application behind it stopped responding.
    c.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                   ckv::Point{abs.x + month.x + 1, abs.y + month.y}, std::nullopt,
                                   ckv::Modifier::None});
    c.app.step(0);
    CK_CHECK(c.desktop->popups().size() == 2);  // the list is up
    c.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 18},
                                   std::nullopt, ckv::Modifier::None});
    c.app.step(0);  // outside everything: the list goes
    c.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 18},
                                   std::nullopt, ckv::Modifier::None});
    c.app.step(0);  // and so does the calendar
    CK_CHECK(c.desktop->popups().empty());
    CK_CHECK(c.app.input_capture() == nullptr);
    CK_CHECK(!c.app.is_modal());
}

CK_TEST(escape_closes_a_dropped_calendar) {
    DroppedCalendar c;
    c.press(ckv::Key::Escape);
    CK_CHECK(c.app.input_capture() == nullptr);
    CK_CHECK(!c.app.is_modal());  // the scope goes with it
}

CK_TEST(an_open_clock_wears_the_menu_bars_own_active_role) {
    // To a reader it is a menu title with its dropdown down, so it uses the
    // same two roles rather than a highlight of its own -- a theme dresses
    // both alike without being asked twice.
    Standalone s;
    ClockView clock;
    attach_standalone(clock, s);
    clock.set_time_provider([] { return TimeValue{9, 41, 0}; });
    clock.set_bounds(ckv::Rect{0, 0, 8, 1});
    ckv::scene::Surface surface(ckv::Size{8, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));

    const auto background_at = [&](int x) {
        ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 8, 1});
        clock.draw(painter);
        return surface.at(ckv::Point{x, 0}).style().bg;
    };
    const ckv::Style normal = s.theme.resolve(s.roles.menu_bar_normal);
    const ckv::Style active = s.theme.resolve(s.roles.menu_bar_active);
    CK_CHECK(normal.bg != active.bg);  // the check would prove nothing otherwise

    CK_CHECK(background_at(1) == normal.bg);
    clock.set_open(true);
    // The padding highlights with the text, so it reads as one item.
    CK_CHECK(background_at(0) == active.bg);
    CK_CHECK(background_at(1) == active.bg);
    clock.set_open(false);
    CK_CHECK(background_at(1) == normal.bg);
}

// --- Toasts that leave by themselves (WP-14) ------------------------------
//
// Two things a notification centre has to be able to do before an application
// can use it for the feedback that is NOT a question: take itself away when
// the reader has had time to read it, and, for the one message they must not
// miss, refuse to. Before this, `Notification::persistent` was a field nobody
// read — the workbench example sets it to true in good faith — and nothing
// expired at all.

namespace {

// `Standalone` above attaches with a null Application, which is right for the
// widgets that never ask the time. Expiry is measured on the injected Clock,
// so these cases need a real Application and a clock a test can move.
struct Timed {
    ckv::term::HeadlessTerminal term{ckv::Size{40, 10}};
    ManualClock clock;
    Application app{term, clock};
    ckv::ui::RoleRegistry registry;
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);

    void attach(ckv::ui::View& view) {
        view.set_context(ckv::ui::Context{&theme, &registry, &app});
        view.on_attached();
        view.set_bounds(ckv::Rect{0, 0, 40, 4});
    }
    // Moves time AND lets the application deliver what is now due: a clock
    // that advances with nobody stepping the loop is a clock nothing reads.
    void advance(std::int64_t nanos) {
        clock.advance(nanos);
        app.step(clock.now_nanos());
    }
};

constexpr std::int64_t kSecond = 1'000'000'000;

Notification info(std::string text, bool persistent = false) {
    return Notification{NotificationSeverity::Info, std::move(text), persistent};
}

ckv::MouseEvent click_row(int y) {
    return ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{0, y},
                           std::nullopt, Modifier::None};
}

}  // namespace

CK_TEST(with_no_interval_set_a_notification_stays_for_ever) {
    // The default, and the promise to every consumer written before this
    // widget could tell the time: nothing expires unless a host asks for it.
    Timed t;
    NotificationCenter centre;
    t.attach(centre);
    centre.add(info("Indexed commands are searchable"));

    t.advance(kSecond * 600);
    CK_CHECK(centre.notifications().size() == 1U);
}

CK_TEST(a_toast_leaves_by_itself_once_its_time_is_up) {
    Timed t;
    NotificationCenter centre;
    t.attach(centre);
    int changes = 0;
    centre.on_changed = [&] { ++changes; };
    centre.set_auto_dismiss(kSecond * 3);
    centre.add(info("Detached"));
    CK_CHECK(changes == 1);

    t.advance(kSecond * 2);
    CK_CHECK(centre.notifications().size() == 1U);  // not yet

    t.advance(kSecond * 2);
    CK_CHECK(centre.notifications().empty());
    // The host hears about it, which is the whole reason the callback exists:
    // expiry happens on a timer nobody outside this view can see, so a host
    // sizing itself from notifications().size() would otherwise be holding a
    // rectangle for rows that are gone.
    CK_CHECK(changes == 2);
}

CK_TEST(a_notification_the_reader_must_not_miss_outlives_every_interval) {
    Timed t;
    NotificationCenter centre;
    t.attach(centre);
    centre.set_auto_dismiss(kSecond);
    centre.add(info("'build' was taken over by ttys011", /*persistent=*/true));
    centre.add(info("Config reloaded"));

    t.advance(kSecond * 30);
    CK_CHECK(centre.notifications().size() == 1U);
    if (centre.notifications().empty()) return;
    CK_CHECK(centre.notifications()[0].text == "'build' was taken over by ttys011");
    CK_CHECK(centre.notifications()[0].persistent);
}

CK_TEST(each_toast_is_measured_from_when_it_was_posted) {
    Timed t;
    NotificationCenter centre;
    t.attach(centre);
    centre.set_auto_dismiss(kSecond * 3);
    centre.add(info("first"));
    t.advance(kSecond * 2);
    centre.add(info("second"));

    // Two seconds later the first is four seconds old and the second two.
    t.advance(kSecond * 2);
    CK_CHECK(centre.notifications().size() == 1U);
    if (centre.notifications().empty()) return;
    CK_CHECK(centre.notifications()[0].text == "second");

    t.advance(kSecond * 2);
    CK_CHECK(centre.notifications().empty());
}

CK_TEST(two_toasts_that_come_due_together_go_together) {
    Timed t;
    NotificationCenter centre;
    t.attach(centre);
    int changes = 0;
    centre.set_auto_dismiss(kSecond);
    centre.add(info("one"));
    centre.add(info("two"));
    centre.on_changed = [&] { ++changes; };

    t.advance(kSecond * 2);
    CK_CHECK(centre.notifications().empty());
    // One sweep, not two: what an expiry costs is a repaint, so waking once
    // and clearing everything due is both cheaper and the only order in which
    // two toasts that expired together leave together.
    CK_CHECK(changes == 1);
}

CK_TEST(shortening_the_interval_re_times_what_is_already_on_screen) {
    Timed t;
    NotificationCenter centre;
    t.attach(centre);
    centre.set_auto_dismiss(kSecond * 60);
    centre.add(info("Config reloaded"));
    t.advance(kSecond * 5);
    CK_CHECK(centre.notifications().size() == 1U);

    // A host that shortens its toasts means the one in front of the reader
    // too; leaving it on the old interval would make the setting take effect
    // at a moment nobody chose.
    centre.set_auto_dismiss(kSecond);
    t.advance(kSecond * 2);
    CK_CHECK(centre.notifications().empty());
}

CK_TEST(a_notification_posted_before_there_was_a_clock_starts_its_life_at_attach) {
    // "Post first, attach later" is an ordinary shape for an application that
    // builds its chrome after it has something to say. Those entries had no
    // clock to take a deadline from, so attaching is where they get one —
    // otherwise they would be accidentally immortal.
    Timed t;
    NotificationCenter centre;
    centre.set_auto_dismiss(kSecond * 2);
    centre.add(info("Started"));
    CK_CHECK(centre.notifications().size() == 1U);

    t.attach(centre);
    t.advance(kSecond * 3);
    CK_CHECK(centre.notifications().empty());
}

CK_TEST(a_click_takes_away_the_line_it_landed_on_and_leaves_the_rest) {
    Timed t;
    NotificationCenter centre;
    t.attach(centre);
    centre.add(info("first"));
    centre.add(info("second"));
    centre.add(info("third"));

    CK_CHECK(centre.on_mouse(click_row(1)));
    CK_CHECK(centre.notifications().size() == 2U);
    if (centre.notifications().size() != 2U) return;
    CK_CHECK(centre.notifications()[0].text == "first");
    CK_CHECK(centre.notifications()[1].text == "third");

    // Past the last line the centre draws nothing, so it takes nothing: a
    // press there belongs to whatever is underneath, and consuming it would
    // swallow a click aimed at the desktop.
    CK_CHECK(!centre.on_mouse(click_row(3)));
    CK_CHECK(centre.notifications().size() == 2U);
}

CK_TEST(a_click_dismisses_a_persistent_notification_too) {
    // Dismissal is the reader saying they have read it — which is exactly
    // what `persistent` was waiting for.
    Timed t;
    NotificationCenter centre;
    t.attach(centre);
    centre.set_auto_dismiss(kSecond);
    centre.add(info("'build' was taken over by ttys011", /*persistent=*/true));

    t.advance(kSecond * 10);
    CK_CHECK(centre.notifications().size() == 1U);
    CK_CHECK(centre.on_mouse(click_row(0)));
    CK_CHECK(centre.notifications().empty());
}

CK_TEST(escape_still_dismisses_the_most_recent_notification) {
    Timed t;
    NotificationCenter centre;
    t.attach(centre);
    centre.add(info("first"));
    centre.add(info("second"));

    CK_CHECK(centre.on_key(key(Key::Escape)));
    CK_CHECK(centre.notifications().size() == 1U);
    if (centre.notifications().empty()) return;
    CK_CHECK(centre.notifications()[0].text == "first");
}

CK_TEST(an_empty_centre_paints_nothing_at_all) {
    // What lets a host leave one lying over its desktop at a generous size
    // rather than resizing it on every post: the cells it does not write show
    // whatever is underneath, so an empty centre is invisible instead of
    // being a filled block in the middle of the reader's work.
    Timed t;
    NotificationCenter centre;
    t.attach(centre);
    centre.set_bounds(ckv::Rect{0, 0, 8, 3});

    ckv::scene::Surface surface(ckv::Size{8, 3}, ckv::Cell::from_grapheme("#", ckv::Style{}));
    {
        ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 8, 3});
        centre.draw(painter);
    }
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 8; ++x) CK_CHECK(surface.at(ckv::Point{x, y}).grapheme() == "#");

    // And one notification paints its own row and only its own row.
    centre.add(info("hi"));
    {
        ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 8, 3});
        centre.draw(painter);
    }
    CK_CHECK(surface.at(ckv::Point{0, 0}).grapheme() == "i");   // the Info marker
    CK_CHECK(surface.at(ckv::Point{0, 1}).grapheme() == "#");   // untouched
    CK_CHECK(surface.at(ckv::Point{0, 2}).grapheme() == "#");
}

CK_TEST(a_host_that_takes_the_focus_stop_away_keeps_it_away_across_attach) {
    // A centre a reader Tabs to is right in a form and wrong over a terminal:
    // there the notification is news, not something to answer, and a focus
    // stop would put the reader's Tab in a message about something that
    // already happened instead of in the program they are typing into. So the
    // default is a stop, and a host may say otherwise — attaching must not
    // quietly put it back.
    Timed t;
    NotificationCenter centre;
    CK_CHECK(centre.focus_policy() == ckv::ui::FocusPolicy::TabStop);  // the default

    centre.set_focus_policy(ckv::ui::FocusPolicy::None);
    t.attach(centre);
    CK_CHECK(centre.focus_policy() == ckv::ui::FocusPolicy::None);
}

// --- Fitting, focus faces and date arithmetic --------------------------------

namespace {

// A view whose context names a real Application, so a test can give it the
// keyboard; drawn by hand onto a surface the size of its bounds.
struct Hosted {
    ckv::term::HeadlessTerminal term{ckv::Size{40, 10}};
    ManualClock clock;
    Application app{term, clock};
    ckv::ui::RoleRegistry registry;
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);

    void attach(ckv::ui::View& view, ckv::Rect bounds) {
        view.set_context(ckv::ui::Context{&theme, &registry, &app});
        view.set_bounds(bounds);
    }
};

ckv::scene::Surface painted(ckv::ui::View& view) {
    const ckv::Size size{view.bounds().width, view.bounds().height};
    ckv::scene::Surface surface(size, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, size.width, size.height});
    view.draw(painter);
    return surface;
}

std::string row_of(const ckv::scene::Surface& surface, int y) {
    std::string row;
    for (int x = 0; x < surface.size().width; ++x) row += surface.at(ckv::Point{x, y}).grapheme();
    return row;
}

ckv::KeyEvent typed(const char* text, Modifier modifiers = Modifier::None) {
    return ckv::KeyEvent{KeyChord{Key::Char, modifiers, text}};
}

bool reversed(const ckv::scene::Surface& surface, ckv::Point cell) {
    return ckv::has_attr(surface.at(cell).style().attrs, ckv::Attr::Reverse);
}

// The two cells starting at `at`, as a calendar draws a day or a heading.
std::string two_cells(const ckv::scene::Surface& surface, ckv::Point at) {
    std::string text(surface.at(at).grapheme());
    text += surface.at(ckv::Point{at.x + 1, at.y}).grapheme();
    return text;
}

// The weekday heading the column that day 1 of `month` is drawn in.
std::string weekday_of_first(CalendarView& calendar, DateValue month) {
    calendar.set_month(month);
    calendar.set_bounds(ckv::Rect{0, 0, 30, 10});
    const ckv::scene::Surface surface = painted(calendar);
    for (int column = 0; column < 7; ++column)
        if (two_cells(surface, ckv::Point{column * 3, 2}) == " 1") return two_cells(surface, ckv::Point{column * 3, 1});
    return {};
}

// The week-number column of a calendar showing its title, one entry a row.
std::vector<std::string> week_numbers(CalendarView& calendar, int rows) {
    calendar.set_bounds(ckv::Rect{0, 0, 30, 10});
    const ckv::scene::Surface surface = painted(calendar);
    std::vector<std::string> weeks;
    for (int row = 0; row < rows; ++row) weeks.push_back(two_cells(surface, ckv::Point{0, 2 + row}));
    return weeks;
}

}  // namespace

CK_TEST(a_leap_year_month_starts_on_its_own_weekday) {
    // Every date of a leap year used to be drawn one weekday late: the year's
    // own leap day was counted from January 1, before it had happened.
    Standalone s;
    CalendarView calendar;
    attach_standalone(calendar, s);
    calendar.set_today(std::nullopt);
    CK_CHECK(weekday_of_first(calendar, DateValue{2024, 2, 1}) == "Th");
    CK_CHECK(weekday_of_first(calendar, DateValue{2024, 3, 1}) == "Fr");
    CK_CHECK(weekday_of_first(calendar, DateValue{2000, 1, 1}) == "Sa");
    CK_CHECK(weekday_of_first(calendar, DateValue{2026, 8, 1}) == "Sa");
}

CK_TEST(iso_week_numbers_label_each_row_with_the_week_of_its_monday) {
    Standalone s;
    CalendarView calendar;
    attach_standalone(calendar, s);
    calendar.set_today(std::nullopt);
    calendar.set_show_iso_week_numbers(true);

    calendar.set_month(DateValue{2026, 8, 1});
    CK_CHECK((week_numbers(calendar, 6) == std::vector<std::string>{"31", "32", "33", "34", "35", "36"}));

    // 1 January 2021 was a Friday, so the month's first row is the last week
    // of 2020 -- ISO week 53 -- and week 1 begins on the row after it.
    calendar.set_month(DateValue{2021, 1, 1});
    CK_CHECK((week_numbers(calendar, 5) == std::vector<std::string>{"53", " 1", " 2", " 3", " 4"}));
}

CK_TEST(a_focused_time_picker_marks_the_field_its_arrows_change) {
    Hosted h;
    TimePicker time;
    h.attach(time, ckv::Rect{0, 0, 12, 1});
    time.set_value(TimeValue{10, 20, 30});
    CK_CHECK(!reversed(painted(time), ckv::Point{0, 0}));  // without the keyboard, no field is marked

    h.app.set_focus(&time);
    ckv::scene::Surface surface = painted(time);
    CK_CHECK(reversed(surface, ckv::Point{0, 0}) && reversed(surface, ckv::Point{1, 0}));
    CK_CHECK(!reversed(surface, ckv::Point{2, 0}) && !reversed(surface, ckv::Point{3, 0}));

    int repaints = 0;
    time.set_dirty_rect_sink([&repaints](ckv::Rect) { ++repaints; });
    CK_CHECK(time.on_key(key(Key::Right)));
    CK_CHECK(repaints > 0);
    surface = painted(time);
    CK_CHECK(!reversed(surface, ckv::Point{0, 0}));
    CK_CHECK(reversed(surface, ckv::Point{3, 0}) && reversed(surface, ckv::Point{4, 0}));
}

CK_TEST(hiding_the_seconds_while_they_are_the_active_field_hands_the_arrows_to_the_minutes) {
    Hosted h;
    TimePicker time;
    h.attach(time, ckv::Rect{0, 0, 12, 1});
    time.set_value(TimeValue{10, 20, 30});
    h.app.set_focus(&time);
    CK_CHECK(time.on_key(key(Key::Right)));
    CK_CHECK(time.on_key(key(Key::Right)));  // on the seconds

    time.set_show_seconds(false);
    CK_CHECK(time.on_key(key(Key::Up)));
    CK_CHECK(time.value() == (TimeValue{10, 21, 30}));
    const ckv::scene::Surface surface = painted(time);
    CK_CHECK(reversed(surface, ckv::Point{3, 0}) && reversed(surface, ckv::Point{4, 0}));
}

CK_TEST(the_inspectors_cursor_row_is_a_full_width_bar_muted_without_the_keyboard) {
    Hosted h;
    PropertyInspector inspector;
    h.attach(inspector, ckv::Rect{0, 0, 20, 2});
    inspector.set_items({PropertyItem{"Name", "ckv", true}, PropertyItem{"Kind", "lib", false}});

    ckv::scene::Surface surface = painted(inspector);
    const ckv::Color muted = h.theme.resolve(h.roles.list_selected_inactive).bg;
    CK_CHECK(surface.at(ckv::Point{0, 0}).style().bg == muted);
    CK_CHECK(surface.at(ckv::Point{19, 0}).style().bg == muted);
    CK_CHECK(surface.at(ckv::Point{0, 1}).style().bg == h.theme.resolve(h.roles.list_normal).bg);

    h.app.set_focus(&inspector);
    surface = painted(inspector);
    const ckv::Color highlight = h.theme.resolve(h.roles.list_selected).bg;
    CK_CHECK(surface.at(ckv::Point{0, 0}).style().bg == highlight);
    CK_CHECK(surface.at(ckv::Point{19, 0}).style().bg == highlight);
}

CK_TEST(a_toolbar_button_shows_its_commands_title_without_the_mnemonic_marker) {
    ckv::term::HeadlessTerminal term{ckv::Size{40, 5}};
    ManualClock clock;
    Application app(term, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    int ran = 0;
    const ckv::ui::CommandId save = app.commands().declare(
        ckv::ui::CommandDescriptor{.key = "test.save", .title = "&Save", .category = "test",
                                   .handler = [&] { ++ran; }});
    auto* toolbar = static_cast<ToolBar*>(app.root().add_child(std::make_unique<ToolBar>()));
    toolbar->set_bounds(ckv::Rect{0, 0, 20, 1});
    toolbar->set_items({CommandPresentation{save}});

    const std::string row = row_of(painted(*toolbar), 0);
    CK_CHECK(row.starts_with("[Save]"));
    CK_CHECK(row.find('&') == std::string::npos);
    // The button answers exactly where it is drawn: the column after "]" is
    // the gap, not the button.
    const auto click = [&](int x) {
        const bool down = toolbar->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                                            ckv::Point{x, 0}, std::nullopt, Modifier::None});
        toolbar->on_mouse(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{x, 0},
                                          std::nullopt, Modifier::None});
        return down;
    };
    CK_CHECK(!click(6));
    CK_CHECK(ran == 0);
    CK_CHECK(click(5));
    CK_CHECK(ran == 1);
}

CK_TEST(a_wizard_page_that_holds_the_reader_back_offers_next_greyed_rather_than_finish) {
    Standalone s;
    Wizard wizard;
    attach_standalone(wizard, s);
    wizard.set_bounds(ckv::Rect{0, 0, 20, 3});
    bool valid = false;
    wizard.set_pages({WizardPage{"Step 1", [&] { return valid; }}, WizardPage{"Step 2", [] { return true; }}});

    // The action sits after the seven columns "< Back " takes, blank or not.
    ckv::scene::Surface surface = painted(wizard);
    CK_CHECK(row_of(surface, 2).substr(7, 6) == "Next >");
    CK_CHECK(surface.at(ckv::Point{7, 2}).style().fg == s.theme.resolve(s.roles.label_disabled).fg);
    CK_CHECK(surface.at(ckv::Point{7, 2}).style().bg == s.theme.resolve(s.roles.dialog_background).bg);

    valid = true;
    surface = painted(wizard);
    CK_CHECK(row_of(surface, 2).substr(7, 6) == "Next >");
    CK_CHECK(surface.at(ckv::Point{7, 2}).style().fg == s.theme.resolve(s.roles.dialog_background).fg);

    CK_CHECK(wizard.next());
    CK_CHECK(row_of(painted(wizard), 2).substr(7, 6) == "Finish");
}

CK_TEST(the_wizards_action_is_marked_as_enters_only_while_it_holds_the_keyboard) {
    Hosted h;
    Wizard wizard;
    h.attach(wizard, ckv::Rect{0, 0, 20, 3});
    bool valid = true;
    wizard.set_pages({WizardPage{"Step 1", [&] { return valid; }}, WizardPage{"Step 2", [] { return true; }}});
    CK_CHECK(!reversed(painted(wizard), ckv::Point{7, 2}));

    h.app.set_focus(&wizard);
    CK_CHECK(reversed(painted(wizard), ckv::Point{7, 2}));
    // A blocked page's action is greyed, not offered to Enter.
    valid = false;
    CK_CHECK(!reversed(painted(wizard), ckv::Point{7, 2}));
}

CK_TEST(a_spin_box_too_narrow_for_its_arrows_shows_the_number_and_never_a_cut_one) {
    Standalone s;
    SpinBox spin;
    attach_standalone(spin, s);
    spin.set_range(0, 100000);
    spin.set_value(12345);

    spin.set_bounds(ckv::Rect{0, 0, 9, 1});
    CK_CHECK(row_of(painted(spin), 0) == "< 12345 >");
    spin.set_bounds(ckv::Rect{0, 0, 8, 1});
    CK_CHECK(row_of(painted(spin), 0) == "12345   ");
    spin.set_bounds(ckv::Rect{0, 0, 4, 1});
    CK_CHECK(row_of(painted(spin), 0) == "123…");
}

CK_TEST(a_breadcrumb_bar_is_a_tab_stop_from_construction) {
    BreadcrumbBar crumbs;
    CK_CHECK(crumbs.focus_policy() == ckv::ui::FocusPolicy::TabStop);
    CK_CHECK(crumbs.focusable());
}

CK_TEST(a_query_longer_than_its_field_shows_its_end_with_the_caret_just_after_it) {
    Hosted h;
    h.app.theme() = ckv::ui::make_classic_theme(h.app.roles(), ckv::ui::intern_standard_roles(h.app.roles()));
    auto* box = h.app.root().add(std::make_unique<SearchBox>());
    box->set_bounds(ckv::Rect{0, 0, 20, 1});
    h.app.set_focus(&box->field());
    // "Search " takes seven columns and the clear control three: the field
    // is ten wide, nine for the text and one for the caret.
    box->set_query("abcdefghijklmnop");
    h.app.step(0);
    std::string row = row_of(h.app.composed_surface(), 0);
    CK_CHECK(row.substr(7, 10) == "hijklmnop ");
    CK_CHECK(row.find("abc") == std::string::npos);
    CK_CHECK(reversed(h.app.composed_surface(), ckv::Point{7 + 9, 0}));

    // Double-width text: four clusters fill eight of the nine columns, and
    // the caret follows the text it shows, not the text's full width.
    box->set_query("一二三四五六七八");
    h.app.step(0);
    row = row_of(h.app.composed_surface(), 0);
    CK_CHECK(row.find("五六七八") != std::string::npos);
    CK_CHECK(row.find("四") == std::string::npos);
    CK_CHECK(reversed(h.app.composed_surface(), ckv::Point{7 + 8, 0}));
}

CK_TEST(each_notifications_mark_wears_its_severitys_colour_over_the_centres_surface) {
    Timed t;
    NotificationCenter centre;
    t.attach(centre);
    centre.add(Notification{NotificationSeverity::Info, "news", false});
    centre.add(Notification{NotificationSeverity::Warning, "careful", false});
    centre.add(Notification{NotificationSeverity::Error, "broken", false});
    const ckv::scene::Surface surface = painted(centre);
    const ckv::Style line = t.theme.resolve(t.roles.status_line_normal);
    const ckv::ui::RoleId severities[] = {t.roles.message_info_text, t.roles.message_warning_text,
                                          t.roles.message_error_text};
    for (int row = 0; row < 3; ++row) {
        const ckv::Style mark = surface.at(ckv::Point{0, row}).style();
        CK_CHECK(mark.fg == t.theme.resolve(severities[row]).fg);
        CK_CHECK(mark.bg == line.bg);
        const ckv::Style text = surface.at(ckv::Point{2, row}).style();
        CK_CHECK(text.fg == line.fg);
        CK_CHECK(text.bg == line.bg);
    }
}

CK_TEST(the_line_escape_would_take_is_marked_only_while_the_centre_holds_the_keyboard) {
    Timed t;
    NotificationCenter centre;
    t.attach(centre);
    centre.add(info("first"));
    centre.add(info("second"));
    ckv::scene::Surface surface = painted(centre);
    CK_CHECK(!reversed(surface, ckv::Point{3, 0}));
    CK_CHECK(!reversed(surface, ckv::Point{3, 1}));

    t.app.set_focus(&centre);
    surface = painted(centre);
    CK_CHECK(!reversed(surface, ckv::Point{3, 0}));
    CK_CHECK(reversed(surface, ckv::Point{3, 1}));
    CK_CHECK(reversed(surface, ckv::Point{39, 1}));
}

CK_TEST(a_tooltip_given_less_room_than_its_text_elides_it_in_the_tooltip_role) {
    Standalone s;
    Tooltip tip{"Opens the selected file"};
    attach_standalone(tip, s);
    tip.show_at(ckv::Point{0, 0});
    tip.set_bounds(ckv::Rect{0, 0, 10, 1});
    const ckv::scene::Surface surface = painted(tip);
    CK_CHECK(row_of(surface, 0) == " Opens t… ");
    CK_CHECK(surface.at(ckv::Point{1, 0}).style() == s.theme.resolve(s.roles.tooltip));
}

// --- WP-38 review findings A1-A5 -------------------------------------------

CK_TEST(a_calendar_without_its_title_answers_a_click_on_its_first_week) {
    // A1: the rows a click maps to are the rows the days are drawn on. With
    // the title off (as in a CalendarDropdown) the first week is drawn on
    // row 1, and a click there has to choose a day of it.
    Standalone s;
    CalendarView calendar;
    attach_standalone(calendar, s);
    calendar.set_show_title(false);
    calendar.set_bounds(ckv::Rect{0, 0, 21, 7});
    calendar.set_selected(DateValue{2026, 8, 20});
    // 1 Aug 2026 is a Saturday: column 5 of a Monday-first week.
    CK_CHECK(calendar.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                               ckv::Point{15, 1}, std::nullopt, Modifier::None}));
    CK_CHECK(calendar.selected() == (DateValue{2026, 8, 1}));
    // The sixth week's row is the last one drawn: 31 Aug is on row 6.
    CK_CHECK(calendar.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                               ckv::Point{0, 6}, std::nullopt, Modifier::None}));
    CK_CHECK(calendar.selected() == (DateValue{2026, 8, 31}));
    // The weekday heading is not a week.
    CK_CHECK(!calendar.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                                ckv::Point{15, 0}, std::nullopt, Modifier::None}));
}

CK_TEST(backspace_in_a_search_box_and_a_palette_removes_a_whole_grapheme) {
    // A2: one grapheme, never one byte: a byte would leave the query holding
    // half a character, which is not text at all.
    Standalone s;
    SearchBox search;
    attach_standalone(search, s);
    search.set_query("café");
    CK_CHECK(search.field().on_key(key(Key::Backspace)));
    CK_CHECK(search.query() == "caf");
    search.set_query("é\U0001F44D");
    CK_CHECK(search.field().on_key(key(Key::Backspace)));
    CK_CHECK(search.query() == "é");
    CK_CHECK(search.field().on_key(key(Key::Backspace)));
    CK_CHECK(search.query().empty());

    CommandPalette palette;
    attach_standalone(palette, s);
    palette.set_query("x\U0001F44D");
    CK_CHECK(palette.on_key(key(Key::Backspace)));
    CK_CHECK(palette.query() == "x");
}

CK_TEST(new_inspector_rows_end_an_edit_and_a_read_only_row_takes_no_backspace) {
    // A3: an edit belongs to the row it began on; rows replaced under it end
    // it, so the first new row -- read-only here -- cannot be changed.
    Standalone s;
    PropertyInspector inspector;
    attach_standalone(inspector, s);
    inspector.set_items({PropertyItem{"Name", "value", true}});
    CK_CHECK(inspector.on_key(key(Key::Enter)));
    int changes = 0;
    inspector.on_change = [&](std::size_t, std::string) { ++changes; };

    inspector.set_items({PropertyItem{"Id", "fixed", false}});
    CK_CHECK(!inspector.on_key(key(Key::Backspace)));
    CK_CHECK(!inspector.on_key(typed("x")));
    CK_CHECK(inspector.items()[0].value == "fixed");
    CK_CHECK(changes == 0);
    CK_CHECK(!inspector.cursor_state().has_value());
}

CK_TEST(a_date_pickers_value_does_not_move_its_seed_or_the_calendars_today) {
    // A4: the seed is the caller's notion of today. The value the reader
    // settles on is a different fact, and must not become "today".
    ckv::term::HeadlessTerminal term(ckv::Size{60, 20});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto* desktop = static_cast<ckv::widgets::Desktop*>(
        app.root().add_child(std::make_unique<ckv::widgets::Desktop>(app.root().bounds())));
    auto* picker = static_cast<DatePicker*>(desktop->add_child(std::make_unique<DatePicker>()));
    picker->set_bounds(ckv::Rect{4, 2, 14, 1});
    picker->set_seed(DateValue{2026, 8, 25});
    picker->set_calendar_host(app, *desktop);
    app.step(0);

    picker->set_value(DateValue{2020, 1, 15});
    CK_CHECK(picker->seed() == (DateValue{2026, 8, 25}));
    CK_CHECK(picker->open_calendar());
    auto* dropdown = dynamic_cast<CalendarDropdown*>(app.input_capture());
    CK_CHECK(dropdown != nullptr);
    if (dropdown != nullptr) {
        CK_CHECK((dropdown->calendar().today() == std::optional<DateValue>{DateValue{2026, 8, 25}}));
        CK_CHECK(dropdown->calendar().selected() == (DateValue{2020, 1, 15}));
    }
}

CK_TEST(spin_box_and_slider_arithmetic_holds_across_the_whole_int_range) {
    // A5: stepping and mapping a pointer column to a value over the widest
    // range an int allows neither overflows nor wraps.
    constexpr int lowest = std::numeric_limits<int>::min();
    constexpr int highest = std::numeric_limits<int>::max();
    Standalone s;
    SpinBox spin;
    attach_standalone(spin, s);
    spin.set_range(lowest, highest);
    spin.set_step(highest);
    spin.set_value(highest - 1);
    CK_CHECK(spin.on_key(key(Key::Up)));
    CK_CHECK(spin.value() == highest);
    spin.set_value(lowest + 1);
    CK_CHECK(spin.on_key(key(Key::Down)));
    CK_CHECK(spin.value() == lowest);

    Slider slider;
    attach_standalone(slider, s);
    slider.set_bounds(ckv::Rect{0, 0, 11, 1});
    slider.set_range(lowest, highest);
    slider.set_step(highest);
    slider.set_value(lowest + 1);
    CK_CHECK(slider.on_key(key(Key::Left)));
    CK_CHECK(slider.value() == lowest);
    const auto click = [&](int x) {
        return slider.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                               ckv::Point{x, 0}, std::nullopt, Modifier::None});
    };
    CK_CHECK(click(10));
    CK_CHECK(slider.value() == highest);
    CK_CHECK(click(5));
    CK_CHECK(slider.value() == -1);  // halfway along 2^32 - 1 steps from the lowest int
    CK_CHECK(click(0));
    CK_CHECK(slider.value() == lowest);

    // The thumb is drawn where the value is: the far end for the highest int.
    slider.set_value(highest);
    ckv::scene::Surface surface = painted(slider);
    CK_CHECK(surface.at(ckv::Point{10, 0}).grapheme() == "●");
    slider.set_value(lowest);
    surface = painted(slider);
    CK_CHECK(surface.at(ckv::Point{0, 0}).grapheme() == "●");
}

CK_TEST(a_calendar_asks_for_the_rows_it_draws_and_says_when_that_changes) {
    // A48: the title, the weekday heading and six week rows with the title
    // on; one row fewer without it. A container is told when a setter
    // changes the measure, so the calendar is laid out again.
    struct Host final : ckv::ui::View {
        int notifications = 0;
        void on_child_size_hint_changed(ckv::ui::View&) override { ++notifications; }
    } host;
    auto* calendar = static_cast<CalendarView*>(host.add_child(std::make_unique<CalendarView>()));
    CK_CHECK(calendar->vertical_size_hint().min == 8);
    CK_CHECK(calendar->vertical_size_hint().preferred == 8);
    CK_CHECK(calendar->vertical_size_hint().max == 8);

    calendar->set_show_title(false);
    CK_CHECK(host.notifications == 1);
    CK_CHECK(calendar->vertical_size_hint().min == 7);
    CK_CHECK(calendar->vertical_size_hint().preferred == 7);
    CK_CHECK(calendar->vertical_size_hint().max == 7);
    // The last week row a month can need is the last row of those seven.
    Standalone s;
    attach_standalone(*calendar, s);
    calendar->set_month(DateValue{2026, 8, 1});  // 31 days from a Saturday: six week rows
    calendar->set_bounds(ckv::Rect{0, 0, 21, 7});
    const ckv::scene::Surface surface = painted(*calendar);
    CK_CHECK(two_cells(surface, ckv::Point{0, 6}) == "31");

    calendar->set_show_iso_week_numbers(true);  // wider: the container is told
    CK_CHECK(host.notifications == 2);
    calendar->set_show_iso_week_numbers(true);
    calendar->set_show_title(false);
    CK_CHECK(host.notifications == 2);
}

CK_TEST(a_calendar_is_at_least_as_wide_as_the_columns_it_draws) {
    // A50: seven day columns of three cells less the trailing gap are 20
    // cells; the "Wk" column adds three. The minimum is what is drawn, and
    // the preferred width keeps one blank after the last column.
    Standalone s;
    CalendarView calendar;
    attach_standalone(calendar, s);
    CK_CHECK(calendar.horizontal_size_hint().min == 20);
    CK_CHECK(calendar.horizontal_size_hint().preferred == 21);
    calendar.set_show_iso_week_numbers(true);
    CK_CHECK(calendar.horizontal_size_hint().min == 23);
    CK_CHECK(calendar.horizontal_size_hint().preferred == 24);

    // At its minimum width every column is on screen: Sunday 30 August
    // 2026 is in the last column, and its week number in the first.
    calendar.set_month(DateValue{2026, 8, 1});
    calendar.set_bounds(ckv::Rect{0, 0, calendar.horizontal_size_hint().min, 8});
    const ckv::scene::Surface surface = painted(calendar);
    CK_CHECK(two_cells(surface, ckv::Point{3 + 6 * 3, 6}) == "30");
    CK_CHECK(two_cells(surface, ckv::Point{0, 6}) == "35");
}

// --- Application-level interaction scripts (WP-38) -----------------------
//
// Each script mounts one component in a real Application on a headless
// terminal and drives it only as a reader would: keys and pointer events go
// through Application::dispatch, and step() renders what they changed. The
// "Interaction scripts" map in docs/coverage.md names these cases.

namespace {

struct Scripted {
    ckv::term::HeadlessTerminal term{ckv::Size{60, 16}};
    ManualClock clock;
    Application app{term, clock};

    Scripted() {
        const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    }

    // Adds `view` to the root at `bounds`, gives it the focus when it takes
    // any, and renders the first frame.
    template <class V>
    V* mount(std::unique_ptr<V> view, ckv::Rect bounds) {
        return mount_in(app.root(), std::move(view), bounds);
    }

    // The same, onto a Desktop covering the root. The bare root paints no
    // background of its own, so a script that watches a view go away mounts
    // it where something paints what it uncovers, as every application does.
    template <class V>
    V* mount_on_desktop(std::unique_ptr<V> view, ckv::Rect bounds) {
        auto* desktop = app.root().add(std::make_unique<Desktop>(app.root().bounds()));
        return mount_in(*desktop, std::move(view), bounds);
    }

    template <class V>
    V* mount_in(ckv::ui::View& parent, std::unique_ptr<V> view, ckv::Rect bounds) {
        V* mounted = parent.add(std::move(view));
        mounted->set_bounds(bounds);
        if (mounted->focus_policy() != ckv::ui::FocusPolicy::None) app.set_focus(mounted);
        app.step(0);
        return mounted;
    }

    bool press(Key k, std::string text = {}) {
        const bool handled = app.dispatch(ckv::KeyEvent{KeyChord{k, Modifier::None, std::move(text)}});
        app.step(0);
        return handled;
    }

    bool type(std::string_view text) {
        bool handled = true;
        for (const char c : text) handled = press(Key::Char, std::string(1, c)) && handled;
        return handled;
    }

    bool click(ckv::Point cell) {
        const bool handled = app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, cell,
                                                          std::nullopt, Modifier::None});
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.step(0);
        return handled;
    }

    std::string row(int y) const { return row_of(app.composed_surface(), y); }

    // The graphemes of `width` cells of row `y` from column `x`.
    std::string cells(int x, int y, int width) const {
        std::string out;
        for (int column = x; column < x + width; ++column)
            out += app.composed_surface().at(ckv::Point{column, y}).grapheme();
        return out;
    }
};

}  // namespace

CK_TEST(a_scripted_calendar_view_moves_by_keys_and_selects_the_clicked_day) {
    Scripted s;
    auto* calendar = s.mount(std::make_unique<CalendarView>(), ckv::Rect{0, 0, 21, 8});
    calendar->set_selected(DateValue{2026, 8, 9});
    std::vector<DateValue> chosen;
    calendar->on_select = [&](DateValue date) { chosen.push_back(date); };
    s.app.step(0);
    CK_CHECK(s.row(0).starts_with("August 2026"));

    CK_CHECK(s.press(Key::Right));
    CK_CHECK(s.press(Key::Down));
    CK_CHECK(calendar->selected() == (DateValue{2026, 8, 17}));
    // Thursday 20 August 2026: the fourth week row, the fourth column.
    CK_CHECK(s.click(ckv::Point{9, 5}));
    CK_CHECK(calendar->selected() == (DateValue{2026, 8, 20}));
    CK_CHECK(chosen.size() == 3U);
    // Up from the first week walks back into July, and the calendar follows.
    CK_CHECK(s.press(Key::Up));
    CK_CHECK(s.press(Key::Up));
    CK_CHECK(s.press(Key::Up));
    CK_CHECK(calendar->selected() == (DateValue{2026, 7, 30}));
    CK_CHECK(s.row(0).starts_with("July 2026"));
}

CK_TEST(a_scripted_spin_box_steps_by_keys_by_clicks_on_either_half_and_by_the_wheel) {
    Scripted s;
    auto* spin = s.mount(std::make_unique<SpinBox>(), ckv::Rect{2, 1, 8, 1});
    spin->set_range(0, 10);
    spin->set_step(3);
    spin->set_value(5);
    s.app.step(0);

    CK_CHECK(s.press(Key::Up));
    CK_CHECK(spin->value() == 8);
    CK_CHECK(s.press(Key::Right));
    CK_CHECK(spin->value() == 10);  // the last step is clamped at the end
    CK_CHECK(s.press(Key::Left));
    CK_CHECK(spin->value() == 7);
    CK_CHECK(s.click(ckv::Point{3, 1}));  // left half: down
    CK_CHECK(spin->value() == 4);
    CK_CHECK(s.click(ckv::Point{8, 1}));  // right half: up
    CK_CHECK(spin->value() == 7);
    CK_CHECK(s.row(1).find(" 7 ") != std::string::npos);
    // The wheel steps it too.
    const auto wheel = [&](ckv::MouseButton direction) {
        const bool handled = s.app.dispatch(
            ckv::MouseEvent{ckv::MouseAction::Wheel, direction, ckv::Point{4, 1}, std::nullopt, Modifier::None});
        s.app.step(0);
        return handled;
    };
    CK_CHECK(wheel(ckv::MouseButton::WheelUp));
    CK_CHECK(spin->value() == 10);
    CK_CHECK(wheel(ckv::MouseButton::WheelDown));
    CK_CHECK(wheel(ckv::MouseButton::WheelDown));
    CK_CHECK(spin->value() == 4);
    CK_CHECK(s.row(1).find(" 4 ") != std::string::npos);
}

CK_TEST(a_scripted_slider_follows_keys_and_a_proportional_click) {
    Scripted s;
    auto* slider = s.mount(std::make_unique<Slider>(), ckv::Rect{0, 2, 11, 1});
    slider->set_range(0, 100);
    slider->set_step(5);
    s.app.step(0);

    CK_CHECK(s.press(Key::Right));
    CK_CHECK(s.press(Key::Right));
    CK_CHECK(slider->value() == 10);
    CK_CHECK(s.press(Key::End));
    CK_CHECK(slider->value() == 100);
    CK_CHECK(s.press(Key::Home));
    CK_CHECK(slider->value() == 0);
    // Column 5 of eleven is the middle of the track.
    CK_CHECK(s.click(ckv::Point{5, 2}));
    CK_CHECK(slider->value() == 50);
    CK_CHECK(s.row(2).find("◆") != std::string::npos);  // focused thumb
}

CK_TEST(a_scripted_tool_bar_click_runs_its_command_only_while_it_is_enabled) {
    Scripted s;
    int saved = 0;
    bool can_save = true;
    const ckv::ui::CommandId save = s.app.commands().declare(
        ckv::ui::CommandDescriptor{.key = "test.save", .title = "&Save", .category = "test",
                                   .handler = [&] { ++saved; }});
    s.app.commands().set_enabled_predicate(save, [&] { return can_save; });
    auto toolbar = std::make_unique<ToolBar>();
    toolbar->set_items({CommandPresentation{save}});
    s.mount(std::move(toolbar), ckv::Rect{0, 0, 20, 1});
    CK_CHECK(s.row(0).starts_with("[Save]"));

    CK_CHECK(s.click(ckv::Point{2, 0}));
    CK_CHECK(saved == 1);
    can_save = false;
    s.click(ckv::Point{2, 0});
    CK_CHECK(saved == 1);
    // Past the button the click is not the tool bar's.
    can_save = true;
    CK_CHECK(!s.click(ckv::Point{12, 0}));
    CK_CHECK(saved == 1);
}

namespace {

// Declares a command the way an application does, counting its runs.
ckv::ui::CommandId declare_counted(Application& app, std::string key, std::string title, int& runs,
                                   std::string chord = {}) {
    return app.commands().declare(ckv::ui::CommandDescriptor{
        .key = std::move(key), .title = std::move(title), .category = "test", .chord = std::move(chord),
        .handler = [&runs] { ++runs; }});
}

}  // namespace

CK_TEST(a_scripted_tool_bar_walks_its_buttons_from_the_keyboard_and_runs_the_chosen_one) {
    Scripted s;
    int opened = 0;
    int saved = 0;
    int printed = 0;
    const auto open = declare_counted(s.app, "test.open", "&Open", opened);
    const auto save = declare_counted(s.app, "test.save", "&Save", saved);
    const auto print = declare_counted(s.app, "test.print", "&Print", printed);
    s.app.commands().set_enabled_predicate(print, [] { return false; });
    auto bar = std::make_unique<ToolBar>();
    bar->set_items({CommandPresentation{open}, CommandPresentation{save}, CommandPresentation{print}});
    auto* tools = s.mount(std::move(bar), ckv::Rect{0, 0, 40, 1});
    // A Tab stop, so mounting it gave it the keyboard, on the first button.
    CK_CHECK(s.app.focused() == tools);
    CK_CHECK(tools->focused_item() == std::optional<std::size_t>{0});
    CK_CHECK(s.row(0).starts_with("[Open] [Save] [Print]"));

    CK_CHECK(s.press(Key::Right));
    CK_CHECK(tools->focused_item() == std::optional<std::size_t>{1});
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(saved == 1);
    // Space presses it too, and the walk wraps at both ends.
    CK_CHECK(s.press(Key::Right));
    CK_CHECK(s.press(Key::Right));
    CK_CHECK(tools->focused_item() == std::optional<std::size_t>{0});
    CK_CHECK(s.press(Key::Char, " "));
    CK_CHECK(opened == 1);
    CK_CHECK(s.press(Key::Left));
    CK_CHECK(tools->focused_item() == std::optional<std::size_t>{2});
    // An unavailable button is reached, and refuses.
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(printed == 0);
    CK_CHECK(s.press(Key::Home));
    CK_CHECK(tools->focused_item() == std::optional<std::size_t>{0});
    CK_CHECK(s.press(Key::End));
    CK_CHECK(tools->focused_item() == std::optional<std::size_t>{2});
    // A mnemonic letter runs its button directly, alone or with Alt.
    CK_CHECK(s.press(Key::Char, "s"));
    CK_CHECK(saved == 2);
    CK_CHECK(s.app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "o"}}));
    CK_CHECK(opened == 2);
    // A bar focused by the Tab walk keeps the keyboard after running one.
    CK_CHECK(s.app.focused() == tools);
}

// The bar is chrome: activated from wherever the reader works, it judges and
// runs its commands for that place and hands the keyboard back.
CK_TEST(an_activated_docked_tool_bar_runs_its_command_for_the_view_it_came_from_and_hands_the_focus_back) {
    Scripted s;
    int saved = 0;
    const auto save = declare_counted(s.app, "test.save", "&Save", saved);
    s.app.commands().set_command_scope(save, ckv::ui::CommandScope{.contexts = {"document"}});
    auto* desktop = s.app.root().add(std::make_unique<Desktop>(s.app.root().bounds()));
    auto* tools = desktop->dock_top(std::make_unique<ToolBar>());
    tools->set_items({CommandPresentation{save}});
    auto* document = desktop->add(std::make_unique<SearchBox>());
    document->set_bounds(ckv::Rect{2, 4, 20, 1});
    document->set_command_context("document");
    s.app.set_focus(&document->field());
    s.app.step(0);

    tools->activate();
    s.app.step(0);
    CK_CHECK(s.app.focused() == tools);
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(saved == 1);
    CK_CHECK(s.app.focused() == &document->field());

    // Escape ends a walk with nothing run.
    tools->activate();
    CK_CHECK(s.press(Key::Escape));
    CK_CHECK(s.app.focused() == &document->field());
    CK_CHECK(saved == 1);
    // Without the document's context, the command is unavailable: the bar
    // shows and refuses it for where the focus really is.
    s.app.set_focus(nullptr);
    tools->activate();
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(saved == 1);
}

CK_TEST(a_scripted_click_on_a_docked_tool_bar_runs_its_command_without_taking_the_keyboard) {
    Scripted s;
    int saved = 0;
    const auto save = declare_counted(s.app, "test.save", "&Save", saved);
    s.app.commands().set_command_scope(save, ckv::ui::CommandScope{.contexts = {"document"}});
    auto* desktop = s.app.root().add(std::make_unique<Desktop>(s.app.root().bounds()));
    auto* tools = desktop->dock_top(std::make_unique<ToolBar>());
    tools->set_items({CommandPresentation{save}});
    auto* document = desktop->add(std::make_unique<SearchBox>());
    document->set_bounds(ckv::Rect{2, 4, 20, 1});
    document->set_command_context("document");
    s.app.set_focus(&document->field());
    s.app.step(0);

    CK_CHECK(s.click(ckv::Point{2, 0}));
    CK_CHECK(saved == 1);
    CK_CHECK(s.app.focused() == &document->field());
    // A press dragged off the button is taken back, and the keyboard still
    // goes home.
    s.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{2, 0}, std::nullopt,
                                   Modifier::None});
    s.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::Left, ckv::Point{30, 8},
                                   std::nullopt, Modifier::None});
    s.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{30, 8}, std::nullopt,
                                   Modifier::None});
    s.app.step(0);
    CK_CHECK(saved == 1);
    CK_CHECK(s.app.focused() == &document->field());
}

CK_TEST(a_scripted_narrow_tool_bar_puts_what_does_not_fit_in_its_overflow_menu) {
    Scripted s;
    int opened = 0;
    int saved = 0;
    int found = 0;
    const auto open = declare_counted(s.app, "test.open", "&Open", opened);
    const auto save = declare_counted(s.app, "test.save", "&Save", saved, "Ctrl+S");
    const auto find = declare_counted(s.app, "test.find", "&Find", found);
    auto bar = std::make_unique<ToolBar>();
    bar->set_items({CommandPresentation{open}, CommandPresentation{save}, CommandPresentation{find}});
    auto* tools = s.mount_on_desktop(std::move(bar), ckv::Rect{0, 0, 12, 1});
    // "[Open]", then the control at the right edge; Save and Find behind it.
    CK_CHECK((tools->shown_items() == std::vector<std::size_t>{0}));
    CK_CHECK((tools->overflow_items() == std::vector<std::size_t>{1, 2}));
    CK_CHECK(s.cells(0, 0, 12) == "[Open]   [»]");

    // The walk reaches the control; Enter opens the menu below the bar.
    CK_CHECK(s.press(Key::Right));
    CK_CHECK(!tools->focused_item().has_value());
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(s.row(2).find("Save") != std::string::npos);
    CK_CHECK(s.row(2).find("Ctrl+S") != std::string::npos);  // the menu states the chord
    CK_CHECK(s.row(3).find("Find") != std::string::npos);
    CK_CHECK(s.press(Key::Down));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(found == 1);
    CK_CHECK(s.app.focused() == tools);  // the menu hands the keyboard back

    // A click on the control opens it too, when the click completes.
    CK_CHECK(s.click(ckv::Point{10, 0}));
    CK_CHECK(s.row(2).find("Save") != std::string::npos);
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(saved == 1);
    // An overflowed button's mnemonic still runs it.
    CK_CHECK(s.press(Key::Char, "f"));
    CK_CHECK(found == 2);
    // Wide enough again, everything is back on the bar.
    tools->set_bounds(ckv::Rect{0, 0, 30, 1});
    s.app.step(0);
    CK_CHECK(tools->overflow_items().empty());
    CK_CHECK(s.row(0).starts_with("[Open] [Save] [Find]"));
}

// One CommandPresentation, one registry: the tool bar and a menu say the
// same thing about a command -- its wording, its chord, whether it can run
// and whether it is on.
CK_TEST(a_tool_bar_and_a_menu_share_one_commands_label_chord_enablement_and_checked_state) {
    Scripted s;
    int wraps = 0;
    bool wrapping = true;
    bool can_wrap = true;
    const auto wrap = declare_counted(s.app, "view.wrap", "Word &wrap", wraps, "Ctrl+W");
    s.app.commands().set_checked_predicate(wrap, [&] { return wrapping; });
    s.app.commands().set_enabled_predicate(wrap, [&] { return can_wrap; });
    const CommandPresentation presentation{wrap, "&Wrap"};
    auto bar = std::make_unique<ToolBar>();
    bar->set_items({presentation});
    bar->set_show_chords(true);
    auto* tools = s.mount_on_desktop(std::move(bar), ckv::Rect{0, 0, 30, 1});
    CK_CHECK(s.row(0).starts_with("[x Wrap Ctrl+W]"));
    wrapping = false;
    tools->invalidate();
    s.app.step(0);
    CK_CHECK(s.row(0).starts_with("[  Wrap Ctrl+W]"));

    auto* desktop = static_cast<Desktop*>(tools->parent());
    DropdownMenu* menu = show_context_menu({MenuItem::command(presentation)}, ckv::Point{0, 4}, s.app, *desktop);
    s.app.step(0);
    CK_CHECK(s.row(5).find("   Wrap") != std::string::npos);
    CK_CHECK(s.row(5).find("Ctrl+W") != std::string::npos);
    wrapping = true;
    menu->invalidate();
    s.app.step(0);
    CK_CHECK(s.row(5).find(" x Wrap") != std::string::npos);
    CK_CHECK(s.press(Key::Escape));

    // Unavailable, the bar draws it in the disabled role and a click does
    // nothing, exactly as the menu refuses it.
    can_wrap = false;
    tools->invalidate();
    s.app.step(0);
    const ckv::Style disabled = s.app.theme().resolve(s.app.roles().find("ckv.menu.dropdown.disabled"));
    CK_CHECK(s.app.composed_surface().at(ckv::Point{3, 0}).style().fg == disabled.fg);
    s.click(ckv::Point{3, 0});
    CK_CHECK(wraps == 0);
}

CK_TEST(a_tool_bar_docked_at_the_bottom_sits_above_the_status_line_and_drops_its_menu_upward) {
    Scripted s;
    int runs = 0;
    std::vector<CommandPresentation> items;
    for (const char* title : {"&Open file", "&Save file", "&Find text", "&Print page", "&Replace text", "&Tile windows"})
        items.emplace_back(declare_counted(s.app, std::string("test.") + (title + 1), title, runs));
    auto* desktop = s.app.root().add(std::make_unique<Desktop>(s.app.root().bounds()));
    desktop->dock_bottom(std::make_unique<StatusLine>());
    auto* tools = desktop->dock(std::make_unique<ToolBar>(), DockEdge::Bottom);
    tools->set_items(std::move(items));
    s.app.step(0);
    CK_CHECK((tools->bounds() == ckv::Rect{0, 14, 60, 1}));
    CK_CHECK((desktop->content_area() == ckv::Rect{0, 0, 60, 14}));
    CK_CHECK((tools->overflow_items() == std::vector<std::size_t>{4, 5}));

    tools->activate();
    CK_CHECK(s.press(Key::End));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(desktop->popups().size() == 1U);
    if (desktop->popups().empty()) return;
    // Two rows and the frame, ending on the row above the bar.
    const ckv::Rect menu = desktop->popups().front()->bounds();
    CK_CHECK(menu.y == 10);
    CK_CHECK(menu.bottom() == 14);
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(runs == 1);
}

CK_TEST(a_scripted_breadcrumb_bar_too_narrow_for_its_path_elides_its_middle_behind_an_ellipsis) {
    Scripted s;
    auto crumbs = std::make_unique<BreadcrumbBar>();
    crumbs->set_segments({"home", "ada", "projects", "ckvision", "docs"});
    std::vector<std::size_t> activated;
    crumbs->on_activate = [&](std::size_t index) { activated.push_back(index); };
    auto* bar = s.mount_on_desktop(std::move(crumbs), ckv::Rect{0, 3, 40, 1});
    CK_CHECK(s.row(3).starts_with("home/ada/projects/ckvision/docs"));
    CK_CHECK(bar->hidden_segments().empty());

    // Narrower, the first and last stay and the segments nearest the last
    // are kept as the room allows.
    bar->set_bounds(ckv::Rect{0, 3, 20, 1});
    s.app.step(0);
    CK_CHECK(s.cells(0, 3, 20) == "home/…/ckvision/docs");
    CK_CHECK((bar->hidden_segments() == std::vector<std::size_t>{1, 2}));
    bar->set_bounds(ckv::Rect{0, 3, 12, 1});
    s.app.step(0);
    CK_CHECK(s.cells(0, 3, 12) == "home/…/docs ");
    CK_CHECK((bar->hidden_segments() == std::vector<std::size_t>{1, 2, 3}));
    // Too narrow even for that: the kept segments are elided themselves.
    bar->set_bounds(ckv::Rect{0, 3, 8, 1});
    s.app.step(0);
    CK_CHECK(s.cells(0, 3, 8) == "h…/…/do…");

    // The ellipsis is a stop of its own.
    bar->set_bounds(ckv::Rect{0, 3, 20, 1});
    s.app.step(0);
    CK_CHECK(s.press(Key::Right));
    CK_CHECK(!bar->focused_segment().has_value());
    CK_CHECK(s.press(Key::Right));
    CK_CHECK(bar->focused_segment() == std::optional<std::size_t>{3});
    // Narrowing elides the focused segment: the ellipsis takes the focus.
    bar->set_bounds(ckv::Rect{0, 3, 12, 1});
    s.app.step(0);
    CK_CHECK(!bar->focused_segment().has_value());
    CK_CHECK(s.press(Key::Right));
    CK_CHECK(bar->focused_segment() == std::optional<std::size_t>{4});
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK((activated == std::vector<std::size_t>{4}));
}

CK_TEST(a_scripted_breadcrumb_ellipsis_lists_the_hidden_segments_and_activates_the_chosen_one) {
    Scripted s;
    auto crumbs = std::make_unique<BreadcrumbBar>();
    crumbs->set_segments({"home", "ada", "projects", "ckvision", "docs"});
    std::vector<std::size_t> activated;
    crumbs->on_activate = [&](std::size_t index) { activated.push_back(index); };
    auto* bar = s.mount_on_desktop(std::move(crumbs), ckv::Rect{0, 3, 20, 1});

    CK_CHECK(s.press(Key::Right));
    CK_CHECK(s.press(Key::Enter));
    // The segments the ellipsis stands for, in a menu hanging below it.
    CK_CHECK(s.row(5).find("ada") != std::string::npos);
    CK_CHECK(s.row(6).find("projects") != std::string::npos);
    CK_CHECK(s.press(Key::Down));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK((activated == std::vector<std::size_t>{2}));
    CK_CHECK(s.app.focused() == bar);

    // A click on the ellipsis opens the list when the click completes.
    CK_CHECK(s.click(ckv::Point{5, 3}));
    CK_CHECK(s.row(5).find("ada") != std::string::npos);
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK((activated == std::vector<std::size_t>{2, 1}));
    // The first and last segments are clicked as before.
    CK_CHECK(s.click(ckv::Point{1, 3}));
    CK_CHECK(s.click(ckv::Point{18, 3}));
    CK_CHECK((activated == std::vector<std::size_t>{2, 1, 0, 4}));
}

CK_TEST(a_scripted_search_box_shows_the_hosts_status_between_the_query_and_the_clear_control) {
    Scripted s;
    auto box = std::make_unique<SearchBox>();
    std::vector<std::string> names{"invoice 1", "invoice 2", "receipt"};
    SearchBox* search = box.get();
    box->on_change = [&](const std::string& query) {
        const auto matches = std::count_if(names.begin(), names.end(), [&](const std::string& name) {
            return name.find(query) != std::string::npos;
        });
        search->set_status(query.empty() ? std::string{} : std::to_string(matches) + " of 3");
    };
    s.mount(std::move(box), ckv::Rect{0, 1, 30, 1});
    s.app.set_focus(&search->field());
    CK_CHECK(s.type("inv"));
    CK_CHECK(search->status() == "2 of 3");
    // Right-aligned against the "[x]", one blank before it.
    CK_CHECK(s.cells(0, 1, 30) == "Search inv" + std::string(11, ' ') + "2 of 3[x]");
    // The caret stays at the end of the query, not after the status.
    CK_CHECK(reversed(s.app.composed_surface(), ckv::Point{10, 1}));
    CK_CHECK(!reversed(s.app.composed_surface(), ckv::Point{20, 1}));
    CK_CHECK(s.type("x"));
    CK_CHECK(s.cells(20, 1, 10) == " 0 of 3[x]");
    // The clear control still clears, and the host takes the status away.
    CK_CHECK(s.click(ckv::Point{28, 1}));
    CK_CHECK(search->query().empty());
    CK_CHECK(search->status().empty());
    CK_CHECK(s.cells(0, 1, 30) == "Search " + std::string(23, ' '));

    // Squeezed, the status is elided before the field gives up its minimum,
    // and dropped once there is no room for a cell of it.
    search->set_query("inv");
    search->set_bounds(ckv::Rect{0, 1, 20, 1});
    s.app.step(0);
    CK_CHECK(s.cells(0, 1, 20) == "Search inv    2 …[x]");
    search->set_bounds(ckv::Rect{0, 1, 17, 1});
    s.app.step(0);
    CK_CHECK(s.cells(0, 1, 17) == "Search inv    [x]");
}

CK_TEST(a_scripted_search_box_edits_its_query_as_an_input_line_does) {
    // The query field is an InputLine (the widget catalog: "InputLine plus
    // clear/status affordance"): the caret moves, Shift extends a selection
    // that typing replaces, undo works, and history recall reports on_change
    // -- while Escape and Enter keep the meaning the box gives them.
    Scripted s;
    auto* search = s.mount(std::make_unique<SearchBox>(), ckv::Rect{0, 1, 30, 1});
    search->set_history_key("find");
    std::vector<std::string> seen;
    search->on_change = [&seen](const std::string& query) { seen.push_back(query); };
    const auto chord = [&s](Key k, Modifier modifiers, std::string text = {}) {
        const bool handled = s.app.dispatch(ckv::KeyEvent{KeyChord{k, modifiers, std::move(text)}});
        s.app.step(0);
        return handled;
    };

    // A press on the prompt puts the keyboard in the field.
    CK_CHECK(s.click(ckv::Point{1, 1}));
    CK_CHECK(s.app.focused() == &search->field());
    CK_CHECK(s.type("invoce"));
    // The caret walks back into the word, and typing lands there.
    CK_CHECK(s.press(Key::Left));
    CK_CHECK(s.press(Key::Left));
    CK_CHECK(s.type("i"));
    CK_CHECK(search->query() == "invoice");
    CK_CHECK(seen.back() == "invoice");
    // "Search " is seven columns: the caret stands on the "c" after "invoi".
    CK_CHECK(s.cells(12, 1, 1) == "c");
    CK_CHECK(reversed(s.app.composed_surface(), ckv::Point{12, 1}));

    // Home, then Shift+End, selects the query; a paste replaces the
    // selection, as typing would.
    CK_CHECK(s.press(Key::Home));
    CK_CHECK(chord(Key::End, Modifier::Shift));
    CK_CHECK(search->field().has_selection());
    s.app.set_clipboard_text("receipt");
    CK_CHECK(chord(Key::Char, Modifier::Ctrl, "v"));
    CK_CHECK(search->query() == "receipt");
    CK_CHECK(seen.back() == "receipt");
    // Undo puts the replaced query back, and reports it.
    CK_CHECK(chord(Key::Char, Modifier::Ctrl, "z"));
    CK_CHECK(search->query() == "invoice");
    CK_CHECK(seen.back() == "invoice");

    // Enter records the query and is left for whatever else answers it.
    CK_CHECK(!s.press(Key::Enter));
    CK_CHECK((s.app.history().entries("find") == std::vector<std::string>{"invoice"}));
    // Escape clears the query, and with nothing left to clear goes on.
    CK_CHECK(s.press(Key::Escape));
    CK_CHECK(search->query().empty());
    CK_CHECK(seen.back().empty());
    CK_CHECK(!s.press(Key::Escape));
    // Up recalls the recorded query, as typing would report it; Down returns.
    CK_CHECK(s.press(Key::Up));
    CK_CHECK(search->query() == "invoice");
    CK_CHECK(seen.back() == "invoice");
    CK_CHECK(s.cells(0, 1, 30) == "Search invoice" + std::string(13, ' ') + "[x]");
    CK_CHECK(s.press(Key::Down));
    CK_CHECK(search->query().empty());
}

CK_TEST(a_scripted_command_palette_filters_as_typed_and_runs_the_highlighted_command) {
    Scripted s;
    int opened = 0;
    int ordered = 0;
    s.app.commands().declare(ckv::ui::CommandDescriptor{.key = "test.open", .title = "&Open file",
                                                        .category = "test", .handler = [&] { ++opened; }});
    s.app.commands().declare(ckv::ui::CommandDescriptor{.key = "test.order", .title = "Order &lines",
                                                        .category = "test", .handler = [&] { ++ordered; }});
    auto* palette = s.mount(std::make_unique<CommandPalette>(), ckv::Rect{0, 0, 40, 8});

    CK_CHECK(s.type("o"));
    CK_CHECK(palette->query() == "o");
    CK_CHECK(palette->filtered_commands().size() == 2U);
    CK_CHECK(s.type("rd"));
    CK_CHECK(palette->filtered_commands().size() == 1U);
    CK_CHECK(s.row(2).find("ord") != std::string::npos);
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(ordered == 1);
    CK_CHECK(opened == 0);

    // Backspace widens the match again; Down moves the highlight to the
    // second result before Enter runs it.
    CK_CHECK(s.press(Key::Backspace));
    CK_CHECK(s.press(Key::Backspace));
    CK_CHECK(s.press(Key::Down));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(ordered == 2);
}

CK_TEST(a_scripted_breadcrumb_bar_walks_and_activates_its_segments) {
    Scripted s;
    auto breadcrumb = std::make_unique<BreadcrumbBar>();
    breadcrumb->set_segments({"home", "ada", "notes"});
    std::vector<std::size_t> activated;
    breadcrumb->on_activate = [&](std::size_t index) { activated.push_back(index); };
    s.mount(std::move(breadcrumb), ckv::Rect{0, 3, 30, 1});
    CK_CHECK(s.row(3).starts_with("home/ada/notes"));

    CK_CHECK(s.press(Key::Right));
    CK_CHECK(s.press(Key::Right));
    CK_CHECK(!s.press(Key::Right));  // already on the last segment
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(s.press(Key::Left));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(s.click(ckv::Point{1, 3}));
    CK_CHECK((activated == std::vector<std::size_t>{2, 1, 0}));
}

CK_TEST(a_scripted_property_inspector_edits_a_value_in_place) {
    Scripted s;
    auto inspector = std::make_unique<PropertyInspector>();
    inspector->set_items({{"Name", "Ada", true}, {"Id", "7", false}, {"Role", "", true}});
    std::vector<std::pair<std::size_t, std::string>> changes;
    inspector->on_change = [&](std::size_t index, std::string value) { changes.emplace_back(index, value); };
    auto* mounted = s.mount(std::move(inspector), ckv::Rect{0, 0, 30, 4});
    // Two columns: the names as wide as the widest, the values aligned.
    CK_CHECK(s.row(0).starts_with("Name  Ada"));
    CK_CHECK(s.row(1).starts_with("Id    7"));

    CK_CHECK(s.press(Key::Down));
    CK_CHECK(mounted->cursor() == 1);
    s.press(Key::Enter);  // read-only: no edit opens
    CK_CHECK(!mounted->editing());
    CK_CHECK(s.press(Key::Down));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(mounted->editing());
    CK_CHECK(s.type("dev"));
    CK_CHECK(s.press(Key::Backspace));
    CK_CHECK(s.row(2).starts_with("Role  de"));
    // Typing is not committing: the row changes when the edit is.
    CK_CHECK(mounted->items()[2].value.empty());
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(!mounted->editing());
    CK_CHECK(mounted->items()[2].value == "de");
    CK_CHECK(mounted->items()[1].value == "7");
    CK_CHECK((changes == std::vector<std::pair<std::size_t, std::string>>{{2U, "de"}}));
    CK_CHECK(s.row(2).starts_with("Role  de"));
    // With the edit closed, typing no longer reaches the value.
    s.type("x");
    CK_CHECK(mounted->items()[2].value == "de");
}

CK_TEST(a_scripted_wizard_goes_forward_back_and_finishes_only_when_allowed) {
    Scripted s;
    bool accepted = false;
    std::vector<WizardOutcome> outcomes;
    auto wizard = std::make_unique<Wizard>();
    wizard->set_pages({{"Welcome", {}}, {"Licence", [&] { return accepted; }}, {"Done", {}}});
    wizard->on_complete = [&](WizardOutcome outcome) { outcomes.push_back(outcome); };
    auto* mounted = s.mount(std::move(wizard), ckv::Rect{0, 0, 40, 6});
    CK_CHECK(s.row(0).find("Welcome") != std::string::npos);

    CK_CHECK(s.press(Key::Right));
    CK_CHECK(mounted->current_page() == 1U);
    s.press(Key::Enter);  // the licence page refuses until accepted
    CK_CHECK(mounted->current_page() == 1U);
    accepted = true;
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(mounted->current_page() == 2U);
    CK_CHECK(s.row(5).find("Finish") != std::string::npos);
    CK_CHECK(s.press(Key::Left));
    CK_CHECK(s.press(Key::Right));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(s.press(Key::Escape));
    CK_CHECK((outcomes == std::vector<WizardOutcome>{WizardOutcome::Finished, WizardOutcome::Cancelled}));
}

CK_TEST(a_scripted_notification_center_expires_toasts_and_dismisses_by_escape_and_click) {
    Scripted s;
    auto* centre = s.mount_on_desktop(std::make_unique<NotificationCenter>(), ckv::Rect{0, 0, 40, 3});
    centre->set_auto_dismiss(kSecond * 2);
    centre->add(info("Saved"));
    centre->add(info("Disk almost full", true));
    centre->add(info("Synced"));
    s.app.step(0);
    CK_CHECK(s.row(0).find("Saved") != std::string::npos);

    // Only time, delivered by the loop, takes the two toasts away; the
    // persistent line stays.
    s.clock.advance(kSecond * 3);
    s.app.step(s.clock.now_nanos());
    CK_CHECK(centre->notifications().size() == 1U);
    CK_CHECK(s.row(0).find("Disk almost full") != std::string::npos);
    CK_CHECK(s.row(1).find("Synced") == std::string::npos);

    centre->add(info("Again", true));
    s.app.step(s.clock.now_nanos());
    CK_CHECK(s.press(Key::Escape));  // the newest goes first
    CK_CHECK(centre->notifications().size() == 1U);
    CK_CHECK(s.click(ckv::Point{4, 0}));
    CK_CHECK(centre->notifications().empty());
    CK_CHECK(s.row(0).find("Disk") == std::string::npos);
}

CK_TEST(a_scripted_tooltip_appears_and_leaves_with_the_frames_its_host_steps) {
    Scripted s;
    auto* tooltip = s.mount_on_desktop(std::make_unique<Tooltip>("Saves the file"), ckv::Rect{});
    CK_CHECK(s.row(4).find("Saves the file") == std::string::npos);

    tooltip->show_at(ckv::Point{6, 4});
    s.app.step(0);
    CK_CHECK(s.cells(6, 4, 16) == " Saves the file ");
    // It takes no input: a key goes past it to nothing that wants it.
    CK_CHECK(!s.press(Key::Char, "x"));
    CK_CHECK(tooltip->shown());

    tooltip->hide();
    s.app.step(0);
    CK_CHECK(s.row(4).find("Saves the file") == std::string::npos);
}

CK_TEST(a_scripted_time_picker_steps_its_fields_and_shows_either_clock_face) {
    Scripted s;
    auto* picker = s.mount(std::make_unique<TimePicker>(), ckv::Rect{0, 2, 14, 1});
    picker->set_value(TimeValue{13, 5, 0});
    s.app.step(0);
    CK_CHECK(s.row(2).find("13:05:00") != std::string::npos);

    CK_CHECK(s.press(Key::Up));  // the hour field
    CK_CHECK(s.press(Key::Right));
    CK_CHECK(s.press(Key::Down));  // then the minutes
    CK_CHECK(picker->value() == (TimeValue{14, 4, 0}));
    CK_CHECK(s.row(2).find("14:04:00") != std::string::npos);

    // The twelve-hour face shows the same value on the other scale.
    picker->set_24_hour(false);
    s.app.step(0);
    CK_CHECK(s.row(2).find("02:04:00 PM") != std::string::npos);
    CK_CHECK(s.press(Key::Down));
    CK_CHECK(picker->value() == (TimeValue{14, 3, 0}));
}

// --- Spin box: typed entry and validation feedback --------------------------

namespace {

// The style the classic scheme gives `role` in the scripted application.
ckv::Style scripted_style(const Scripted& s, ckv::ui::RoleId role) { return s.app.theme().resolve(role); }

ckv::ui::StandardRoles scripted_roles(Scripted& s) { return ckv::ui::intern_standard_roles(s.app.roles()); }

}  // namespace

CK_TEST(a_scripted_editable_spin_box_takes_a_typed_number_and_refuses_what_it_cannot_take) {
    Scripted s;
    auto* spin = s.mount(std::make_unique<SpinBox>(), ckv::Rect{2, 1, 12, 1});
    spin->set_range(0, 10);
    spin->set_value(3);
    spin->set_editable(true);
    std::vector<int> changes;
    std::vector<std::string> reasons;
    spin->on_change = [&](int value) { changes.push_back(value); };
    spin->on_invalid = [&](const std::string& reason) { reasons.push_back(reason); };
    s.app.step(0);
    const ckv::ui::StandardRoles roles = scripted_roles(s);

    // A typed number is drawn in place of the value, with the caret after it,
    // and becomes the value on Enter.
    CK_CHECK(s.type("7"));
    CK_CHECK(spin->editing());
    CK_CHECK(spin->entry() == "7");
    CK_CHECK(s.row(1).find("< 7 >") != std::string::npos);
    CK_CHECK(s.app.current_cursor().visible);
    CK_CHECK(s.app.current_cursor().position == (ckv::Point{5, 1}));
    CK_CHECK(spin->value() == 3);  // nothing is taken while it is typed
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(!spin->editing());
    CK_CHECK(spin->value() == 7);
    CK_CHECK((changes == std::vector<int>{7}));

    // Out of range: refused, never clamped, and marked with the reason.
    CK_CHECK(s.type("12"));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(spin->editing());
    CK_CHECK(spin->entry() == "12");
    CK_CHECK(spin->value() == 7);
    CK_CHECK(!spin->valid());
    CK_CHECK(spin->validation_message() == "Enter a number from 0 to 10.");
    CK_CHECK((reasons == std::vector<std::string>{"Enter a number from 0 to 10."}));
    CK_CHECK(s.app.composed_surface().at(ckv::Point{4, 1}).style() == scripted_style(s, roles.input_invalid));
    CK_CHECK((changes == std::vector<int>{7}));
    // Editing the entry takes the mark away until the next commit.
    CK_CHECK(s.press(Key::Backspace));
    CK_CHECK(spin->valid());
    CK_CHECK(spin->entry() == "1");
    CK_CHECK(s.app.composed_surface().at(ckv::Point{4, 1}).style() == scripted_style(s, roles.input_focused));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(spin->value() == 1);

    // Not a number at all.
    CK_CHECK(s.type("4x"));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(spin->validation_message() == "Not a whole number.");
    // Escape abandons it: the value shows again, unmarked.
    CK_CHECK(s.press(Key::Escape));
    CK_CHECK(!spin->editing());
    CK_CHECK(spin->valid());
    CK_CHECK(spin->value() == 1);
    CK_CHECK(s.row(1).find("< 1 >") != std::string::npos);
    CK_CHECK(!s.app.current_cursor().visible);
}

CK_TEST(a_spin_box_entry_is_committed_by_the_arrows_by_a_click_and_by_leaving_it) {
    Scripted s;
    auto* spin = s.mount(std::make_unique<SpinBox>(), ckv::Rect{2, 1, 12, 1});
    auto* other = s.mount_in(s.app.root(), std::make_unique<SpinBox>(), ckv::Rect{2, 3, 12, 1});
    s.app.set_focus(spin);
    spin->set_range(-20, 20);
    spin->set_editable(true);
    s.app.step(0);

    // An arrow commits first, then steps from what it committed.
    CK_CHECK(s.type("-5"));
    CK_CHECK(s.press(Key::Up));
    CK_CHECK(spin->value() == -4);
    // A sign and spaces are fine; "+-" is not a number.
    CK_CHECK(s.type(" +9 "));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(spin->value() == 9);
    CK_CHECK(s.type("+-3"));
    CK_CHECK(s.press(Key::Down));
    CK_CHECK(spin->value() == 9);  // refused, so the arrow does nothing more
    CK_CHECK(!spin->valid());
    // Leaving a refused entry keeps it, marked, for the reader to come back to.
    s.app.set_focus(other);
    s.app.step(0);
    CK_CHECK(spin->editing());
    CK_CHECK(!spin->valid());
    CK_CHECK(s.row(1).find("+-3") != std::string::npos);
    // Leaving a good one commits it.
    s.app.set_focus(spin);
    CK_CHECK(s.press(Key::Backspace));
    CK_CHECK(s.press(Key::Backspace));
    CK_CHECK(s.type("3"));  // "+3"
    s.app.set_focus(other);
    s.app.step(0);
    CK_CHECK(!spin->editing());
    CK_CHECK(spin->value() == 3);
    // A click commits too, then steps; with no entry, Backspace opens one
    // from the value less its last digit.
    s.app.set_focus(spin);
    CK_CHECK(s.type("11"));
    CK_CHECK(s.click(ckv::Point{12, 1}));  // the right half
    CK_CHECK(spin->value() == 12);
    CK_CHECK(s.press(Key::Backspace));
    CK_CHECK(spin->entry() == "1");
    // A number past the int range is out of range, not garbage.
    CK_CHECK(s.type("9999999999999"));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(spin->validation_message() == "Enter a number from -20 to 20.");
}

CK_TEST(a_spin_box_that_is_not_editable_leaves_typing_alone_and_an_owner_value_abandons_an_entry) {
    Scripted s;
    auto* spin = s.mount(std::make_unique<SpinBox>(), ckv::Rect{2, 1, 12, 1});
    CK_CHECK(!spin->editable());
    CK_CHECK(!s.type("5"));
    CK_CHECK(!spin->editing());
    CK_CHECK(!s.press(Key::Enter));

    spin->set_editable(true);
    spin->set_refusal_text([](SpinBoxRefusal refusal, int minimum, int maximum) {
        return refusal == SpinBoxRefusal::NotANumber
                   ? std::string{"Keine Zahl."}
                   : "Zwischen " + std::to_string(minimum) + " und " + std::to_string(maximum) + ".";
    });
    CK_CHECK(s.type("x"));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(spin->validation_message() == "Keine Zahl.");
    CK_CHECK(s.press(Key::Escape));
    CK_CHECK(s.type("500"));
    CK_CHECK(!spin->commit_entry());
    CK_CHECK(spin->validation_message() == "Zwischen 0 und 100.");
    // The owner's value wins over what the reader was typing.
    spin->set_value(42);
    CK_CHECK(!spin->editing());
    CK_CHECK(spin->valid());
    CK_CHECK(spin->value() == 42);
    // A range change keeps an entry, and judges it against the new range.
    CK_CHECK(s.type("150"));
    spin->set_range(0, 200);
    CK_CHECK(spin->editing());
    CK_CHECK(spin->commit_entry());
    CK_CHECK(spin->value() == 150);
    // Turning editing off abandons an entry.
    CK_CHECK(s.type("7"));
    spin->set_editable(false);
    CK_CHECK(!spin->editing());
    CK_CHECK(spin->value() == 150);
}

// --- Slider: tick labels ----------------------------------------------------

CK_TEST(a_scripted_slider_with_ticks_marks_the_track_and_labels_them_below) {
    Scripted s;
    auto* slider = s.mount(std::make_unique<Slider>(), ckv::Rect{0, 2, 21, 2});
    slider->set_range(0, 100);
    CK_CHECK(slider->vertical_size_hint().preferred == 1);
    slider->set_ticks({{0, "0"}, {100, "100"}, {50, "50"}});
    CK_CHECK(slider->vertical_size_hint().min == 2);
    CK_CHECK(slider->vertical_size_hint().preferred == 2);
    s.app.step(0);
    // Marks on the track at 0, 50 and 100; the thumb stands over the first.
    CK_CHECK(s.cells(10, 2, 1) == "┬");
    CK_CHECK(s.cells(20, 2, 1) == "┬");
    CK_CHECK(s.cells(0, 2, 1) == "◆");
    // Each label centred on its mark and kept inside the slider.
    CK_CHECK(s.cells(0, 3, 21) == "0        50       100");
    // On the filled part the mark is drawn in the fill's weight.
    CK_CHECK(s.press(Key::End));
    CK_CHECK(s.cells(10, 2, 1) == "┯");
    // A click on the label row sets the value as one on the track does.
    CK_CHECK(s.click(ckv::Point{5, 3}));
    CK_CHECK(slider->value() == 25);
    slider->set_ticks({});
    CK_CHECK(slider->vertical_size_hint().preferred == 1);
}

CK_TEST(slider_tick_labels_that_would_collide_are_left_out_in_the_order_given) {
    Scripted s;
    auto* slider = s.mount(std::make_unique<Slider>(), ckv::Rect{0, 0, 11, 2});
    slider->set_range(0, 100);
    // "Low" at the left edge and "Mid" five units on land on the same
    // column: the one listed first wins, and "High" keeps its blank to it.
    slider->set_ticks({{0, "Low"}, {5, "Mid"}, {100, "High"}});
    s.app.step(0);
    CK_CHECK(s.cells(0, 1, 11) == "Low    High");
    slider->set_ticks({{5, "Mid"}, {0, "Low"}, {100, "High"}});
    s.app.step(0);
    CK_CHECK(s.cells(0, 1, 11) == "Mid    High");
    // Two labels that would touch keep at least one blank between them.
    slider->set_ticks({{0, "Left"}, {50, "Mid"}, {100, "Right"}});
    s.app.step(0);
    CK_CHECK(s.cells(0, 1, 11) == "Left  Right");
    // A label wider than the slider is elided to fit it; one outside the
    // range is not drawn at all, mark or label.
    slider->set_ticks({{50, "A very long label"}, {200, "Far"}});
    s.app.step(0);
    CK_CHECK(s.cells(0, 1, 11) == "A very lon…");
    CK_CHECK(s.row(0).find("Far") == std::string::npos);
}

// --- Date picker: formatting and parsing policy, typed entry ----------------

CK_TEST(date_formats_write_and_read_dates_in_the_order_separator_and_month_style_they_state) {
    const DateValue day{2026, 8, 9};
    // The default is the ISO form, exactly.
    CK_CHECK(format_date(day, DateFormat{}) == format_iso_date(day));
    CK_CHECK((parse_date("2026-08-09", DateFormat{}) == std::optional<DateValue>{day}));
    CK_CHECK(!parse_date("2026-8-9", DateFormat{}));
    CK_CHECK(!parse_date("2026-02-30", DateFormat{}));

    DateFormat dotted;
    dotted.order = {DateField::Day, DateField::Month, DateField::Year};
    dotted.separator = ".";
    CK_CHECK(format_date(day, dotted) == "09.08.2026");
    CK_CHECK((parse_date(" 09.08.2026 ", dotted) == std::optional<DateValue>{day}));
    CK_CHECK(!parse_date("9.8.2026", dotted));

    DateFormat american;
    american.order = {DateField::Month, DateField::Day, DateField::Year};
    american.separator = "/";
    american.zero_pad = false;
    CK_CHECK(format_date(day, american) == "8/9/2026");
    CK_CHECK((parse_date("08/9/2026", american) == std::optional<DateValue>{day}));
    CK_CHECK(!parse_date("8/9/26", american));  // a year is written in its four digits

    DateFormat named;
    named.order = {DateField::Day, DateField::Month, DateField::Year};
    named.separator = " ";
    named.month_style = MonthStyle::Name;
    named.zero_pad = false;
    DateTimeLabels labels;
    labels.month_names = {"Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"};
    CK_CHECK(format_date(DateValue{2026, 3, 1}, named, labels) == "1 Mär 2026");
    CK_CHECK((parse_date("1 mär 2026", named, labels) == std::optional<DateValue>{DateValue{2026, 3, 1}}));
    CK_CHECK(format_date(day, named) == "9 August 2026");  // the English names by default
    CK_CHECK(!parse_date("9 Augu 2026", named));

    // A caller's own policy answers for itself; a date it reads that is not
    // one is still refused.
    DateFormat own;
    own.format = [](DateValue date) { return "day " + std::to_string(date.day); };
    own.parse = [](std::string_view text) -> std::optional<DateValue> {
        if (text == "leap") return DateValue{2023, 2, 29};
        if (text == "today") return DateValue{2026, 8, 9};
        return std::nullopt;
    };
    CK_CHECK(format_date(day, own) == "day 9");
    CK_CHECK((parse_date("today", own) == std::optional<DateValue>{day}));
    CK_CHECK(!parse_date("leap", own));
    CK_CHECK(!parse_date("2026-08-09", own));
}

CK_TEST(a_scripted_date_picker_edits_the_segments_its_format_lays_out) {
    Scripted s;
    auto* picker = s.mount(std::make_unique<DatePicker>(), ckv::Rect{0, 1, 16, 1});
    DateFormat dotted;
    dotted.order = {DateField::Day, DateField::Month, DateField::Year};
    dotted.separator = ".";
    picker->set_format(dotted);
    picker->set_value(DateValue{2026, 8, 9});
    s.app.step(0);
    CK_CHECK(s.row(1).starts_with("09.08.2026"));
    // The year is the active segment until the reader picks another; Left
    // walks back through the order the format writes.
    const auto marked = [&](int x) {
        return ckv::has_attr(s.app.composed_surface().at(ckv::Point{x, 1}).style().attrs, ckv::Attr::Reverse);
    };
    CK_CHECK(marked(6) && marked(9) && !marked(0));
    CK_CHECK(s.press(Key::Left));  // the month
    CK_CHECK(s.press(Key::Up));
    CK_CHECK((picker->value() == std::optional<DateValue>{DateValue{2026, 9, 9}}));
    CK_CHECK(s.press(Key::Left));  // the day
    CK_CHECK(s.press(Key::Up));
    CK_CHECK((picker->value() == std::optional<DateValue>{DateValue{2026, 9, 10}}));
    CK_CHECK(s.row(1).starts_with("10.09.2026"));
    // A click chooses the segment under the pointer.
    CK_CHECK(s.click(ckv::Point{8, 1}));
    CK_CHECK(s.press(Key::Down));
    CK_CHECK((picker->value() == std::optional<DateValue>{DateValue{2025, 9, 10}}));
}

CK_TEST(a_date_picker_shows_its_callers_text_away_from_the_keyboard_and_its_segments_with_it) {
    Scripted s;
    auto* picker = s.mount(std::make_unique<DatePicker>(), ckv::Rect{0, 1, 16, 1});
    auto* elsewhere = s.mount_in(s.app.root(), std::make_unique<SpinBox>(), ckv::Rect{0, 3, 8, 1});
    DateFormat own;
    own.format = [](DateValue date) { return "Day " + std::to_string(date.day) + " of month " + std::to_string(date.month); };
    picker->set_format(own);
    picker->set_value(DateValue{2026, 8, 9});
    s.app.set_focus(elsewhere);
    s.app.step(0);
    CK_CHECK(s.row(1).starts_with("Day 9 of month 8"));
    s.app.set_focus(picker);
    s.app.step(0);
    CK_CHECK(s.row(1).starts_with("2026-08-09"));
}

CK_TEST(a_scripted_date_picker_takes_a_typed_date_and_refuses_one_its_format_cannot_read) {
    Scripted s;
    auto* picker = s.mount(std::make_unique<DatePicker>(), ckv::Rect{0, 1, 16, 1});
    DateFormat dotted;
    dotted.order = {DateField::Day, DateField::Month, DateField::Year};
    dotted.separator = ".";
    picker->set_format(dotted);
    DateTimeLabels labels;
    labels.not_a_date = "Kein Datum.";
    picker->set_labels(labels);
    picker->set_value(DateValue{2026, 8, 9});
    std::vector<std::string> reasons;
    picker->on_invalid = [&](const std::string& reason) { reasons.push_back(reason); };
    s.app.step(0);
    const ckv::ui::StandardRoles roles = scripted_roles(s);

    CK_CHECK(s.type("30.02.2026"));
    CK_CHECK(picker->editing());
    CK_CHECK(s.row(1).starts_with("30.02.2026"));
    CK_CHECK(s.app.current_cursor().visible && s.app.current_cursor().position == (ckv::Point{10, 1}));
    CK_CHECK(s.press(Key::Enter));
    // No such day: refused as typed, the value untouched, the reason given.
    CK_CHECK(picker->editing());
    CK_CHECK((picker->value() == std::optional<DateValue>{DateValue{2026, 8, 9}}));
    CK_CHECK(!picker->valid());
    CK_CHECK(picker->validation_message() == "Kein Datum.");
    CK_CHECK((reasons == std::vector<std::string>{"Kein Datum."}));
    CK_CHECK(s.app.composed_surface().at(ckv::Point{0, 1}).style() == scripted_style(s, roles.input_invalid));
    // An arrow commits first; refused, it does nothing more.
    CK_CHECK(s.press(Key::Up));
    CK_CHECK((picker->value() == std::optional<DateValue>{DateValue{2026, 8, 9}}));
    // Corrected, it is taken.
    for (int i = 0; i < 10; ++i) s.press(Key::Backspace);
    CK_CHECK(picker->valid());
    CK_CHECK(s.type("28.02.2026"));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(!picker->editing());
    CK_CHECK((picker->value() == std::optional<DateValue>{DateValue{2026, 2, 28}}));
    // Escape abandons an entry; an empty one empties an optional date.
    CK_CHECK(s.type("1"));
    CK_CHECK(s.press(Key::Escape));
    CK_CHECK((picker->value() == std::optional<DateValue>{DateValue{2026, 2, 28}}));
    CK_CHECK(s.type("1"));
    CK_CHECK(s.press(Key::Backspace));
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(!picker->value());
    CK_CHECK(s.row(1).starts_with("— no date —"));
    // Required, an empty entry is refused instead.
    picker->set_empty_allowed(false);
    CK_CHECK(s.type("x"));
    CK_CHECK(s.press(Key::Backspace));
    CK_CHECK(!picker->commit_entry());
    CK_CHECK(picker->validation_message() == "Kein Datum.");
    // The owner's value replaces the entry.
    picker->set_value(DateValue{2026, 1, 1});
    CK_CHECK(!picker->editing());
    CK_CHECK(picker->valid());
}

CK_TEST(a_date_pickers_calendar_and_empty_text_speak_its_label_table) {
    Scripted s;
    auto* desktop = s.app.root().add(std::make_unique<Desktop>(s.app.root().bounds()));
    auto* picker = s.mount_in(*desktop, std::make_unique<DatePicker>(), ckv::Rect{2, 1, 16, 1});
    picker->set_calendar_host(s.app, *desktop);
    DateTimeLabels labels;
    labels.month_names = {"Januar", "Februar", "März",      "April",   "Mai",      "Juni",
                          "Juli",   "August",  "September", "Oktober", "November", "Dezember"};
    labels.weekday_names = {"Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};
    labels.no_date = "— kein Datum —";
    picker->set_labels(labels);
    s.app.step(0);
    CK_CHECK(s.row(1).find("— kein Datum —") != std::string::npos);
    picker->set_value(DateValue{2026, 3, 4});
    CK_CHECK(picker->open_calendar());
    s.app.step(0);
    std::string screen;
    for (int y = 0; y < 16; ++y) screen += s.row(y) + "\n";
    CK_CHECK(screen.find("März") != std::string::npos);
    CK_CHECK(screen.find("Mo Di Mi Do Fr Sa So") != std::string::npos);
}

CK_TEST(a_time_picker_writes_the_meridiem_its_host_gives_it) {
    Scripted s;
    auto* picker = s.mount(std::make_unique<TimePicker>(), ckv::Rect{0, 2, 16, 1});
    picker->set_value(TimeValue{21, 5, 0});
    picker->set_24_hour(false);
    picker->set_meridiem_labels("vorm.", "nachm.");
    s.app.step(0);
    CK_CHECK(s.row(2).starts_with("09:05:00 nachm."));
    CK_CHECK(s.press(Key::Down));  // the hour, back into the morning's half
    picker->set_value(TimeValue{9, 5, 0});
    s.app.step(0);
    CK_CHECK(s.row(2).starts_with("09:05:00 vorm."));
    picker->set_meridiem_labels("", "");
    s.app.step(0);
    CK_CHECK(s.row(2).starts_with("09:05:00 "));
    CK_CHECK(s.row(2).find("vorm") == std::string::npos);
}

CK_TEST(a_calendar_dropdown_says_a_refused_year_is_invalid_in_its_tables_word) {
    Scripted s;
    auto* desktop = s.app.root().add(std::make_unique<Desktop>(s.app.root().bounds()));
    auto* anchor = s.mount_in(*desktop, std::make_unique<Button>("Date"), ckv::Rect{30, 0, 10, 1});
    CalendarDropdown* dropdown = show_calendar_dropdown(*anchor, s.app, *desktop);
    DateTimeLabels labels;
    labels.invalid_year = "ungültig";
    dropdown->set_labels(labels);
    dropdown->show_month(DateValue{2026, 8, 1});
    s.app.set_focus(&dropdown->year_field());
    dropdown->year_field().set_text("26");
    CK_CHECK(s.press(Key::Enter));
    std::string screen;
    for (int y = 0; y < 16; ++y) screen += s.row(y) + "\n";
    CK_CHECK(screen.find("ungültig") != std::string::npos);
}

// --- Wizard: pages with content, the step indicator, Cancel, typed completion

CK_TEST(a_scripted_wizard_shows_the_current_pages_content_and_moves_the_focus_with_the_page) {
    Scripted s;
    auto wizard = std::make_unique<Wizard>();
    wizard->set_pages({{"Your name", {}}, {"Your town", {}}, {"Done", {}}});
    auto name = std::make_unique<InputLine>();
    InputLine* const name_field = name.get();
    wizard->set_page_content(0, std::move(name));
    auto town = std::make_unique<InputLine>();
    InputLine* const town_field = town.get();
    wizard->set_page_content(1, std::move(town));
    std::optional<WizardOutcome> outcome;
    wizard->on_complete = [&](WizardOutcome ended) { outcome = ended; };
    auto* mounted = s.mount(std::move(wizard), ckv::Rect{0, 0, 40, 3});
    s.app.set_focus(name_field);
    s.app.step(0);
    CK_CHECK(s.row(0).starts_with("Your name"));
    CK_CHECK(s.cells(0, 0, 40).ends_with("Step 1 of 3"));
    CK_CHECK(name_field->visible() && !town_field->visible());
    CK_CHECK(name_field->bounds() == (ckv::Rect{0, 1, 40, 1}));

    CK_CHECK(s.type("Ada"));
    // Enter in the page's field reaches the wizard: on to the next page,
    // and the focus with it.
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(mounted->current_page() == 1U);
    CK_CHECK(s.app.focused() == town_field);
    CK_CHECK(!name_field->visible() && town_field->visible());
    CK_CHECK(s.cells(0, 0, 40).ends_with("Step 2 of 3"));
    CK_CHECK(s.press(Key::Enter));
    // A page with no content leaves the focus on the wizard itself.
    CK_CHECK(mounted->current_page() == 2U);
    CK_CHECK(s.app.focused() == mounted);
    CK_CHECK(s.row(2).find("Finish") != std::string::npos);
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(outcome == WizardOutcome::Finished);
    CK_CHECK(name_field->text() == "Ada");
}

CK_TEST(a_scripted_wizard_is_operated_by_its_drawn_back_next_and_cancel_controls) {
    Scripted s;
    bool ready = false;
    auto wizard = std::make_unique<Wizard>();
    wizard->set_pages({{"One", [&] { return ready; }}, {"Two", {}}});
    std::vector<WizardOutcome> outcomes;
    wizard->on_complete = [&](WizardOutcome outcome) { outcomes.push_back(outcome); };
    auto* mounted = s.mount(std::move(wizard), ckv::Rect{0, 0, 30, 4});
    // "< Back" (hidden on the first page), "Next >" after it, "Cancel" at
    // the right edge.
    CK_CHECK(s.cells(0, 3, 30) == "       Next >           Cancel");
    CK_CHECK(!s.click(ckv::Point{8, 3}));  // the page holds the reader back
    CK_CHECK(mounted->current_page() == 0U);
    ready = true;
    CK_CHECK(s.click(ckv::Point{8, 3}));
    CK_CHECK(mounted->current_page() == 1U);
    CK_CHECK(s.cells(0, 3, 30) == "< Back Finish           Cancel");
    CK_CHECK(s.click(ckv::Point{2, 3}));
    CK_CHECK(mounted->current_page() == 0U);
    CK_CHECK(!s.click(ckv::Point{20, 3}));  // between the controls: nothing
    CK_CHECK(s.click(ckv::Point{26, 3}));
    CK_CHECK((outcomes == std::vector<WizardOutcome>{WizardOutcome::Cancelled}));
    CK_CHECK(s.click(ckv::Point{8, 3}));
    CK_CHECK(s.click(ckv::Point{9, 3}));
    CK_CHECK((outcomes == std::vector<WizardOutcome>{WizardOutcome::Cancelled, WizardOutcome::Finished}));
}

CK_TEST(a_wizard_draws_its_hosts_words_and_gives_its_title_room_before_the_step_indicator) {
    Scripted s;
    auto wizard = std::make_unique<Wizard>();
    wizard->set_pages({{"Anmeldung", {}}, {"Fertig", {}}});
    WizardLabels labels;
    labels.back = "< Zurück";
    labels.next = "Weiter >";
    labels.finish = "Fertig";
    labels.cancel = "Abbrechen";
    labels.step = [](std::size_t current, std::size_t count) {
        return "Schritt " + std::to_string(current) + "/" + std::to_string(count);
    };
    wizard->set_labels(labels);
    auto* mounted = s.mount(std::move(wizard), ckv::Rect{0, 0, 40, 3});
    CK_CHECK(s.row(0).starts_with("Anmeldung"));
    CK_CHECK(s.row(0).find("Schritt 1/2") != std::string::npos);
    CK_CHECK(s.row(2).find("Weiter >") != std::string::npos);
    CK_CHECK(s.row(2).find("Abbrechen") != std::string::npos);
    // Too narrow for both, the title keeps the row.
    mounted->set_bounds(ckv::Rect{0, 0, 14, 3});
    s.app.step(0);
    CK_CHECK(s.cells(0, 0, 14) == "Anmeldung     ");
    // No step text, no indicator.
    labels.step = {};
    mounted->set_labels(labels);
    mounted->set_bounds(ckv::Rect{0, 0, 40, 3});
    s.app.step(0);
    CK_CHECK(s.row(0).find("Schritt") == std::string::npos);
}

CK_TEST(a_presented_wizard_completes_typed_and_without_blocking) {
    Scripted s;
    auto* desktop = s.app.root().add(std::make_unique<Desktop>(s.app.root().bounds()));
    const ckv::ui::StandardRoles roles = scripted_roles(s);
    const auto make = [] {
        auto wizard = std::make_unique<Wizard>();
        wizard->set_pages({{"Name", {}}, {"Confirm", {}}});
        wizard->set_page_content(0, std::make_unique<InputLine>());
        return wizard;
    };

    WizardPresentation finished = present_modal_wizard(make(), "Set up", s.app, *desktop, roles);
    std::optional<WizardOutcome> first;
    finished.set_completion_handler([&](WizardOutcome outcome) { first = outcome; });
    s.app.step(0);
    CK_CHECK(s.app.is_modal());
    CK_CHECK(dynamic_cast<InputLine*>(s.app.focused()) != nullptr);  // the page's field
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(!first);  // presenting returned at once; nothing has ended yet
    CK_CHECK(s.press(Key::Enter));
    CK_CHECK(first == WizardOutcome::Finished);
    CK_CHECK(!s.app.is_modal());

    WizardPresentation cancelled = present_modal_wizard(make(), "Set up", s.app, *desktop, roles);
    s.app.step(0);
    CK_CHECK(s.press(Key::Escape));
    CK_CHECK((cancelled.result() == std::optional<WizardOutcome>{WizardOutcome::Cancelled}));

    // Taken away from outside, it completes as cancelled too.
    WizardPresentation removed = present_modal_wizard(make(), "Set up", s.app, *desktop, roles);
    s.app.step(0);
    std::unique_ptr<ckv::ui::View> gone = desktop->remove_child(desktop->windows().back());
    CK_CHECK((removed.result() == std::optional<WizardOutcome>{WizardOutcome::Cancelled}));
}
