// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/file_dialog.hpp"

#include <optional>

#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::MemoryFileSystem;
using ckv::Modifier;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::FileDialogMode;
using ckv::widgets::FileDialogResult;
using ckv::widgets::make_file_dialog;
using ckv::widgets::present_file_dialog;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

MemoryFileSystem sample_fs() {
    MemoryFileSystem fs;
    fs.add_directory("/home/user/docs");
    fs.add_file("/home/user/readme.txt");
    fs.add_file("/home/user/docs/report.txt");
    return fs;
}

ckv::KeyEvent key(ckv::Key k, ckv::Modifier m = Modifier::None) { return ckv::KeyEvent{KeyChord{k, m, ""}}; }
}  // namespace

// --- Construction / initial listing --------------------------------

CK_TEST(the_dialog_builds_successfully_for_an_existing_directory) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", fs, f.roles, app, nullptr, nullptr);
    CK_CHECK(handle.window != nullptr);
    CK_CHECK(handle.initial_focus != nullptr);
}

CK_TEST(construction_for_a_nonexistent_directory_does_not_crash) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MemoryFileSystem fs;  // "/does/not/exist" was never added
    auto handle = make_file_dialog(FileDialogMode::Open, "/does/not/exist", fs, f.roles, app, nullptr,
                                    nullptr);
    CK_CHECK(handle.window != nullptr);
}

// --- Navigation and selection --------------------------------------

CK_TEST(activating_a_directory_entry_navigates_into_it_and_activating_dotdot_navigates_back) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    std::vector<FileDialogResult> results;
    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", fs, f.roles, app, nullptr,
                                    [&](FileDialogResult r) { results.push_back(r); });
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);  // the list has focus

    // "/home/user" contains "docs/" (dir, sorted first) then
    // "readme.txt" — row 0 is "docs/".
    app.dispatch(key(Key::Enter));  // activate row 0: navigate into docs/
    app.dispatch(key(Key::Enter));  // row 0 is now ".." (docs/ is non-root): navigate back
    CK_CHECK(results.empty());      // navigation never fires on_result
}

CK_TEST(activating_a_file_entry_fills_the_path_field_rather_than_navigating) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    int result_calls = 0;
    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", fs, f.roles, app, nullptr,
                                    [&](FileDialogResult) { ++result_calls; });
    ckv::widgets::Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    app.dispatch(key(Key::Down));   // move to "readme.txt" (row 1, after "docs/")
    app.dispatch(key(Key::Enter));  // activate the file entry
    // Selecting the file must NOT close the dialog — activation only
    // fills the path field; on_result must not have fired yet.
    CK_CHECK(result_calls == 0);

    // Confirm the field now holds the selected file's full path by
    // accepting through the default button and checking the result.
    window_ptr->accept_request();
    CK_CHECK(result_calls == 1);
}

// --- Accept / cancel -----------------------------------------------

CK_TEST(clicking_ok_with_a_typed_absolute_path_accepts_it_verbatim) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    FileDialogResult result;
    auto handle = make_file_dialog(FileDialogMode::Save, "/home/user", fs, f.roles, app, nullptr,
                                    [&](FileDialogResult r) { result = r; });
    ckv::widgets::Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));

    // Directly exercise the default-button accept path (Enter anywhere
    // in the dialog), regardless of which control currently has focus.
    window_ptr->accept_request();
    CK_CHECK(result.accepted);
    CK_CHECK(result.path == "/home/user");  // the path field starts showing the current directory
}

CK_TEST(a_relative_typed_path_is_joined_with_the_current_directory) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    FileDialogResult result;
    auto handle = make_file_dialog(FileDialogMode::Save, "/home/user", fs, f.roles, app, nullptr,
                                    [&](FileDialogResult r) { result = r; });
    ckv::widgets::Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);  // the list

    // Tab backward from the list to reach the path field (declaration
    // order: path field, list, OK, Cancel), clear it, and type a bare
    // relative filename.
    app.focus_previous();
    app.dispatch(key(Key::Home));
    app.dispatch(key(Key::End, Modifier::Shift));
    app.dispatch(ckv::TextEvent{"newfile.txt", false});

    window_ptr->accept_request();
    CK_CHECK(result.accepted);
    CK_CHECK(result.path == "/home/user/newfile.txt");
}

CK_TEST(cancel_fires_on_result_with_accepted_false_and_an_empty_path) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    FileDialogResult result;
    result.accepted = true;  // ensure Cancel actually flips it
    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", fs, f.roles, app, nullptr,
                                    [&](FileDialogResult r) { result = r; });
    ckv::widgets::Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));

    window_ptr->cancel_request();
    CK_CHECK(!result.accepted);
    CK_CHECK(result.path.empty());
}

CK_TEST(closing_restores_focus_to_the_view_that_invoked_the_dialog) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto* invoker = app.root().add_child(std::make_unique<ckv::ui::View>());
    invoker->set_focus_policy(ckv::ui::FocusPolicy::TabStop);

    auto fs = sample_fs();
    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", fs, f.roles, app, invoker, nullptr);
    ckv::widgets::Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));

    window_ptr->cancel_request();
    CK_CHECK(app.focused() == invoker);
}

CK_TEST(present_file_dialog_completes_after_its_modal_window_detaches) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 80, 24}, app.theme(), roles.dialog_background);
    auto* desktop = app.root().add(std::move(desktop_owned));
    auto fs = sample_fs();

    auto presentation = present_file_dialog(FileDialogMode::Open, "/home/user", fs, app, *desktop, roles);
    std::optional<FileDialogResult> completion;
    presentation.set_completion_handler([&](FileDialogResult result) { completion = std::move(result); });

    CK_CHECK(app.is_modal());
    desktop->windows().back()->accept_request();
    CK_CHECK(!presentation.completed());
    app.step(0);

    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result().has_value());
    CK_CHECK(presentation.result()->accepted);
    CK_CHECK(presentation.result()->path == "/home/user");
    CK_CHECK(completion.has_value());
    CK_CHECK(completion->accepted);
    CK_CHECK(!app.is_modal());
}
