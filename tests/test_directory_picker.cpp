// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/directory_picker.hpp"

#include "cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"

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
    auto handle = make_directory_picker(fs, "/a", f.theme, f.roles, app, nullptr, nullptr);
    CK_CHECK(handle.window != nullptr);
    CK_CHECK(handle.initial_focus != nullptr);
}

CK_TEST(construction_for_a_nonexistent_root_does_not_crash) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    MemoryFileSystem fs;  // "/nowhere" was never added
    auto handle = make_directory_picker(fs, "/nowhere", f.theme, f.roles, app, nullptr, nullptr);
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
    auto handle = make_directory_picker(fs, "/a", f.theme, f.roles, app, nullptr,
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
    auto handle = make_directory_picker(fs, "/a", f.theme, f.roles, app, nullptr,
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
    auto handle = make_directory_picker(fs, "/a", f.theme, f.roles, app, nullptr,
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
    auto handle = make_directory_picker(fs, "/a", f.theme, f.roles, app, nullptr,
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

CK_TEST(files_are_excluded_from_the_tree_only_subdirectories_appear) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();  // "/a" also contains a file, notadir.txt
    std::string chosen;
    auto handle = make_directory_picker(fs, "/a", f.theme, f.roles, app, nullptr,
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
    auto handle = make_directory_picker(fs, "/a", f.theme, f.roles, app, invoker, nullptr);
    Window* window_ptr = handle.window.get();
    app.root().add_child(std::move(handle.window));

    window_ptr->cancel_request();
    CK_CHECK(app.focused() == invoker);
}
