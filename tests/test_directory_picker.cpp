// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/directory_picker.hpp"

#include <optional>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "path_dialog_test_fixture.hpp"

using ckv::ManualClock;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::DirectoryPickerResult;
using ckv::widgets::exec_modal_directory_picker;
using ckv::widgets::make_directory_picker;
using ckv::widgets::present_modal_directory_picker;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

ckv::testing::ScriptedDirectoryService sample_service() {
    ckv::testing::ScriptedDirectoryService service;
    service.fs.add_directory("/a/b/c");
    service.fs.add_directory("/a/d");
    service.fs.add_file("/a/notadir.txt");
    return service;
}
}  // namespace

CK_TEST(the_picker_builds_successfully_for_an_existing_root) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto service = sample_service();
    auto handle = make_directory_picker(service, "/a", f.roles, app, nullptr, nullptr);
    CK_CHECK(handle.window != nullptr);
    CK_CHECK(handle.initial_focus != nullptr);
}

CK_TEST(construction_for_a_nonexistent_root_does_not_crash) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    Fixture f;
    auto service = sample_service();
    auto handle = make_directory_picker(service, "/nowhere", f.roles, app, nullptr, nullptr);
    CK_CHECK(handle.window != nullptr);
}

CK_TEST(directory_picker_returns_directory_picker_result_on_accept_and_cancel) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().make<ckv::widgets::Desktop>(ckv::Rect{0, 0, 80, 24});
    auto service = sample_service();

    std::vector<DirectoryPickerResult> results;
    auto handle = make_directory_picker(service, "/a", roles, app, nullptr,
                                        [&results](DirectoryPickerResult r) { results.push_back(std::move(r)); });
    desktop->present_modeless(std::move(handle), app);
    app.step(0);

    desktop->windows().back()->accept_request();
    app.step(0);
    CK_CHECK(results.size() == 1U);
    CK_CHECK(results[0].accepted);
    CK_CHECK(results[0].path == "/a");

    auto handle2 = make_directory_picker(service, "/a", roles, app, nullptr,
                                         [&results](DirectoryPickerResult r) { results.push_back(std::move(r)); });
    desktop->present_modeless(std::move(handle2), app);
    app.step(0);
    desktop->windows().back()->cancel_request();
    app.step(0);
    CK_CHECK(results.size() == 2U);
    CK_CHECK(!results[1].accepted);
    CK_CHECK(results[1].path.empty());
}

CK_TEST(present_modal_directory_picker_resolves_after_detachment) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().make<ckv::widgets::Desktop>(ckv::Rect{0, 0, 80, 24});
    auto service = sample_service();

    std::optional<DirectoryPickerResult> completion;
    auto picker = present_modal_directory_picker(service, "/a", app, *desktop, roles);
    picker.set_completion_handler([&completion](DirectoryPickerResult result) { completion = std::move(result); });
    app.step(0);

    desktop->windows().back()->accept_request();
    app.step(0);
    CK_CHECK(completion.has_value());
    CK_CHECK(completion->accepted);
    CK_CHECK(completion->path == "/a");
    CK_CHECK(!app.is_modal());
}

CK_TEST(exec_modal_directory_picker_returns_the_modal_result_without_leaving_a_window_attached) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().make<ckv::widgets::Desktop>(ckv::Rect{0, 0, 80, 24});
    auto service = sample_service();

    app.post([desktop] { desktop->windows().back()->accept_request(); });
    const auto accepted = exec_modal_directory_picker(service, "/a", app, *desktop, roles);
    CK_CHECK(accepted.accepted);
    CK_CHECK(accepted.path == "/a");
    CK_CHECK(desktop->windows().empty());
    CK_CHECK(!app.is_modal());

    app.post([desktop] { desktop->windows().back()->cancel_request(); });
    const auto cancelled = exec_modal_directory_picker(service, "/a", app, *desktop, roles);
    CK_CHECK(!cancelled.accepted);
    CK_CHECK(cancelled.path.empty());
    CK_CHECK(desktop->windows().empty());
    CK_CHECK(!app.is_modal());
}

CK_TEST(exec_modal_directory_picker_host_quit_returns_cancellation_and_detaches_the_open_dialog) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().make<ckv::widgets::Desktop>(ckv::Rect{0, 0, 80, 24});
    auto service = sample_service();
    app.post([&app] { app.request_quit(); });

    const auto result = exec_modal_directory_picker(service, "/a", app, *desktop, roles);
    CK_CHECK(!result.accepted);
    CK_CHECK(result.path.empty());
    CK_CHECK(app.quit_requested());
    CK_CHECK(desktop->windows().empty());
    CK_CHECK(!app.is_modal());
}
