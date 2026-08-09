// Copyright (c) 2026 C. Klukas. All rights reserved.
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

#include "cktest.hpp"
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

namespace {

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
    descriptor.buttons.push_back(ButtonDescriptor{"OK", true, [&] { default_pressed = true; }});

    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    CK_CHECK(f.app.is_modal());
    CK_CHECK(!presentation.completed());
    f.terminal.inject_bytes("host\r", 0);
    CK_CHECK(f.app.step(0));
    CK_CHECK(default_pressed);
    // The self-detach is deferred until the button callback unwinds, then
    // drained by this same step's ordinary posted-work phase.
    CK_CHECK((presentation.result() == DialogResult{true, {"localhost"}}));
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.desktop->windows().empty());
}

CK_TEST(descriptor_dialog_external_detach_and_host_quit_return_cancellation) {
    Fixture f;
    DialogDescriptor descriptor;
    descriptor.fields.push_back(FieldDescriptor{"&Name:", "value", nullptr});
    descriptor.buttons.push_back(ButtonDescriptor{"OK", true, nullptr});

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
    descriptor.buttons.push_back(ButtonDescriptor{"OK", true, [&] {
        std::unique_ptr<Window> removed = f.desktop->remove_window(dialog);
        CK_CHECK(removed.get() == dialog);
        removed.reset();
    }});

    auto presentation = present_dialog(std::move(descriptor), f.app, *f.desktop, f.roles);
    dialog = f.desktop->windows().back();
    f.terminal.inject_bytes("\r", 0);
    CK_CHECK(f.app.step(0));
    CK_CHECK((presentation.result() == DialogResult{true, {"value"}}));
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

    CK_CHECK(f.app.execute_command(ckv::ui::std_commands::kQuit));
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
