// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <string>
#include <string_view>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/input_line.hpp"
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
