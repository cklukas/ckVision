// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/directory_picker.hpp"

#include <optional>

#include "cvision/testing/cktest.hpp"
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
using ckv::widgets::make_directory_picker;
using ckv::widgets::exec_directory_picker;
using ckv::widgets::present_directory_picker;
using ckv::widgets::Window;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

MemoryFileSystem sample_fs() {
    MemoryFileSystem fs;
    fs.add_directory("/a/b/c");
    fs.add_directory("/a/d");
    fs.add_file("/a/notadir.txt");  // must be excluded from the tree entirely
    return fs;
}

ckv::KeyEvent key(ckv::Key k) { return ckv::KeyEvent{KeyChord{k, Modifier::None, ""}}; }
}  // namespace

CK_TEST(the_picker_builds_successfully_for_an_existing_root) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    auto handle = make_directory_picker(fs, "/a", f.roles, app, nullptr, nullptr);
    CK_CHECK(handle.window != nullptr);
    CK_CHECK(handle.initial_focus != nullptr);
}

CK_TEST(construction_for_a_nonexistent_root_does_not_crash) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MemoryFileSystem fs;  // "/nowhere" was never added
    auto handle = make_directory_picker(fs, "/nowhere", f.roles, app, nullptr, nullptr);
    CK_CHECK(handle.window != nullptr);
}

CK_TEST(accepting_the_root_without_navigating_returns_the_root_path) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    bool accepted = false;
    std::string chosen;
    auto handle = make_directory_picker(fs, "/a", f.roles, app, nullptr,
                                         [&](bool ok, std::string path) {
                                             accepted = ok;
                                             chosen = path;
                                         });
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));

    window_ptr->accept_request();
    CK_CHECK(accepted);
    CK_CHECK(chosen == "/a");
}

CK_TEST(navigating_into_a_subdirectory_and_accepting_returns_its_full_path) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    bool accepted = false;
    std::string chosen;
    auto handle = make_directory_picker(fs, "/a", f.roles, app, nullptr,
                                         [&](bool ok, std::string path) {
                                             accepted = ok;
                                             chosen = path;
                                         });
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    // "/a" has children "b" and "d" (alphabetical). Expand the root
    // (Right), then move down into "b".
    app.dispatch(key(Key::Right));  // expands "/a"
    app.dispatch(key(Key::Down));   // moves to "b"
    window_ptr->accept_request();
    CK_CHECK(accepted);
    CK_CHECK(chosen == "/a/b");
}

CK_TEST(navigating_two_levels_deep_still_returns_the_correct_full_path) {
    // A regression guard for the address-stability concern documented
    // in directory_picker.cpp: path_of's keys must remain valid TreeNode
    // addresses after the tree is moved into TreeView, at every depth,
    // not just the root's immediate children.
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    std::string chosen;
    auto handle = make_directory_picker(fs, "/a", f.roles, app, nullptr,
                                         [&](bool, std::string path) { chosen = path; });
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);

    app.dispatch(key(Key::Right));  // expand "/a"
    app.dispatch(key(Key::Down));   // move to "b"
    app.dispatch(key(Key::Right));  // expand "b"
    app.dispatch(key(Key::Down));   // move to "c"
    window_ptr->accept_request();
    CK_CHECK(chosen == "/a/b/c");
}

CK_TEST(cancel_reports_accepted_false_with_an_empty_path) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    bool accepted = true;
    std::string chosen = "unset";
    auto handle = make_directory_picker(fs, "/a", f.roles, app, nullptr,
                                         [&](bool ok, std::string path) {
                                             accepted = ok;
                                             chosen = path;
                                         });
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));

    window_ptr->cancel_request();
    CK_CHECK(!accepted);
    CK_CHECK(chosen.empty());
}

CK_TEST(a_result_callback_may_destroy_its_directory_picker) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    Window* dialog = nullptr;
    int results = 0;
    auto handle = make_directory_picker(fs, "/a", f.roles, app, nullptr,
                                        [&](bool accepted, std::string path) {
                                            CK_CHECK(!accepted);
                                            CK_CHECK(path.empty());
                                            ++results;
                                            std::unique_ptr<ckv::ui::View> detached = app.root().remove_child(dialog);
                                            CK_CHECK(detached.get() == dialog);
                                        });
    dialog = static_cast<Window*>(app.root().add_child(std::move(handle.window)));

    dialog->cancel_request();
    CK_CHECK(results == 1);
}

CK_TEST(a_directory_picker_result_callback_cannot_deliver_a_reentrant_second_result) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    Window* dialog = nullptr;
    int results = 0;
    auto handle = make_directory_picker(fs, "/a", f.roles, app, nullptr,
                                        [&](bool, std::string) {
                                            ++results;
                                            dialog->cancel_request();
                                        });
    dialog = static_cast<Window*>(app.root().add_child(std::move(handle.window)));

    dialog->cancel_request();
    CK_CHECK(results == 1);
}

CK_TEST(files_are_excluded_from_the_tree_only_subdirectories_appear) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();  // "/a" also contains a file, notadir.txt
    std::string chosen;
    auto handle = make_directory_picker(fs, "/a", f.roles, app, nullptr,
                                         [&](bool, std::string path) { chosen = path; });
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));
    app.set_focus(handle.initial_focus);
    app.dispatch(key(Key::Right));  // expand "/a" — only "b" and "d" should be reachable
    app.dispatch(key(Key::Down));   // -> "b"
    app.dispatch(key(Key::Down));   // -> "d" (would be "notadir.txt" if the file leaked into the tree)
    window_ptr->accept_request();
    CK_CHECK(chosen == "/a/d");
}

CK_TEST(closing_restores_focus_to_the_view_that_invoked_the_picker) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto* invoker = app.root().add_child(std::make_unique<ckv::ui::View>());
    invoker->set_focus_policy(ckv::ui::FocusPolicy::TabStop);

    auto fs = sample_fs();
    auto handle = make_directory_picker(fs, "/a", f.roles, app, invoker, nullptr);
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));

    window_ptr->cancel_request();
    CK_CHECK(app.focused() == invoker);
}

CK_TEST(present_directory_picker_completes_after_its_modal_window_detaches) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 80, 24});
    auto* desktop = app.root().add(std::move(desktop_owned));
    auto fs = sample_fs();

    auto presentation = present_directory_picker(fs, "/a", app, *desktop, roles);
    std::optional<ckv::widgets::DirectoryPickerResult> completion;
    presentation.set_completion_handler(
        [&](ckv::widgets::DirectoryPickerResult result) { completion = std::move(result); });

    CK_CHECK(app.is_modal());
    desktop->windows().back()->accept_request();
    CK_CHECK(!presentation.completed());
    app.step(0);

    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result().has_value());
    CK_CHECK(presentation.result()->accepted);
    CK_CHECK(presentation.result()->path == "/a");
    CK_CHECK(completion.has_value());
    CK_CHECK(completion->accepted);
    CK_CHECK(!app.is_modal());
}

CK_TEST(exec_directory_picker_returns_the_modal_result_without_leaving_a_window_attached) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 80, 24});
    auto* desktop = app.root().add(std::move(desktop_owned));
    auto fs = sample_fs();

    app.post([desktop] { desktop->windows().back()->accept_request(); });
    const auto accepted = exec_directory_picker(fs, "/a", app, *desktop, roles);
    CK_CHECK(accepted.accepted);
    CK_CHECK(accepted.path == "/a");
    CK_CHECK(desktop->windows().empty());
    CK_CHECK(!app.is_modal());

    app.post([desktop] { desktop->windows().back()->cancel_request(); });
    const auto cancelled = exec_directory_picker(fs, "/a", app, *desktop, roles);
    CK_CHECK(!cancelled.accepted);
    CK_CHECK(cancelled.path.empty());
    CK_CHECK(desktop->windows().empty());
    CK_CHECK(!app.is_modal());
}

CK_TEST(exec_directory_picker_host_quit_returns_cancellation_and_detaches_the_open_dialog) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 80, 24});
    auto* desktop = app.root().add(std::move(desktop_owned));
    auto fs = sample_fs();
    app.post([&app] { app.request_quit(); });

    const auto result = exec_directory_picker(fs, "/a", app, *desktop, roles);
    CK_CHECK(!result.accepted);
    CK_CHECK(result.path.empty());
    CK_CHECK(app.quit_requested());
    CK_CHECK(desktop->windows().empty());
    CK_CHECK(!app.is_modal());
}
