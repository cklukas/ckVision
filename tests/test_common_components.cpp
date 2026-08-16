// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/common_components.hpp"

#include <algorithm>
#include <optional>

#include "cktest.hpp"
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
