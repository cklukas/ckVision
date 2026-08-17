// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/common_components.hpp"

#include <algorithm>
#include <optional>

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::ui::Application;
using namespace ckv::widgets;

namespace {
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

    CK_CHECK(date.value() == (DateValue{2026, 12, 24}));
    CK_CHECK(time.on_key(key(Key::Up)));
    CK_CHECK(time.value() == (TimeValue{0, 58, 59}));
    CK_CHECK(time.on_key(key(Key::Right)));
    CK_CHECK(time.on_key(key(Key::Up)));
    CK_CHECK(time.value() == (TimeValue{0, 59, 59}));
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
    CK_CHECK(search.on_text(ckv::TextEvent{"abc", false}));
    CK_CHECK(query == "abc");
    CK_CHECK(search.on_key(key(Key::Escape)));
    CK_CHECK(search.query().empty());

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
    constexpr ckv::ui::CommandId command = ckv::ui::kFirstApplicationCommandId + 200;
    app.commands().add(ckv::ui::CommandDescriptor{command, "Build", "test", "", "", [&] { ++ran; }});

    auto toolbar = std::make_unique<ToolBar>();
    toolbar->set_bounds(ckv::Rect{0, 0, 20, 1});
    toolbar->set_commands({command});
    ToolBar* toolbar_view = toolbar.get();
    app.root().add(std::move(toolbar));
    CK_CHECK(toolbar_view->on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                                    ckv::Point{1, 0}, std::nullopt, Modifier::None}));
    CK_CHECK(ran == 1);

    auto palette = std::make_unique<CommandPalette>();
    CommandPalette* palette_ptr = palette.get();
    app.root().add(std::move(palette));
    const auto palette_commands = palette_ptr->filtered_commands();
    CK_CHECK(std::all_of(palette_commands.begin(), palette_commands.end(),
                         [](const ckv::ui::CommandInfo& info) {
                             return info.id >= ckv::ui::kFirstApplicationCommandId;
                         }));
    CK_CHECK(std::none_of(palette_commands.begin(), palette_commands.end(), [](const ckv::ui::CommandInfo& info) {
        return info.id == ckv::ui::std_commands::kQuit;
    }));
    palette_ptr->set_query("bui");
    CK_CHECK(palette_ptr->highlighted_command() == command);
    CK_CHECK(palette_ptr->on_key(key(Key::Enter)));
    CK_CHECK(ran == 2);
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

    std::optional<std::string> changed;
    inspector.set_items({PropertyItem{"Name", "ckVision", true}});
    inspector.on_change = [&](std::size_t, std::string value) { changed = std::move(value); };
    CK_CHECK(inspector.on_key(key(Key::Enter)));
    CK_CHECK(inspector.on_text(ckv::TextEvent{"!", false}));
    CK_CHECK(changed == std::string{"ckVision!"});

    bool valid = false;
    bool finished = false;
    wizard.set_pages({WizardPage{"Step 1", [&] { return valid; }}, WizardPage{"Step 2", [] { return true; }}});
    wizard.on_finish = [&] { finished = true; };
    CK_CHECK(!wizard.next());
    valid = true;
    CK_CHECK(wizard.next());
    CK_CHECK(wizard.finish());
    CK_CHECK(finished);

    notifications.add(Notification{NotificationSeverity::Warning, "Saved", true});
    CK_CHECK(notifications.notifications().size() == 1);
    CK_CHECK(notifications.on_key(key(Key::Escape)));
    CK_CHECK(notifications.notifications().empty());

    tooltip.show_at(ckv::Point{3, 4});
    CK_CHECK(tooltip.shown());
    tooltip.hide();
    CK_CHECK(!tooltip.shown());
}

CK_TEST(a_search_box_accepts_ordinary_typed_characters) {
    // Regression: the box only ever appended text from TextEvent, but a
    // terminal reports typed characters as Key::Char key events — TextEvent
    // is for IMEs and paste. The box therefore could not be typed into at
    // all, which is the one thing it exists to do.
    ckv::widgets::SearchBox box;
    std::vector<std::string> observed;
    box.on_change = [&observed](const std::string& query) { observed.push_back(query); };

    const auto type = [&box](const char* text) {
        return box.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::None, text}});
    };
    CK_CHECK(type("v"));
    CK_CHECK(type("i"));
    CK_CHECK(type("m"));
    CK_CHECK(box.query() == "vim");
    CK_CHECK(observed.size() == 3U);
    CK_CHECK(observed.back() == "vim");

    // Backspace and Esc keep working, and Esc reports the clear.
    CK_CHECK(box.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Backspace, ckv::Modifier::None, ""}}));
    CK_CHECK(box.query() == "vi");
    CK_CHECK(box.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Escape, ckv::Modifier::None, ""}}));
    CK_CHECK(box.query().empty());

    // A modified character is a chord, not text: command routing must still
    // be able to claim it.
    CK_CHECK(!box.on_key(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Char, ckv::Modifier::Ctrl, "q"}}));
    CK_CHECK(box.query().empty());
}

CK_TEST(a_search_box_looks_like_a_field_and_its_clear_control_is_where_it_is_drawn) {
    // Regression: the box drew "Search: <query> [x]" as one run of
    // label-coloured cells, so it read as a caption rather than something to
    // type into — and the "[x]" landed wherever the query happened to end,
    // while only the last three columns answered a click.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::ui::RoleRegistry registry;
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(registry);
    ckv::ui::Theme theme = ckv::ui::make_classic_theme(registry, roles);

    ckv::widgets::SearchBox box;
    box.set_context(ckv::ui::Context{&theme, &registry, &app});
    box.on_attached();
    box.set_bounds(ckv::Rect{0, 0, 20, 1});

    ckv::scene::Surface surface(ckv::Size{20, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 20, 1});
    box.draw(painter);

    // The field carries the input surface's own background, which is what
    // says "text goes here"; the prompt does not.
    const ckv::Style field = surface.at(ckv::Point{10, 0}).style();
    const ckv::Style prompt = surface.at(ckv::Point{0, 0}).style();
    CK_CHECK(field.bg == theme.resolve(roles.input_normal).bg);
    CK_CHECK(field.bg != prompt.bg);

    // With no query there is nothing to clear, so no control is offered...
    std::string row;
    for (int x = 0; x < 20; ++x) row += surface.at(ckv::Point{x, 0}).grapheme();
    CK_CHECK(row.find("[x]") == std::string::npos);
    CK_CHECK(!box.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                            ckv::Point{19, 0}, std::nullopt, ckv::Modifier::None}) == false);

    // ...and once there is, it is drawn at the right edge, exactly where a
    // click on it is answered.
    box.set_query("vim");
    box.draw(painter);
    row.clear();
    for (int x = 0; x < 20; ++x) row += surface.at(ckv::Point{x, 0}).grapheme();
    CK_CHECK(row.find("vim") != std::string::npos);
    CK_CHECK(row.rfind("[x]") == 17U);  // the last three columns of a 20-wide box

    CK_CHECK(box.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left,
                                           ckv::Point{18, 0}, std::nullopt, ckv::Modifier::None}));
    CK_CHECK(box.query().empty());
}

CK_TEST(a_focused_search_box_shows_a_caret_where_typing_will_land) {
    ckv::widgets::SearchBox box;
    box.set_bounds(ckv::Rect{0, 0, 20, 1});
    CK_CHECK(!box.cursor_state().has_value());  // unfocused: no caret

    box.on_focus(ckv::FocusEvent{true});
    const auto empty_caret = box.cursor_state();
    CK_CHECK(empty_caret.has_value());
    CK_CHECK(empty_caret->visible);

    box.set_query("vi");
    const auto typed_caret = box.cursor_state();
    CK_CHECK(typed_caret.has_value());
    CK_CHECK(typed_caret->position.x == empty_caret->position.x + 2);  // follows the text
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

    CK_CHECK(day_style(surface, calendar, 9).bg == s.theme.resolve(s.roles.list_selected).bg);
    // A day that is today AND in the span shows today.
    calendar.set_selected(DateValue{2026, 8, 20});
    CK_CHECK(day_style(surface, calendar, 9).bg == s.theme.resolve(s.roles.calendar_today).bg);
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
    c.press(ckv::Key::Down);  // open the list
    CK_CHECK(c.dropdown->month_picker().dropdown_open());
    // The list is drawn over the rows below it, so those rows step aside.
    CK_CHECK(!c.dropdown->calendar().visible());
    CK_CHECK(!c.dropdown->year_field().visible());
    for (int i = 0; i < 3; ++i) c.press(ckv::Key::Down);  // August -> November
    c.press(ckv::Key::Enter);
    CK_CHECK(c.dropdown->calendar().month().month == 11);
    CK_CHECK(c.dropdown->calendar().month().year == 2026);
    CK_CHECK(c.dropdown->calendar().visible());
    CK_CHECK(c.dropdown->year_field().visible());
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
    c.press(ckv::Key::Tab);  // onto the year field
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

CK_TEST(the_month_list_opens_at_full_length_rather_than_scrolling_in_place) {
    DroppedCalendar c;
    const int closed = c.dropdown->bounds().height;
    c.press(ckv::Key::Tab);   // onto the month picker
    c.press(ckv::Key::Down);  // open the list
    // Twelve months shown through eight rows is a list the reader has to
    // scroll to see; there is room below, so the popup takes it.
    CK_CHECK(c.dropdown->bounds().height > closed);
    CK_CHECK(c.dropdown->month_picker().bounds().height >= 13);  // the row and all twelve
    c.press(ckv::Key::Escape);  // closes the list, not the popup
    CK_CHECK(c.dropdown->bounds().height == closed);
}

CK_TEST(the_month_picker_is_as_wide_as_its_longest_month_and_no_wider) {
    DroppedCalendar c;
    // Space past the longest name is space the year beside it could have had,
    // and the year is on the same row.
    CK_CHECK(c.dropdown->month_picker().bounds().width == 11);  // "September" plus its arrow
    const ckv::Rect month = c.dropdown->month_picker().bounds();
    const ckv::Rect year = c.dropdown->year_field().bounds();
    CK_CHECK(year.y == month.y);
    CK_CHECK(year.x > month.right());  // the steppers sit between them
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
