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
