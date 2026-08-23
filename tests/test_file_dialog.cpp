// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/file_dialog.hpp"

#include <optional>
#include <string>
#include <string_view>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/list_view.hpp"

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
using ckv::widgets::FileDialogOptions;
using ckv::widgets::FileDialogFilter;
using ckv::widgets::FileDialogResult;
using ckv::widgets::exec_file_dialog;
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

template <class T>
T* find_descendant(ckv::ui::View& view) {
    if (auto* typed = dynamic_cast<T*>(&view)) return typed;
    for (const auto& child : view.children()) {
        if (auto* found = find_descendant<T>(*child)) return found;
    }
    return nullptr;
}

ckv::widgets::Button* find_button(ckv::ui::View& view, std::string_view text) {
    if (auto* button = dynamic_cast<ckv::widgets::Button*>(&view); button != nullptr && button->text() == text)
        return button;
    for (const auto& child : view.children()) {
        if (auto* found = find_button(*child, text)) return found;
    }
    return nullptr;
}
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

CK_TEST(filters_limit_visible_files_and_the_filter_button_cycles_filters) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    fs.add_file("/home/user/notes.md");

    FileDialogOptions options;
    options.filters = {FileDialogFilter{"Text", {".txt"}}, FileDialogFilter{"All", {}}};
    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", fs, std::move(options), f.roles, app,
                                   nullptr, nullptr);
    auto* list = static_cast<ckv::widgets::ListView*>(handle.initial_focus);
    const std::vector<std::string> text_only{"..", "docs/", "readme.txt"};
    CK_CHECK(list->items() == text_only);

    auto* filter = find_button(*handle.window, "Filter: Text");
    CK_CHECK(filter != nullptr);
    filter->on_press();
    const std::vector<std::string> all_files{"..", "docs/", "notes.md", "readme.txt"};
    CK_CHECK(list->items() == all_files);
}

CK_TEST(hidden_toggle_reveals_and_hides_dot_entries_without_host_filesystem_access) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    fs.add_file("/home/user/.secret");

    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", fs, f.roles, app, nullptr, nullptr);
    auto* list = static_cast<ckv::widgets::ListView*>(handle.initial_focus);
    const std::vector<std::string> hidden_off{"..", "docs/", "readme.txt"};
    CK_CHECK(list->items() == hidden_off);

    auto* hidden = find_button(*handle.window, "Show Hidden");
    CK_CHECK(hidden != nullptr);
    hidden->on_press();
    const std::vector<std::string> hidden_on{"..", "docs/", ".secret", "readme.txt"};
    CK_CHECK(list->items() == hidden_on);

    hidden = find_button(*handle.window, "Hide Hidden");
    CK_CHECK(hidden != nullptr);
    hidden->on_press();
    CK_CHECK(list->items() == hidden_off);
}

CK_TEST(recent_locations_are_listed_as_navigable_rows_and_accept_records_the_current_directory) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    ckv::ui::HistoryRegistry recent;
    recent.record("recent-files", "/home/user/docs");

    FileDialogOptions options;
    options.recent_locations = &recent;
    options.recent_locations_key = "recent-files";
    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", fs, std::move(options), f.roles, app,
                                   nullptr, nullptr);
    auto* list = static_cast<ckv::widgets::ListView*>(handle.initial_focus);
    CK_CHECK(list->items().front() == "Recent: /home/user/docs");

    list->on_activate(0);
    const std::vector<std::string> docs_listing{"Recent: /home/user/docs", "..", "report.txt"};
    CK_CHECK(list->items() == docs_listing);

    ckv::widgets::Window* window_ptr = handle.window.get();
    window_ptr->accept_request();
    CK_CHECK(recent.entries("recent-files").front() == "/home/user/docs");
}

CK_TEST(path_field_tab_completion_uses_the_filtered_injected_directory_listing) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();

    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", fs, f.roles, app, nullptr, nullptr);
    auto* input = find_descendant<ckv::widgets::InputLine>(*handle.window);
    CK_CHECK(input != nullptr);
    input->set_text("doc");
    CK_CHECK(input->on_key(key(Key::Tab)));
    CK_CHECK(input->text() == "/home/user/docs/");
}

CK_TEST(relative_paths_with_backslashes_are_normalized_against_the_current_directory_on_accept) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    FileDialogResult result;
    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", fs, f.roles, app, nullptr,
                                   [&](FileDialogResult r) { result = std::move(r); });
    auto* input = find_descendant<ckv::widgets::InputLine>(*handle.window);
    CK_CHECK(input != nullptr);
    input->set_text("docs\\report.txt");

    handle.window->accept_request();
    CK_CHECK(result.accepted);
    CK_CHECK(result.path == "/home/user/docs/report.txt");
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

CK_TEST(a_result_callback_may_destroy_its_file_dialog) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    ckv::widgets::Window* dialog = nullptr;
    int results = 0;
    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", fs, f.roles, app, nullptr,
                                   [&](FileDialogResult result) {
                                       CK_CHECK(!result.accepted);
                                       CK_CHECK(result.path.empty());
                                       ++results;
                                       std::unique_ptr<ckv::ui::View> detached = app.root().remove_child(dialog);
                                       CK_CHECK(detached.get() == dialog);
                                   });
    dialog = static_cast<ckv::widgets::Window*>(app.root().add_child(std::move(handle.window)));

    dialog->cancel_request();
    CK_CHECK(results == 1);
}

CK_TEST(a_file_dialog_result_callback_cannot_deliver_a_reentrant_second_result) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto fs = sample_fs();
    ckv::widgets::Window* dialog = nullptr;
    int results = 0;
    auto handle = make_file_dialog(FileDialogMode::Open, "/home/user", fs, f.roles, app, nullptr,
                                   [&](FileDialogResult) {
                                       ++results;
                                       dialog->cancel_request();
                                   });
    dialog = static_cast<ckv::widgets::Window*>(app.root().add_child(std::move(handle.window)));

    dialog->cancel_request();
    CK_CHECK(results == 1);
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
        std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 80, 24});
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

CK_TEST(presented_file_dialog_accepts_a_terminal_typed_path_inside_its_modal_scope) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 80, 24});
    auto* desktop = app.root().add(std::move(desktop_owned));
    auto fs = sample_fs();

    auto presentation = present_file_dialog(FileDialogMode::Save, "/home/user", fs, app, *desktop, roles);
    CK_CHECK(app.is_modal());

    // The initial focus is the file list. Drive Shift+Tab, Home, Delete,
    // ordinary text, and Enter as one terminal-read batch. This validates
    // the real decoder → modal router → InputLine → Window accept path, not
    // a direct TextEvent or factory callback.
    constexpr std::string_view kEraseInitialPath =
        "\x1B[3~\x1B[3~\x1B[3~\x1B[3~\x1B[3~\x1B[3~\x1B[3~\x1B[3~\x1B[3~\x1B[3~";
    term.inject_bytes("\x1B[Z\x1B[H" + std::string(kEraseInitialPath) + "newfile.txt\r", 1);
    CK_CHECK(app.step(1));

    CK_CHECK(presentation.completed());
    CK_CHECK(presentation.result().has_value());
    CK_CHECK(presentation.result()->accepted);
    CK_CHECK(presentation.result()->path == "/home/user/newfile.txt");
    CK_CHECK(desktop->windows().empty());
    CK_CHECK(!app.is_modal());
}

CK_TEST(exec_file_dialog_returns_the_modal_result_without_leaving_a_window_attached) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto desktop_owned =
        std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 80, 24});
    auto* desktop = app.root().add(std::move(desktop_owned));
    auto fs = sample_fs();

    // The outer-loop convenience pumps Application::step itself, so the
    // scripted dismissal is queued before entry and runs in that first step.
    app.post([desktop] { desktop->windows().back()->accept_request(); });
    const FileDialogResult accepted = exec_file_dialog(FileDialogMode::Open, "/home/user", fs, app, *desktop, roles);
    CK_CHECK(accepted.accepted);
    CK_CHECK(accepted.path == "/home/user");
    CK_CHECK(desktop->windows().empty());
    CK_CHECK(!app.is_modal());

    app.post([desktop] { desktop->windows().back()->cancel_request(); });
    const FileDialogResult cancelled =
        exec_file_dialog(FileDialogMode::Save, "/home/user", fs, app, *desktop, roles);
    CK_CHECK(!cancelled.accepted);
    CK_CHECK(cancelled.path.empty());
    CK_CHECK(desktop->windows().empty());
    CK_CHECK(!app.is_modal());
}

CK_TEST(exec_file_dialog_host_quit_returns_cancellation_and_detaches_the_open_dialog) {
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

    const FileDialogResult result =
        exec_file_dialog(FileDialogMode::Open, "/home/user", fs, app, *desktop, roles);
    CK_CHECK(!result.accepted);
    CK_CHECK(result.path.empty());
    CK_CHECK(app.quit_requested());
    CK_CHECK(desktop->windows().empty());
    CK_CHECK(!app.is_modal());
}
