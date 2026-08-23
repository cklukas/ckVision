// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Cross-family D-038 presentation-result contract tests. Individual widget
// suites cover each dialog's controls; this file keeps lifecycle outcomes
// visibly uniform across the public present_* surface.
#include "cvision/widgets/directory_picker.hpp"
#include "cvision/widgets/dialog.hpp"
#include "cvision/widgets/file_dialog.hpp"
#include "cvision/widgets/help_viewer.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/window_list_dialog.hpp"

#include <optional>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/command.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"

using ckv::ManualClock;
using ckv::MemoryFileSystem;
using ckv::Rect;
using ckv::Size;
using ckv::term::HeadlessTerminal;
using ckv::ui::Application;
using ckv::ui::StandardRoles;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::widgets::Desktop;
using ckv::widgets::DialogDescriptor;
using ckv::widgets::DialogResult;
using ckv::widgets::DirectoryPickerResult;
using ckv::widgets::FieldDescriptor;
using ckv::widgets::FileDialogMode;
using ckv::widgets::FileDialogResult;
using ckv::widgets::HelpViewerResult;
using ckv::widgets::MemoryHelpProvider;
using ckv::widgets::MessageBoxButtons;
using ckv::widgets::MessageBoxDescriptor;
using ckv::widgets::MessageBoxKind;
using ckv::widgets::MessageBoxResult;
using ckv::widgets::WindowListDialogResult;
using ckv::widgets::Window;
using ckv::widgets::present_directory_picker;
using ckv::widgets::present_dialog;
using ckv::widgets::present_file_dialog;
using ckv::widgets::present_help_viewer;
using ckv::widgets::present_message_box;
using ckv::widgets::present_window_list_dialog;
using ckv::widgets::exec_dialog;
using ckv::widgets::ButtonDescriptor;
using ckv::widgets::ButtonRole;

namespace {

// The framework's own commands, by name. A test names the concept and
// asks the registry that assigned the ids, exactly as application code
// does — no test knows or states a command's number.
const ckv::ui::StandardCommands& standard(const ckv::ui::Application& app) {
    return app.commands().standard();
}

struct Fixture {
    HeadlessTerminal terminal{Size{80, 24}};
    ManualClock clock;
    Application app{terminal, clock};
    StandardRoles roles = intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;

    Fixture() {
        app.theme() = make_classic_theme(app.roles(), roles);
        desktop = app.root().add(
            std::make_unique<Desktop>(Rect{0, 0, 80, 24}));
    }
};

MemoryFileSystem sample_file_system() {
    MemoryFileSystem fs;
    fs.add_directory("/home/user/docs");
    fs.add_file("/home/user/readme.txt");
    return fs;
}

MemoryHelpProvider sample_help() {
    MemoryHelpProvider provider;
    provider.add_topic("intro", {"Introduction", "Welcome.", {}});
    return provider;
}

MessageBoxDescriptor confirmation() {
    return MessageBoxDescriptor{MessageBoxKind::Confirm, "Confirm", "Proceed?", MessageBoxButtons::OkCancel};
}

class FormerFocus final : public ckv::ui::View {
public:
    FormerFocus() { set_focus_policy(ckv::ui::FocusPolicy::TabStop); }
};

FormerFocus* add_former_focus(Fixture& fixture) {
    auto* focus = fixture.app.root().add(std::make_unique<FormerFocus>());
    focus->set_bounds(Rect{0, 0, 1, 1});
    fixture.app.set_focus(focus);
    return focus;
}

void destroy_former_focus(Fixture& fixture, FormerFocus* focus) {
    std::unique_ptr<ckv::ui::View> removed = fixture.app.root().remove_child(focus);
    CK_CHECK(removed.get() == focus);
    removed.reset();
}

}  // namespace

CK_TEST(standard_dialogs_never_dereference_a_destroyed_former_focus_on_close) {
    Fixture f;
    auto fs = sample_file_system();
    auto help = sample_help();

    auto* message_focus = add_former_focus(f);
    auto message = present_message_box(f.app, *f.desktop, f.roles, confirmation());
    destroy_former_focus(f, message_focus);
    CK_CHECK(f.desktop->windows().back()->close());
    f.app.step(0);
    CK_CHECK(message.result() == MessageBoxResult::Cancel);

    auto* file_focus = add_former_focus(f);
    auto file = present_file_dialog(FileDialogMode::Open, "/home/user", fs, f.app, *f.desktop, f.roles);
    destroy_former_focus(f, file_focus);
    CK_CHECK(f.desktop->windows().back()->close());
    f.app.step(0);
    CK_CHECK(file.result().has_value());
    CK_CHECK(!file.result()->accepted);

    auto* directory_focus = add_former_focus(f);
    auto directory = present_directory_picker(fs, "/home/user", f.app, *f.desktop, f.roles);
    destroy_former_focus(f, directory_focus);
    CK_CHECK(f.desktop->windows().back()->close());
    f.app.step(0);
    CK_CHECK(directory.result().has_value());
    CK_CHECK(!directory.result()->accepted);

    auto* list_focus = add_former_focus(f);
    auto list = present_window_list_dialog(*f.desktop, f.app, f.roles);
    destroy_former_focus(f, list_focus);
    CK_CHECK(f.desktop->windows().back()->close());
    f.app.step(0);
    CK_CHECK(list.result() == WindowListDialogResult::Closed);

    auto* help_focus = add_former_focus(f);
    auto viewer = present_help_viewer(help, "intro", f.app, *f.desktop, f.roles);
    destroy_former_focus(f, help_focus);
    CK_CHECK(f.desktop->windows().back()->close());
    f.app.step(0);
    CK_CHECK(viewer.result() == HelpViewerResult::Closed);
    CK_CHECK(!f.app.is_modal());
}

CK_TEST(descriptor_dialog_presentation_returns_typed_values_only_after_detachment) {
    Fixture f;
    bool default_pressed = false;
    DialogDescriptor descriptor;
    descriptor.title = "Connection";
    descriptor.fields.push_back(FieldDescriptor{"&Host:", "local", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, [&] { default_pressed = true; }});

    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    CK_CHECK(f.app.is_modal());
    CK_CHECK(!presentation.completed());
    f.terminal.inject_bytes("host\r", 0);
    CK_CHECK(f.app.step(0));
    CK_CHECK(default_pressed);
    // The self-detach is deferred until the button callback unwinds, then
    // drained by this same step's ordinary posted-work phase.
    CK_CHECK((presentation.result() == DialogResult{true, {"localhost"}, {false}, {-1}, {std::nullopt}}));
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.desktop->windows().empty());
}

CK_TEST(pressing_a_dismissing_button_ends_a_presented_dialog_with_no_values) {
    // Driven the way a reader drives it: Tab onto Cancel, press Space. What
    // this pins is the whole of what a Cancel promises — the dialog goes, the
    // modality goes with it, and nothing the reader typed comes back as an
    // answer they never gave.
    Fixture f;
    bool cancel_ran = false;
    DialogDescriptor descriptor;
    descriptor.title = "Connection";
    descriptor.fields.push_back(FieldDescriptor{"&Host:", "local", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    descriptor.buttons.push_back(
        ButtonDescriptor{"Cancel", ButtonRole::Dismiss, [&] { cancel_ran = true; }});

    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    f.terminal.inject_bytes("typed", 0);
    CK_CHECK(f.app.step(0));

    ckv::widgets::Button* cancel = nullptr;
    for (int step = 0; step < 8 && cancel == nullptr; ++step) {
        f.app.focus_next();
        auto* const focused = dynamic_cast<ckv::widgets::Button*>(f.app.focused());
        if (focused != nullptr && focused->text() == "Cancel") cancel = focused;
    }
    CK_CHECK(cancel != nullptr);

    f.terminal.inject_bytes(" ", 0);
    CK_CHECK(f.app.step(0));
    CK_CHECK(cancel_ran);
    CK_CHECK(presentation.result() == DialogResult{});
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.desktop->windows().empty());
}

CK_TEST(pressing_the_accepting_button_accepts_with_the_typed_values) {
    // The other half of the button row, driven the same way: Tab onto OK,
    // press Space — or click it, which is the same fire_press. It used to do
    // nothing at all: the accept path lived only on the window's
    // accept_request, which Enter reaches and a button press did not, so OK
    // buttons everywhere answered the keyboard and ignored the mouse.
    Fixture f;
    bool ok_ran = false;
    DialogDescriptor descriptor;
    descriptor.title = "Connection";
    descriptor.fields.push_back(FieldDescriptor{"&Host:", "local", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, [&] { ok_ran = true; }});
    descriptor.buttons.push_back(ButtonDescriptor{"Cancel", ButtonRole::Dismiss, nullptr});

    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    f.terminal.inject_bytes("typed", 0);
    CK_CHECK(f.app.step(0));

    ckv::widgets::Button* ok = nullptr;
    for (int step = 0; step < 8 && ok == nullptr; ++step) {
        f.app.focus_next();
        auto* const focused = dynamic_cast<ckv::widgets::Button*>(f.app.focused());
        if (focused != nullptr && focused->text() == "OK") ok = focused;
    }
    CK_CHECK(ok != nullptr);

    f.terminal.inject_bytes(" ", 0);
    CK_CHECK(f.app.step(0));
    CK_CHECK(ok_ran);
    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result().has_value());
    if (presentation.result().has_value()) {
        CK_CHECK(presentation.result()->accepted);
        CK_CHECK(!presentation.result()->values.empty());
        if (!presentation.result()->values.empty())
            CK_CHECK(presentation.result()->values.front() == "typedlocal" ||
                     presentation.result()->values.front() == "localtyped");
    }
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.desktop->windows().empty());
}

CK_TEST(descriptor_dialog_supports_measured_centered_bottom_actions) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.title = "Measured";
    descriptor.fields.push_back(FieldDescriptor{"&Name:", "value", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"&OK", ButtonRole::Accept, nullptr});
    descriptor.minimum_window_size = Size{40, 12};
    descriptor.button_alignment = ckv::ui::Alignment::Center;
    descriptor.anchor_buttons_to_bottom = true;

    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    Window* const window = f.desktop->windows().back();
    CK_CHECK(window != nullptr);
    CK_CHECK((window->bounds() == Rect{20, 6, 40, 12}));
    CK_CHECK(window->content() != nullptr);
    // The fields' scroll viewport and the action row, its sibling below
    // (U4-g; before it, a third child sat between them — an expanding spacer
    // whose whole job the pane now does directly). Anchored, so the row takes
    // the pane's own last two rows; centered, so it sits in the middle of its
    // width. The measured 12-row window leaves the pane 9 rows.
    CK_CHECK(window->content()->children().size() == 2u);
    CK_CHECK((window->content()->children().back()->bounds() == Rect{13, 7, 10, 2}));
    (void)presentation;
}

CK_TEST(standard_modal_presentations_size_and_center_an_unpositioned_window) {
    Fixture f;
    auto message = present_message_box(
        f.app, *f.desktop, f.roles,
        MessageBoxDescriptor{MessageBoxKind::Info, "Visible", "A visible modal message.", MessageBoxButtons::Ok});
    Window* window = f.desktop->windows().back();
    const Rect area = f.desktop->content_area();
    CK_CHECK(window->bounds().width > 0);
    CK_CHECK(window->bounds().height > 0);
    CK_CHECK(window->bounds().x >= area.x);
    CK_CHECK(window->bounds().y >= area.y);
    CK_CHECK(window->bounds().x + window->bounds().width <= area.x + area.width);
    CK_CHECK(window->bounds().y + window->bounds().height <= area.y + area.height);

    f.app.step(0);
    std::string frame_text;
    for (int y = 0; y < f.app.current_frame().size().height; ++y)
        for (int x = 0; x < f.app.current_frame().size().width; ++x)
            frame_text += f.app.current_frame().at(ckv::Point{x, y}).grapheme();
    CK_CHECK(frame_text.find("Visible") != std::string::npos);
    CK_CHECK(frame_text.find("A visible modal message.") != std::string::npos);
    CK_CHECK(f.app.is_modal());
    (void)message;
}

CK_TEST(descriptor_dialog_external_detach_and_host_quit_return_cancellation) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&Name:", "value", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});

    auto detached = present_dialog(descriptor, f.app, *f.desktop, f.roles);
    CK_CHECK(f.desktop->remove_window(f.desktop->windows().back()) != nullptr);
    CK_CHECK(detached.result() == DialogResult{});

    f.app.post([&] { f.app.request_quit(); });
    const DialogResult quit = exec_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    CK_CHECK(quit == DialogResult{});
    CK_CHECK(f.desktop->windows().empty());
    CK_CHECK(!f.app.is_modal());
}

CK_TEST(descriptor_dialog_acceptance_survives_a_default_callback_that_destroys_its_window) {
    Fixture f;
    Window* dialog = nullptr;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&Name:", "value", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, [&] {
        std::unique_ptr<Window> removed = f.desktop->remove_window(dialog);
        CK_CHECK(removed.get() == dialog);
        removed.reset();
    }});

    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    dialog = f.desktop->windows().back();
    f.terminal.inject_bytes("\r", 0);
    CK_CHECK(f.app.step(0));
    CK_CHECK((presentation.result() == DialogResult{true, {"value"}, {false}, {-1}, {std::nullopt}}));
    CK_CHECK(!f.app.is_modal());
}

CK_TEST(external_detach_resolves_every_standard_presentation_to_its_documented_result) {
    Fixture f;
    auto fs = sample_file_system();
    auto help = sample_help();

    auto message = present_message_box(f.app, *f.desktop, f.roles, confirmation());
    CK_CHECK(f.desktop->remove_window(f.desktop->windows().back()) != nullptr);
    CK_CHECK(message.completed());
    CK_CHECK(message.result() == MessageBoxResult::Cancel);
    CK_CHECK(!f.app.is_modal());

    auto file = present_file_dialog(FileDialogMode::Open, "/home/user", fs, f.app, *f.desktop, f.roles);
    CK_CHECK(f.desktop->remove_window(f.desktop->windows().back()) != nullptr);
    CK_CHECK(file.completed());
    CK_CHECK(file.result().has_value());
    CK_CHECK(!file.result()->accepted);
    CK_CHECK(file.result()->path.empty());
    CK_CHECK(!f.app.is_modal());

    auto directory = present_directory_picker(fs, "/home/user", f.app, *f.desktop, f.roles);
    CK_CHECK(f.desktop->remove_window(f.desktop->windows().back()) != nullptr);
    CK_CHECK(directory.completed());
    CK_CHECK(directory.result().has_value());
    CK_CHECK(!directory.result()->accepted);
    CK_CHECK(directory.result()->path.empty());
    CK_CHECK(!f.app.is_modal());

    auto window_list = present_window_list_dialog(*f.desktop, f.app, f.roles);
    CK_CHECK(f.desktop->remove_window(f.desktop->windows().back()) != nullptr);
    CK_CHECK(window_list.completed());
    CK_CHECK(window_list.result() == WindowListDialogResult::Closed);
    CK_CHECK(!f.app.is_modal());

    auto viewer = present_help_viewer(help, "intro", f.app, *f.desktop, f.roles);
    CK_CHECK(f.desktop->remove_window(f.desktop->windows().back()) != nullptr);
    CK_CHECK(viewer.completed());
    CK_CHECK(viewer.result() == HelpViewerResult::Closed);
    CK_CHECK(!f.app.is_modal());
}

CK_TEST(close_resolves_every_standard_presentation_to_its_documented_result_after_detach) {
    Fixture f;
    auto fs = sample_file_system();
    auto help = sample_help();

    auto message = present_message_box(f.app, *f.desktop, f.roles, confirmation());
    CK_CHECK(f.desktop->windows().back()->close());
    CK_CHECK(!message.completed());
    f.app.step(0);
    CK_CHECK(message.result() == MessageBoxResult::Cancel);
    CK_CHECK(!f.app.is_modal());

    auto file = present_file_dialog(FileDialogMode::Open, "/home/user", fs, f.app, *f.desktop, f.roles);
    CK_CHECK(f.desktop->windows().back()->close());
    CK_CHECK(!file.completed());
    f.app.step(0);
    CK_CHECK(file.result().has_value());
    CK_CHECK(!file.result()->accepted);
    CK_CHECK(file.result()->path.empty());
    CK_CHECK(!f.app.is_modal());

    auto directory = present_directory_picker(fs, "/home/user", f.app, *f.desktop, f.roles);
    CK_CHECK(f.desktop->windows().back()->close());
    CK_CHECK(!directory.completed());
    f.app.step(0);
    CK_CHECK(directory.result().has_value());
    CK_CHECK(!directory.result()->accepted);
    CK_CHECK(directory.result()->path.empty());
    CK_CHECK(!f.app.is_modal());

    auto window_list = present_window_list_dialog(*f.desktop, f.app, f.roles);
    CK_CHECK(f.desktop->windows().back()->close());
    CK_CHECK(!window_list.completed());
    f.app.step(0);
    CK_CHECK(window_list.result() == WindowListDialogResult::Closed);
    CK_CHECK(!f.app.is_modal());

    auto viewer = present_help_viewer(help, "intro", f.app, *f.desktop, f.roles);
    CK_CHECK(f.desktop->windows().back()->close());
    CK_CHECK(!viewer.completed());
    f.app.step(0);
    CK_CHECK(viewer.result() == HelpViewerResult::Closed);
    CK_CHECK(!f.app.is_modal());
}

CK_TEST(queued_external_destruction_completes_a_presentation_once_after_modal_scope_removal) {
    Fixture f;
    auto presentation = present_message_box(f.app, *f.desktop, f.roles, confirmation());
    auto* const box = f.desktop->windows().back();
    int completions = 0;
    bool scope_was_removed = false;
    presentation.set_completion_handler([&](MessageBoxResult result) {
        ++completions;
        CK_CHECK(result == MessageBoxResult::Cancel);
        scope_was_removed = !f.app.is_modal();
    });

    f.app.post([&] { CK_CHECK(f.desktop->remove_window(box) != nullptr); });
    CK_CHECK(!presentation.completed());
    CK_CHECK(f.app.is_modal());
    f.app.step(0);

    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result() == MessageBoxResult::Cancel);
    CK_CHECK(completions == 1);
    CK_CHECK(scope_was_removed);
    CK_CHECK(f.desktop->windows().empty());
    CK_CHECK(!f.app.is_modal());

    f.app.step(0);
    CK_CHECK(completions == 1);
}

CK_TEST(quit_sweep_completes_every_standard_presentation_after_all_windows_detach) {
    Fixture f;
    auto fs = sample_file_system();
    auto help = sample_help();

    auto message = present_message_box(f.app, *f.desktop, f.roles, confirmation());
    auto file = present_file_dialog(FileDialogMode::Open, "/home/user", fs, f.app, *f.desktop, f.roles);
    auto directory = present_directory_picker(fs, "/home/user", f.app, *f.desktop, f.roles);
    auto window_list = present_window_list_dialog(*f.desktop, f.app, f.roles);
    auto viewer = present_help_viewer(help, "intro", f.app, *f.desktop, f.roles);

    CK_CHECK(f.app.execute_command(standard(f.app).quit));
    CK_CHECK(f.app.quit_requested());
    CK_CHECK(!message.completed());
    CK_CHECK(!file.completed());
    CK_CHECK(!directory.completed());
    CK_CHECK(!window_list.completed());
    CK_CHECK(!viewer.completed());

    f.app.step(0);

    CK_CHECK(f.desktop->windows().empty());
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(message.result() == MessageBoxResult::Cancel);
    CK_CHECK(file.result().has_value());
    CK_CHECK(!file.result()->accepted);
    CK_CHECK(file.result()->path.empty());
    CK_CHECK(directory.result().has_value());
    CK_CHECK(!directory.result()->accepted);
    CK_CHECK(directory.result()->path.empty());
    CK_CHECK(window_list.result() == WindowListDialogResult::Closed);
    CK_CHECK(viewer.result() == HelpViewerResult::Closed);
}

CK_TEST(an_accepted_dialog_reports_each_check_field_beside_its_text_fields) {
    // The end a caller actually touches: a settings form asks one yes/no
    // question and reads the answer back at that field's own index.
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.title = "Settings";
    descriptor.fields.push_back(FieldDescriptor{"&Name:", "kept", nullptr});
    descriptor.fields.push_back(FieldDescriptor{
        .label = "&Enabled", .kind = ckv::widgets::FieldKind::Check, .initial_checked = true});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});

    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    f.terminal.inject_bytes("\r", 0);
    CK_CHECK(f.app.step(0));

    CK_CHECK(presentation.completed());
    CK_CHECK((presentation.result() ==
              DialogResult{true, {"kept", ""}, {false, true}, {-1, -1}, {std::nullopt, std::nullopt}}));
}

CK_TEST(a_cancelled_dialog_reports_no_check_state_either) {
    // Cancellation is one result, not a partly-filled one: no text escapes a
    // cancelled form, and neither does the box the reader ticked.
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.title = "Settings";
    descriptor.fields.push_back(FieldDescriptor{
        .label = "&Enabled", .kind = ckv::widgets::FieldKind::Check, .initial_checked = true});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});

    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    CK_CHECK(f.desktop->remove_window(f.desktop->windows().back()) != nullptr);

    // The default result carries an empty `checked`, so equality with it is
    // exactly the claim that no box state escaped.
    CK_CHECK(presentation.result() == DialogResult{});
}

CK_TEST(a_form_asks_for_one_of_several_alternatives_and_reads_the_answer_back) {
    // FieldKind::Radio. The three printer modes are the case this exists for:
    // a reader choosing between them should see all three without opening
    // anything, and the form should say which one they chose.
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.title = "Printer";
    descriptor.fields.push_back(FieldDescriptor{.label = "&When a program prints:",
                                                 .kind = ckv::widgets::FieldKind::Radio,
                                                 .options = {"Ask", "Capture", "Off"},
                                                 .initial_selection = 1});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});

    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    f.terminal.inject_bytes("\r", 0);
    CK_CHECK(f.app.step(0));
    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result()->accepted);
    CK_CHECK(presentation.result()->selected == std::vector<int>{1});
    // The other vectors still carry an entry for this field, so a caller
    // indexes any of them by field position without asking what kind it was.
    CK_CHECK(presentation.result()->values == std::vector<std::string>{""});
    CK_CHECK(presentation.result()->checked == std::vector<bool>{false});
}

CK_TEST(a_radio_group_reports_what_the_reader_moved_to_rather_than_what_it_opened_with) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{.label = "&Mode:",
                                                 .kind = ckv::widgets::FieldKind::Radio,
                                                 .options = {"txt", "ansi"},
                                                 .initial_selection = 0});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    f.terminal.inject_bytes("\x1b[B", 0);  // Down: the second choice
    CK_CHECK(f.app.step(0));
    f.terminal.inject_bytes("\r", 0);
    CK_CHECK(f.app.step(0));
    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result()->selected == std::vector<int>{1});
}

CK_TEST(a_combo_field_answers_with_its_index_and_its_text) {
    // FieldKind::Combo, for a list too long to show at once. Both answers are
    // filled: an index for what was picked, and the text for a caller that
    // would rather have the word than the position.
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{.label = "&Theme:",
                                                 .kind = ckv::widgets::FieldKind::Combo,
                                                 .options = {"dark", "light", "mono"},
                                                 .initial_selection = 2});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    f.terminal.inject_bytes("\r", 0);
    CK_CHECK(f.app.step(0));
    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result()->selected == std::vector<int>{2});
    CK_CHECK(presentation.result()->values == std::vector<std::string>{"mono"});
}

CK_TEST(a_number_field_refuses_to_accept_what_is_not_a_number) {
    // FieldKind::Number. The check runs at accept time, like every other
    // validator: a field mid-edit is expected to be transiently invalid, and
    // a form that refuses the keystroke cannot be corrected from the middle.
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{.label = "&Megapixels:",
                                                 .initial_text = "sixty",
                                                 .kind = ckv::widgets::FieldKind::Number,
                                                 .minimum = 1,
                                                 .maximum = 4096});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    f.terminal.inject_bytes("\r", 0);
    CK_CHECK(f.app.step(0));
    // Vetoed: the dialog is still open, which is what the reader needs in
    // order to fix the field.
    CK_CHECK(!presentation.completed());
    CK_CHECK(f.app.is_modal());
}

CK_TEST(a_number_field_hands_over_a_number_rather_than_text_to_parse_again) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{.label = "&Megapixels:",
                                                 .initial_text = "128",
                                                 .kind = ckv::widgets::FieldKind::Number,
                                                 .minimum = 1,
                                                 .maximum = 4096});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    f.terminal.inject_bytes("\r", 0);
    CK_CHECK(f.app.step(0));
    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result()->accepted);
    CK_CHECK(presentation.result()->numbers.size() == 1U);
    if (presentation.result()->numbers.size() == 1U) {
        CK_CHECK(presentation.result()->numbers[0].has_value());
        if (presentation.result()->numbers[0]) CK_CHECK(*presentation.result()->numbers[0] == 128);
    }
    // The text is there too, for a caller that wants what was typed.
    CK_CHECK(presentation.result()->values == std::vector<std::string>{"128"});
}

CK_TEST(a_number_outside_its_bounds_is_refused_like_any_other_bad_value) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{.label = "&Megapixels:",
                                                 .initial_text = "99999",
                                                 .kind = ckv::widgets::FieldKind::Number,
                                                 .minimum = 1,
                                                 .maximum = 4096});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});
    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    f.terminal.inject_bytes("\r", 0);
    CK_CHECK(f.app.step(0));
    CK_CHECK(!presentation.completed());
}

CK_TEST(a_typed_form_mixes_every_field_kind_and_keeps_each_answer_at_its_own_index) {
    // The shape a real settings dialog has, and the promise the parallel
    // vectors make: whatever kind a field is, its answer is at its index.
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.title = "Printer Settings";
    descriptor.fields.push_back(FieldDescriptor{"&Folder:", "/tmp", nullptr});
    descriptor.fields.push_back(FieldDescriptor{
        .label = "&Ask for a filename", .kind = ckv::widgets::FieldKind::Check, .initial_checked = true});
    descriptor.fields.push_back(FieldDescriptor{.label = "&Mode:",
                                                 .kind = ckv::widgets::FieldKind::Radio,
                                                 .options = {"ask", "capture", "off"},
                                                 .initial_selection = 0});
    descriptor.fields.push_back(FieldDescriptor{.label = "&Format:",
                                                 .kind = ckv::widgets::FieldKind::Combo,
                                                 .options = {"txt", "ansi"},
                                                 .initial_selection = 1});
    descriptor.fields.push_back(FieldDescriptor{.label = "&Spool limit (KB):",
                                                 .initial_text = "1024",
                                                 .kind = ckv::widgets::FieldKind::Number,
                                                 .minimum = 1});
    descriptor.fields.push_back(FieldDescriptor{
        .label = "Nothing is ever sent to a device.", .kind = ckv::widgets::FieldKind::Note});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", ButtonRole::Accept, nullptr});

    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    f.terminal.inject_bytes("\r", 0);
    CK_CHECK(f.app.step(0));
    CK_CHECK(presentation.completed());
    const DialogResult result = *presentation.result();
    CK_CHECK(result.accepted);
    CK_CHECK(result.values.size() == 6U);
    CK_CHECK(result.checked.size() == 6U);
    CK_CHECK(result.selected.size() == 6U);
    CK_CHECK(result.numbers.size() == 6U);
    if (result.values.size() == 6U) {
        CK_CHECK(result.values[0] == "/tmp");
        CK_CHECK(result.checked[1]);
        CK_CHECK(result.selected[2] == 0);
        CK_CHECK(result.selected[3] == 1);
        CK_CHECK(result.values[3] == "ansi");
        CK_CHECK(result.numbers[4].has_value());
        if (result.numbers[4]) CK_CHECK(*result.numbers[4] == 1024);
        // The note answered nothing, and said so in every vector.
        CK_CHECK(result.values[5].empty());
        CK_CHECK(!result.checked[5]);
        CK_CHECK(result.selected[5] == -1);
        CK_CHECK(!result.numbers[5].has_value());
    }
}

CK_TEST(a_dropped_presentation_withdraws_its_handler_while_a_kept_one_still_completes) {
    Fixture f;
    int dropped_completions = 0;
    int kept_completions = 0;

    // Two dialogs, identical but for what the caller does with the handle.
    auto kept = present_message_box(f.app, *f.desktop, f.roles, confirmation());
    auto* const kept_box = f.desktop->windows().back();
    kept.set_completion_handler([&](MessageBoxResult) { ++kept_completions; });

    Window* dropped_box = nullptr;
    {
        auto dropped = present_message_box(f.app, *f.desktop, f.roles, confirmation());
        dropped_box = f.desktop->windows().back();
        dropped.set_completion_handler([&](MessageBoxResult) { ++dropped_completions; });
    }  // the caller declined the completion; the dialog is still on screen

    CK_CHECK(f.desktop->remove_window(dropped_box) != nullptr);
    CK_CHECK(f.desktop->remove_window(kept_box) != nullptr);
    f.app.step(0);

    // The negative claim, and the positive one that proves the completion
    // path itself still runs — without it this test would pass just as
    // happily if detaching stopped completing anything at all.
    CK_CHECK(dropped_completions == 0);
    CK_CHECK(kept_completions == 1);
}

CK_TEST(an_owner_destroyed_before_its_application_is_not_called_back_during_teardown) {
    // The shape that made capture_editor_screenshots die roughly two runs
    // in five: an object narrower-lived than the Application registers a
    // completion handler that captures it, then the Application's own
    // destructor detaches the dialog and completes the presentation. The
    // handler must already be gone by then — a raw `bool alive` here is
    // what `this` was in the real one.
    Fixture f;
    bool called_after_owner_died = false;

    struct Owner {
        bool* flag;
        std::optional<ckv::widgets::MessageBoxPresentation> presentation;
    };

    {
        Owner owner{&called_after_owner_died, std::nullopt};
        owner.presentation.emplace(
            present_message_box(f.app, *f.desktop, f.roles, confirmation()));
        owner.presentation->set_completion_handler(
            [&owner](MessageBoxResult) { *owner.flag = true; });
        // The dialog outlives the owner: nothing removes it here, exactly
        // as nothing removed the editor's confirmation box before main
        // returned.
    }

    // Standing in for ~Application: detach every remaining window.
    while (!f.desktop->windows().empty())
        CK_CHECK(f.desktop->remove_window(f.desktop->windows().back()) != nullptr);
    f.app.step(0);

    CK_CHECK(!called_after_owner_died);
}
