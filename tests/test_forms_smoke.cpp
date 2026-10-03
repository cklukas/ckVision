// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/option_group.hpp"
#include "forms_app.hpp"

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
    ckv::forms::FormsApp forms{app};
};

bool display_contains(const ckv::term::VirtualDisplay& display, std::string_view needle) {
    const ckv::FrameView frame = display.frame();
    for (int y = 0; y < frame.size().height; ++y) {
        std::string row;
        for (int x = 0; x < frame.size().width; ++x) row += frame.at(ckv::Point{x, y}).grapheme();
        if (row.find(needle) != std::string::npos) return true;
    }
    return false;
}
}  // namespace

CK_TEST(forms_about_dialog_carries_the_project_copyright) {
    Fixture f;
    CK_CHECK(f.app.execute_command(f.app.commands().standard().help));
    f.app.step(0);
    CK_CHECK(display_contains(f.term.display(),
                              "Copyright (c) 2026 C. Klukas. All rights reserved."));
}

CK_TEST(forms_example_renders_form_controls_and_chrome) {
    Fixture f;
    f.app.step(0);
    CK_CHECK(display_contains(f.term.display(), "Forms"));
    CK_CHECK(display_contains(f.term.display(), "Profile"));
    CK_CHECK(display_contains(f.term.display(), "Quit"));
    CK_CHECK(f.app.focused() == f.forms.name_input());
    CK_CHECK(f.forms.options()->check_state(1) == ckv::widgets::CheckState::Mixed);
    CK_CHECK(f.forms.mode()->selected() == 0);
    CK_CHECK(f.forms.country()->text() == "DE");
    CK_CHECK((f.forms.date_picker()->value() ==
              std::optional<ckv::widgets::DateValue>{ckv::widgets::DateValue{2026, 8, 9}}));
    CK_CHECK(f.forms.time_picker()->value() == (ckv::widgets::TimeValue{14, 30, 0}));
    CK_CHECK(f.forms.spin_box()->value() == 3);
    CK_CHECK(f.forms.slider()->value() == 40);
    CK_CHECK(!f.forms.wizard()->can_go_next());
}

CK_TEST(forms_descriptor_dialog_vetoes_invalid_accept_and_records_valid_completion) {
    Fixture f;
    f.forms.present_profile_dialog();
    f.app.step(0);
    CK_CHECK(f.app.is_modal());

    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    f.app.step(0);
    CK_CHECK(f.app.is_modal());
    CK_CHECK(f.forms.validation_attempts() == 1);

    f.app.dispatch(ckv::TextEvent{"Ada", false});
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Tab, Modifier::None, ""}});
    f.app.dispatch(ckv::TextEvent{"ada@example.invalid", false});
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    f.app.step(0);

    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.forms.last_dialog_result().has_value());
    CK_CHECK(f.forms.last_dialog_result()->accepted);
}

CK_TEST(forms_message_box_uses_the_standard_modal_path) {
    Fixture f;
    f.forms.present_info_message();
    f.app.step(0);
    CK_CHECK(f.app.is_modal());
    CK_CHECK(f.term.written_bytes().find("Forms") != std::string::npos);

    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    f.app.step(0);
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.forms.last_message_result() == ckv::widgets::MessageBoxResult::Ok);
}

CK_TEST(forms_window_close_veto_is_visible_to_clients) {
    Fixture f;
    CK_CHECK(!f.forms.window()->close());
    CK_CHECK(f.forms.desktop().windows().size() == 1);

    f.forms.set_close_allowed(true);
    CK_CHECK(f.forms.window()->close());
    CK_CHECK(f.forms.desktop().windows().size() == 1);
}

// --- Focus, mnemonics and the status line, as a reader drives them ---------
//
// Application-level scripts: every key and click enters through the
// HeadlessTerminal and is delivered by Application::step.

namespace {

std::string read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

struct Script {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    ckv::forms::FormsApp forms{app};

    Script() { app.step(0); }

    void key(KeyChord chord) {
        term.inject_event(ckv::KeyEvent{std::move(chord)});
        app.step(0);
    }
    void press(Key k, Modifier modifiers = Modifier::None) { key(KeyChord{k, modifiers, ""}); }
    void alt(const char* letter) { key(KeyChord{Key::Char, Modifier::Alt, letter}); }
    void text(std::string typed) {
        term.inject_event(ckv::TextEvent{std::move(typed), false});
        app.step(0);
    }
    void click(ckv::Point cell) {
        term.inject_event(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, cell, std::nullopt,
                                          Modifier::None});
        term.inject_event(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, cell, std::nullopt,
                                          Modifier::None});
        app.step(0);
    }
    std::string status() const {
        std::string row;
        const ckv::FrameView frame = term.display().frame();
        for (int x = 0; x < frame.size().width; ++x) row += frame.at(ckv::Point{x, 23}).grapheme();
        return row;
    }
    std::string frame() const {
        return ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
    }
    // Every field of the Forms window, in the order the window lists them.
    std::vector<const ckv::ui::View*> fields() const {
        return {forms.name_input(),  forms.options(),     forms.mode(),     &forms.country()->focus_target(),
                forms.date_picker(), forms.time_picker(), forms.spin_box(), forms.slider(),
                forms.wizard(), &forms.wizard()->cancel_button(), forms.dialog_button(), forms.message_button(), forms.help_button()};
    }
};

}  // namespace

CK_TEST(forms_tab_and_shift_tab_walk_every_field_forward_and_back) {
    Script s;
    const std::vector<const ckv::ui::View*> fields = s.fields();
    CK_CHECK(s.app.focused() == fields.front());
    for (std::size_t index = 1; index < fields.size(); ++index) {
        s.press(Key::Tab);
        CK_CHECK(s.app.focused() == fields[index]);
    }
    // Past the last field the walk reaches the menu bar, a Tab stop of its
    // own, and then comes round to the first field again.
    s.press(Key::Tab);
    CK_CHECK(dynamic_cast<ckv::widgets::MenuBar*>(s.app.focused()) != nullptr);
    s.press(Key::Tab);
    CK_CHECK(s.app.focused() == fields.front());

    s.press(Key::Tab, Modifier::Shift);
    CK_CHECK(dynamic_cast<ckv::widgets::MenuBar*>(s.app.focused()) != nullptr);
    for (std::size_t index = fields.size(); index-- > 0;) {
        s.press(Key::Tab, Modifier::Shift);
        CK_CHECK(s.app.focused() == fields[index]);
    }
}

CK_TEST(forms_arrows_move_within_an_option_group_and_the_focus_frame_is_pinned) {
    Script s;
    const std::string initial = read_file("golden/forms_focus_name.dump");
    CK_CHECK(!initial.empty());
    CK_CHECK(s.frame() == initial);

    s.press(Key::Tab);
    CK_CHECK(s.app.focused() == s.forms.options());
    s.press(Key::Tab);
    CK_CHECK(s.app.focused() == s.forms.mode());
    CK_CHECK(s.forms.mode()->selected() == 0);
    s.press(Key::Down);  // within the group: the next choice, and the focus stays
    CK_CHECK(s.forms.mode()->selected() == 1);
    CK_CHECK(s.app.focused() == s.forms.mode());
    const std::string group = read_file("golden/forms_focus_group.dump");
    CK_CHECK(!group.empty());
    CK_CHECK(s.frame() == group);
    s.press(Key::Up);
    CK_CHECK(s.forms.mode()->selected() == 0);
    CK_CHECK(s.app.focused() == s.forms.mode());

    // The check group: arrows move its cursor, wrapping within the group,
    // Space toggles the choice under it, and the focus stays on the group
    // throughout; Tab is what leaves it.
    s.press(Key::Tab, Modifier::Shift);
    CK_CHECK(s.app.focused() == s.forms.options());
    CK_CHECK(!s.forms.options()->checked(2));
    s.press(Key::Down);
    s.press(Key::Down);
    CK_CHECK(s.app.focused() == s.forms.options());
    s.key(KeyChord{Key::Char, Modifier::None, " "});
    CK_CHECK(s.forms.options()->checked(2));
    CK_CHECK(s.app.focused() == s.forms.options());
    s.press(Key::Down);  // from the last choice round to the first
    CK_CHECK(s.app.focused() == s.forms.options());
    CK_CHECK(!s.forms.options()->checked(0));
    s.key(KeyChord{Key::Char, Modifier::None, " "});
    CK_CHECK(s.forms.options()->checked(0));
    s.press(Key::Tab);
    CK_CHECK(s.app.focused() == s.forms.mode());
}

CK_TEST(forms_label_mnemonics_focus_their_buddy_in_the_window_and_in_the_profile_dialog) {
    Script s;
    s.press(Key::Tab);
    s.press(Key::Tab);
    CK_CHECK(s.app.focused() == s.forms.mode());
    s.alt("n");  // the "&Name:" label's buddy
    CK_CHECK(s.app.focused() == s.forms.name_input());

    s.alt("p");  // the "&Profile..." button
    CK_CHECK(s.app.is_modal());
    s.text("Ada");
    s.alt("e");  // the dialog's "&Email:" label
    auto* email = dynamic_cast<ckv::widgets::InputLine*>(s.app.focused());
    CK_CHECK(email != nullptr);
    s.text("ada@example.invalid");
    s.alt("n");  // back to its "&Name:" field
    auto* name = dynamic_cast<ckv::widgets::InputLine*>(s.app.focused());
    CK_CHECK(name != nullptr && name != email);
    CK_CHECK(name->text() == "Ada");
    s.press(Key::Enter);
    CK_CHECK(!s.app.is_modal());
    CK_CHECK(s.forms.last_dialog_result().has_value());
    CK_CHECK(s.forms.last_dialog_result()->accepted);
    CK_CHECK(s.forms.last_dialog_result()->values ==
             (std::vector<std::string>{"Ada", "ada@example.invalid"}));
}

CK_TEST(forms_status_hint_follows_focus_and_names_the_bound_chord) {
    Script s;
    CK_CHECK(s.status().find("Shift+F1 opens the forms help topic") != std::string::npos);
    s.press(Key::Tab);
    CK_CHECK(s.app.focused() == s.forms.options());
    CK_CHECK(s.status().find("Arrows move within the group") != std::string::npos);
    CK_CHECK(s.status().find("forms help topic") == std::string::npos);
    s.press(Key::Tab);
    s.press(Key::Tab);
    CK_CHECK(s.app.focused() == &s.forms.country()->focus_target());
    CK_CHECK(s.status().find("Arrows move within the group") == std::string::npos);

    // Rebound, the hint names the new chord, and the new chord opens the topic.
    s.app.commands().unbind_key(*KeyChord::parse("Shift+F1"));
    s.app.commands().bind_key(KeyChord{Key::F12, Modifier::None, ""}, s.forms.forms_help_command());
    s.alt("n");
    CK_CHECK(s.status().find("F12 opens the forms help topic") != std::string::npos);
    CK_CHECK(s.forms.desktop().windows().size() == 1);
    s.press(Key::F12);
    CK_CHECK(s.forms.desktop().windows().size() == 2);
    CK_CHECK(s.frame().find("Forms Help") != std::string::npos);
}

CK_TEST(forms_clicking_status_items_runs_their_commands) {
    Script s;
    const std::string line = s.status();
    const std::size_t help = line.find("F1 Help");
    CK_CHECK(help != std::string::npos);
    s.click(ckv::Point{static_cast<int>(help), 23});
    CK_CHECK(s.app.is_modal());  // the About box the help command presents
    CK_CHECK(s.frame().find("ckVision Forms example") != std::string::npos);
    s.press(Key::Escape);
    CK_CHECK(!s.app.is_modal());

    const std::size_t quit = s.status().find("Quit");
    CK_CHECK(quit != std::string::npos);
    CK_CHECK(!s.app.quit_requested());
    s.click(ckv::Point{static_cast<int>(quit), 23});
    CK_CHECK(s.app.quit_requested());
}

CK_TEST(forms_wizard_steps_on_once_named_and_reports_how_it_ended) {
    Script s;
    CK_CHECK(!s.forms.wizard_outcome());
    s.text("Ada");
    // Clicking Next where it is drawn moves the wizard on, Finish ends it.
    const ckv::Rect wizard = s.forms.wizard()->absolute_bounds();
    const ckv::Point next{wizard.x + 8, wizard.bottom() - 1};
    s.click(next);
    CK_CHECK(s.forms.wizard()->current_page() == 1U);
    CK_CHECK(s.frame().find("Step 2 of 2") != std::string::npos);
    s.click(next);
    CK_CHECK(s.forms.wizard_outcome() == ckv::widgets::WizardOutcome::Finished);
    // Escape with the wizard focused cancels it.
    s.press(Key::Escape);
    CK_CHECK(s.app.focused() == &s.forms.wizard()->forward_button());
    CK_CHECK(s.forms.wizard_outcome() == ckv::widgets::WizardOutcome::Cancelled);
}
