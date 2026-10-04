// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_terminal.hpp"
#include "cvision/term/windows_terminal_subsession.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/terminal_view.hpp"
#include "cvision/testing/cktest.hpp"
#include "scratch_directory.hpp"

namespace {
using ckv::term::TerminalSubsessionState;
using ckv::term::WindowsTerminalSubsession;

std::string screen_text(const ckv::term::TerminalSnapshot& snapshot) {
    std::string text;
    for (const ckv::Cell& cell : snapshot.cell_buffer)
        if (!cell.is_continuation()) text += cell.grapheme();
    return text;
}

ckv::term::TerminalLaunchSpec child(std::vector<std::string> arguments, ckv::Size cells = {48, 8}) {
    ckv::term::TerminalLaunchSpec spec =
        ckv::term::TerminalLaunchSpec::program(CKV_WINDOWS_TERMINAL_CHILD_PATH, std::move(arguments));
    spec.working_directory = std::filesystem::path(CKV_WINDOWS_TERMINAL_CHILD_PATH).parent_path().string();
    spec.profile.cells = cells;
    spec.exit_policy = ckv::term::TerminalExitPolicy::TerminateAfterGrace;
    return spec;
}

void wait_for_readiness(ckv::term::TerminalSubsession& session) {
    std::array<HANDLE, 4> native{};
    DWORD count = 0;
    for (const ckv::term::WaitHandle handle : session.wait_handles()) {
        if (handle.kind == ckv::term::WaitHandleKind::WindowsHandle)
            native[count++] = reinterpret_cast<HANDLE>(handle.value);
    }
    if (count > 0) (void)::WaitForMultipleObjects(count, native.data(), FALSE, 20);
    else ::Sleep(1);
}

bool pump_until(WindowsTerminalSubsession& session, std::string_view marker, int guard_ms = 10'000) {
    const auto guard = std::chrono::steady_clock::now() + std::chrono::milliseconds(guard_ms);
    while (std::chrono::steady_clock::now() < guard) {
        wait_for_readiness(session);
        (void)session.drain(16 * 1024);
        if (screen_text(session.snapshot()).find(marker) != std::string::npos) return true;
        if (session.state() == TerminalSubsessionState::Failed) return false;
    }
    return false;
}

bool pump_until_exit(WindowsTerminalSubsession& session, int guard_ms = 15'000) {
    const auto guard = std::chrono::steady_clock::now() + std::chrono::milliseconds(guard_ms);
    while (std::chrono::steady_clock::now() < guard) {
        wait_for_readiness(session);
        (void)session.drain(16 * 1024);
        if (session.state() == TerminalSubsessionState::Exited) return true;
        if (session.state() == TerminalSubsessionState::Failed) return false;
    }
    return false;
}

std::string utf8_path(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

std::filesystem::path path_from_utf8(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

ckv::term::TerminalLaunchSpec cmd_spec(std::optional<std::string> command = std::nullopt) {
    std::array<wchar_t, 32768> system{};
    const UINT length = ::GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size()));
    CK_CHECK(length > 0 && length < system.size());
    const auto directory = std::filesystem::path(std::wstring(system.data(), length < system.size() ? length : 0));
    auto spec = ckv::term::TerminalLaunchSpec::windows_command_processor(
        utf8_path(directory / L"cmd.exe"), std::move(command));
    spec.working_directory = utf8_path(directory);
    spec.profile.cells = {120, 12};
    spec.exit_policy = ckv::term::TerminalExitPolicy::TerminateAfterGrace;
    return spec;
}
}  // namespace

CK_TEST(windows_outer_terminal_refuses_non_console_handles_with_an_actionable_diagnostic) {
    HANDLE input = nullptr;
    HANDLE output = nullptr;
    const BOOL created = ::CreatePipe(&input, &output, nullptr, 0);
    CK_CHECK(created);
    if (!created) return;

    const UINT input_cp = ::GetConsoleCP();
    const UINT output_cp = ::GetConsoleOutputCP();
    ckv::term::WindowsClock clock;
    std::string diagnostic;
    try {
        ckv::term::WindowsTerminal terminal(clock, output, input);
    } catch (const std::runtime_error& error) {
        diagnostic = error.what();
    }
    CK_CHECK(diagnostic.find("stdin and stdout must be attached to a VT-capable console") !=
             std::string::npos);
    CK_CHECK(::GetConsoleCP() == input_cp);
    CK_CHECK(::GetConsoleOutputCP() == output_cp);
    (void)::CloseHandle(output);
    (void)::CloseHandle(input);
}

CK_TEST(windows_outer_terminal_negotiates_kitty_with_a_declared_private_host) {
    auto session = WindowsTerminalSubsession::launch(child({"outer-probe"}));
    CK_CHECK(session->state() != TerminalSubsessionState::Failed);
    CK_CHECK(pump_until_exit(*session, 8'000));
    CK_CHECK(session->exit_code() == 0);
    CK_CHECK(screen_text(session->snapshot()).find("OUTER-PROBE-OK") != std::string::npos);
}

CK_TEST(windows_outer_terminal_expires_probes_and_restores_on_a_silent_host) {
    auto spec = child({"outer-no-replies"});
    spec.profile.query_policy = ckv::term::TerminalQueryPolicy::NoResponse;
    auto session = WindowsTerminalSubsession::launch(std::move(spec));
    CK_CHECK(session->state() != TerminalSubsessionState::Failed);
    CK_CHECK(pump_until_exit(*session, 8'000));
    CK_CHECK(session->exit_code() == 0);
    CK_CHECK(screen_text(session->snapshot()).find("OUTER-PROBE-OK") != std::string::npos);
}

CK_TEST(windows_outer_terminal_delivers_verified_pixel_and_cell_mouse_over_conpty) {
    auto spec = child({"outer-pixel-mouse"});
    spec.profile.query_policy = ckv::term::TerminalQueryPolicy::NoResponse;
    auto session = WindowsTerminalSubsession::launch(std::move(spec));
    CK_CHECK(session->state() != TerminalSubsessionState::Failed);
    CK_CHECK(pump_until(*session, "PIXEL-MOUSE-READY"));
    // These are host replies to the published DECRPM 1016 and XTWINOPS 16
    // queries. Their order is deliberately reversed; neither alone proves
    // that pixel coordinates can safely be mapped to cells.
    session->send_input("\x1B[?1016;1$y\x1B[6;20;10t");
    CK_CHECK(pump_until(*session, "PIXEL-MOUSE-ARMED"));
    // Coordinates stay inside the 48x8 cell grid so only verified mode 1016
    // can distinguish them from ordinary SGR cell reports.
    session->send_input("\x1B[<0;31;5M\x1B[<32;32;5M\x1B[<0;32;5m");
    CK_CHECK(pump_until_exit(*session, 8'000));
    CK_CHECK(session->exit_code() == 0);
    CK_CHECK(screen_text(session->snapshot()).find("PIXEL-MOUSE-OK") != std::string::npos);
}

CK_TEST(windows_outer_terminal_keeps_cell_mouse_on_a_plain_vt_conpty) {
    auto spec = child({"outer-plain-vt-mouse"});
    spec.profile.query_policy = ckv::term::TerminalQueryPolicy::NoResponse;
    auto session = WindowsTerminalSubsession::launch(std::move(spec));
    CK_CHECK(session->state() != TerminalSubsessionState::Failed);
    CK_CHECK(pump_until(*session, "PLAIN-VT-MOUSE-READY"));
    session->send_input("\x1B[<0;10;5M\x1B[<0;10;5m");
    CK_CHECK(pump_until_exit(*session, 8'000));
    CK_CHECK(session->exit_code() == 0);
    CK_CHECK(screen_text(session->snapshot()).find("PLAIN-VT-MOUSE-OK") != std::string::npos);
}

CK_TEST(windows_outer_terminal_imports_unicode_bracketed_paste_over_conpty) {
    auto session = WindowsTerminalSubsession::launch(child({"outer-paste"}));
    CK_CHECK(session->state() != TerminalSubsessionState::Failed);
    CK_CHECK(pump_until(*session, "OUTER-PASTE-READY"));
    session->send_input("\x1B[200~clipboard text \xCE\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80\x1B[201~");
    CK_CHECK(pump_until_exit(*session, 8'000));
    CK_CHECK(session->exit_code() == 0);
    CK_CHECK(screen_text(session->snapshot()).find("OUTER-PASTE-OK") != std::string::npos);
}

CK_TEST(windows_outer_terminal_recovers_previously_verified_graphics_after_silent_resize) {
    auto spec = child({"outer-resize"});
    spec.profile.query_policy = ckv::term::TerminalQueryPolicy::NoResponse;
    auto session = WindowsTerminalSubsession::launch(std::move(spec));
    CK_CHECK(session->state() != TerminalSubsessionState::Failed);
    CK_CHECK(pump_until(*session, "OUTER-RESIZE-READY"));
    session->send_input("\x1B[?1;4c");  // Initial DA1 proof; the resize probe stays silent.
    CK_CHECK(pump_until(*session, "OUTER-RESIZE-ARMED"));
    session->resize({57, 10}, {9, 18});
    CK_CHECK(pump_until_exit(*session, 8'000));
    CK_CHECK(session->exit_code() == 0);
    CK_CHECK(screen_text(session->snapshot()).find("OUTER-RESIZE-OK") != std::string::npos);
}

CK_TEST(windows_conpty_child_output_and_exit_status_are_private) {
    auto session = WindowsTerminalSubsession::launch(child({"output"}));
    CK_CHECK(session->state() != TerminalSubsessionState::Failed);
    CK_CHECK(pump_until_exit(*session));
    CK_CHECK(screen_text(session->snapshot()).find("PRIVATE-HELLO") != std::string::npos);
    CK_CHECK(session->exit_code() == 7);
    CK_CHECK(session->wait_handles().empty());
}

CK_TEST(windows_conpty_input_and_two_sessions_remain_isolated) {
    auto first = WindowsTerminalSubsession::launch(child({"echo"}));
    auto second = WindowsTerminalSubsession::launch(child({"echo"}));
    CK_CHECK(pump_until(*first, "READY>"));
    CK_CHECK(pump_until(*second, "READY>"));
    first->send_input("first\r");
    second->send_input("second\r");
    CK_CHECK(pump_until_exit(*first));
    CK_CHECK(pump_until_exit(*second));
    CK_CHECK(screen_text(first->snapshot()).find("ECHO:first") != std::string::npos);
    CK_CHECK(screen_text(first->snapshot()).find("ECHO:second") == std::string::npos);
    CK_CHECK(screen_text(second->snapshot()).find("ECHO:second") != std::string::npos);
    CK_CHECK(screen_text(second->snapshot()).find("ECHO:first") == std::string::npos);
}

CK_TEST(windows_conpty_child_sees_resize_and_alternate_buffer) {
    auto resized = WindowsTerminalSubsession::launch(child({"resize", "57", "10"}, {40, 8}));
    CK_CHECK(pump_until(*resized, "SIZE:40x8"));
    resized->resize({57, 10}, {9, 18});
    resized->send_input("\r");
    CK_CHECK(pump_until_exit(*resized));
    CK_CHECK(resized->exit_code() == 0);
    CK_CHECK(screen_text(resized->snapshot()).find("SIZE:57x10") != std::string::npos);

    auto alternate = WindowsTerminalSubsession::launch(child({"alternate"}));
    CK_CHECK(pump_until(*alternate, "ALT-SCREEN"));
    CK_CHECK(alternate->snapshot().alternate_buffer);
    alternate->send_input("\r");
    CK_CHECK(pump_until_exit(*alternate));
    CK_CHECK(!alternate->snapshot().alternate_buffer);
    CK_CHECK(screen_text(alternate->snapshot()).find("PRIMARY-DONE") != std::string::npos);
}

CK_TEST(windows_conpty_argv_and_environment_are_explicit) {
    auto spec = child({"arguments", "a \"quoted\" path\\"});
    spec.environment_policy = ckv::term::TerminalEnvironmentPolicy::ExplicitOnly;
    spec.environment = {{"CKV_CHILD_TEST", "override"}};
    auto session = WindowsTerminalSubsession::launch(std::move(spec));
    CK_CHECK(pump_until_exit(*session));
    const std::string screen = screen_text(session->snapshot());
    CK_CHECK(screen.find("ARG:a \"quoted\" path\\") != std::string::npos);
    CK_CHECK(screen.find("ENV:override") != std::string::npos);
}

CK_TEST(windows_conpty_crt_roundtrip_preserves_every_argument_and_filename_argv0) {
    const std::vector<std::string> arguments{"argv-all", "", "é 日本 😀 & ^ %", "a\"quoted\"\\", "tail\\"};
    auto spec = child(arguments, {200, 16});
    spec.argv0 = "C:\\日本\\";
    auto session = WindowsTerminalSubsession::launch(std::move(spec));
    CK_CHECK(pump_until_exit(*session));
    CK_CHECK(session->exit_code() == 0);
    const std::string screen = screen_text(session->snapshot());
    CK_CHECK(screen.find("ARGC:6") != std::string::npos);
    CK_CHECK(screen.find("ARG0:433a5ce697a5e69cac5c:END") != std::string::npos);
    CK_CHECK(screen.find("ARG1:617267762d616c6c:END") != std::string::npos);
    CK_CHECK(screen.find("ARG2::END") != std::string::npos);
    CK_CHECK(screen.find("ARG3:c3a920e697a5e69cac20f09f98802026205e2025:END") != std::string::npos);
    CK_CHECK(screen.find("ARG4:612271756f746564225c:END") != std::string::npos);
    CK_CHECK(screen.find("ARG5:7461696c5c:END") != std::string::npos);
}

CK_TEST(windows_conpty_refuses_an_unencodable_filename_argv0_before_starting) {
    auto spec = child({"argv-all"});
    spec.argv0 = "ambiguous\"image";
    auto session = WindowsTerminalSubsession::launch(std::move(spec));
    CK_CHECK(session->state() == TerminalSubsessionState::Failed);
    CK_CHECK(!session->diagnostics().empty());
    if (!session->diagnostics().empty())
        CK_CHECK(session->diagnostics().front().message.find("encode") != std::string::npos);
}

CK_TEST(windows_command_processor_preserves_builtin_command_quotes_and_unicode) {
    for (const auto& marker : {std::string("CMD-BASIC"), std::string("CMD-\"two words\""),
                                     std::string("CMD-Gr\u00fc\u03b2")}) {
        auto session = WindowsTerminalSubsession::launch(cmd_spec("echo " + marker));
        CK_CHECK(pump_until_exit(*session));
        CK_CHECK(session->exit_code() == 0);
        CK_CHECK(screen_text(session->snapshot()).find(marker) != std::string::npos);
    }
}

CK_TEST(windows_command_processor_runs_a_quoted_unicode_executable_path_and_operators) {
    ckv::testing::ScratchDirectory scratch("cmd-command");
    const auto directory = scratch.path() / L"quoted & Unicode \u00fc";
    std::filesystem::create_directory(directory);
    const auto executable = directory / L"child with spaces.exe";
    std::filesystem::copy_file(path_from_utf8(CKV_WINDOWS_TERMINAL_CHILD_PATH), executable);
    auto spec = cmd_spec("\"" + utf8_path(executable) +
        "\" arguments \"DATA two words\" && echo CMD-OPERATOR-OK");
    spec.environment = {{"CKV_CHILD_TEST", "cmd-override"}};
    auto session = WindowsTerminalSubsession::launch(std::move(spec));
    CK_CHECK(pump_until_exit(*session));
    CK_CHECK(session->exit_code() == 0);
    const auto screen = screen_text(session->snapshot());
    CK_CHECK(screen.find("ARG:DATA two words") != std::string::npos);
    CK_CHECK(screen.find("ENV:cmd-override") != std::string::npos);
    CK_CHECK(screen.find("CMD-OPERATOR-OK") != std::string::npos);
}

CK_TEST(windows_command_processor_reports_the_actual_command_exit_status) {
    auto session = WindowsTerminalSubsession::launch(cmd_spec("exit /b 37"));
    CK_CHECK(pump_until_exit(*session));
    CK_CHECK(session->exit_code() == 37);
}

CK_TEST(windows_command_processor_distinguishes_interactive_from_an_empty_command) {
    auto interactive = cmd_spec();
    CK_CHECK(interactive.windows_command.has_value());
    if (!interactive.windows_command) return;
    CK_CHECK(!interactive.windows_command->command);
    auto session = WindowsTerminalSubsession::launch(std::move(interactive));
    CK_CHECK(session->state() != TerminalSubsessionState::Failed);
    // Change the environment of the running child, not just its launch inputs.
    session->resize({80, 20}, {9, 18});
    CK_CHECK(session->snapshot().cells == ckv::Size(80, 20));
    session->send_input("echo CMD-INTERACTIVE & exit 19\r");
    CK_CHECK(pump_until_exit(*session));
    CK_CHECK(session->exit_code() == 19);
    CK_CHECK(screen_text(session->snapshot()).find("CMD-INTERACTIVE") != std::string::npos);

    auto empty = cmd_spec(std::string{});
    CK_CHECK(empty.windows_command.has_value());
    if (!empty.windows_command) return;
    CK_CHECK(empty.windows_command->command.has_value());
    auto one_shot = WindowsTerminalSubsession::launch(std::move(empty));
    CK_CHECK(pump_until_exit(*one_shot));
    CK_CHECK(one_shot->exit_code() == 0);
}

CK_TEST(windows_command_processor_image_separators_cannot_be_parsed_as_switches) {
    ckv::testing::ScratchDirectory scratch("cmd-image-separators");
    const auto system = cmd_spec();
    const auto system_image = path_from_utf8(system.executable);
    for (const auto& name : {std::wstring(L"klaunch ASCII plain"), std::wstring(L"klaunch ASCII & quoted"),
                            std::wstring(L"klaunch Unicode \u00fc\u65e5\u672c\u8a9e plain"),
                            std::wstring(L"klaunch Unicode \u00fc\u65e5\u672c\u8a9e & quoted")}) {
        const auto directory = scratch.path() / name;
        std::filesystem::create_directory(directory);
        const auto image = directory / L"cmd.exe";
        std::filesystem::copy_file(system_image, image);
        // A relocated cmd needs its installed MUI resources, not only its PE image.
        std::size_t resources = 0;
        for (const auto& entry : std::filesystem::directory_iterator(system_image.parent_path())) {
            std::error_code error;
            if (!entry.is_directory(error) || error) continue;
            const auto resource = entry.path() / L"cmd.exe.mui";
            if (!std::filesystem::is_regular_file(resource, error) || error) continue;
            const auto destination = directory / entry.path().filename();
            std::filesystem::create_directory(destination);
            std::filesystem::copy_file(resource, destination / L"cmd.exe.mui");
            ++resources;
        }
        CK_CHECK(resources > 0);
        for (const bool forward_slashes : {false, true}) {
            std::string executable = utf8_path(image);
            if (forward_slashes) std::replace(executable.begin(), executable.end(), '\\', '/');
            for (const bool interactive : {false, true}) {
                auto spec = ckv::term::TerminalLaunchSpec::windows_command_processor(executable,
                    interactive ? std::nullopt : std::optional<std::string>("echo CMD-SEPARATORS & exit /b 37"));
                spec.working_directory = utf8_path(directory);
                spec.profile.cells = {120, 12};
                spec.exit_policy = ckv::term::TerminalExitPolicy::TerminateAfterGrace;
                auto session = WindowsTerminalSubsession::launch(std::move(spec));
                if (interactive) session->send_input("echo CMD-SEPARATORS & exit 37\r");
                CK_CHECK(pump_until_exit(*session));
                CK_CHECK(session->exit_code() == 37);
                CK_CHECK(screen_text(session->snapshot()).find("CMD-SEPARATORS") != std::string::npos);
            }
        }
    }
}

CK_TEST(windows_command_processor_refuses_invalid_and_oversized_text_before_spawning) {
    for (const auto& command : {std::string("\xc3\x28"), std::string("exit\0x", 6),
                               std::string(32767, 'x')}) {
        auto session = WindowsTerminalSubsession::launch(cmd_spec(command));
        CK_CHECK(session->state() == TerminalSubsessionState::Failed);
        CK_CHECK(session->process_id() < 0);
        CK_CHECK(session->wait_handles().empty());
        CK_CHECK(!session->diagnostics().empty());
    }
}

CK_TEST(windows_command_processor_refuses_an_ambiguous_argv_spec_before_spawning) {
    for (const bool interactive : {false, true}) {
        for (const bool argv0 : {false, true}) {
            auto spec = interactive ? cmd_spec() : cmd_spec("exit /b 0");
            if (argv0) spec.argv0 = "custom-name";
            else spec.arguments = {"unexpected-argument"};
            auto session = WindowsTerminalSubsession::launch(std::move(spec));
            CK_CHECK(session->state() == TerminalSubsessionState::Failed);
            CK_CHECK(session->process_id() < 0);
            CK_CHECK(session->wait_handles().empty());
            CK_CHECK(!session->diagnostics().empty());
            if (!session->diagnostics().empty())
                CK_CHECK(session->diagnostics().front().message.find("cannot be combined") != std::string::npos);
        }
    }
}

CK_TEST(windows_powershell_command_keeps_the_ordinary_argument_vector_contract) {
    auto spec = cmd_spec("unused");
    const auto directory = path_from_utf8(spec.working_directory);
    spec.executable = utf8_path(directory / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe");
    spec.windows_command.reset();
    spec.arguments = {"-NoLogo", "-NoProfile", "-Command", "Write-Output 'PS-\"two words\"'"};
    auto session = WindowsTerminalSubsession::launch(std::move(spec));
    CK_CHECK(pump_until_exit(*session));
    CK_CHECK(session->exit_code() == 0);
    CK_CHECK(screen_text(session->snapshot()).find("PS-\"two words\"") != std::string::npos);
}

CK_TEST(windows_conpty_nested_ckvision_application_receives_input_inside_the_parent_view) {
    ckv::term::HeadlessTerminal outer(ckv::Size{80, 24});
    ckv::ManualClock clock;
    ckv::ui::Application app(outer, clock);
    auto spec = ckv::term::TerminalLaunchSpec::program(CKV_NESTED_TERMINAL_CHILD_PATH);
    spec.profile.cells = {40, 12};
    spec.exit_policy = ckv::term::TerminalExitPolicy::TerminateAfterGrace;
    ckv::term::TerminalSubsession& session = app.launch_terminal_subsession(std::move(spec));
    CK_CHECK(session.state() != TerminalSubsessionState::Failed);
    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    view->set_fills_root(false);
    view->set_bounds(ckv::Rect{2, 2, 40, 12});
    ckv::widgets::TerminalView* const terminal_view = view.get();
    app.root().add_child(std::move(view));
    app.set_focus(terminal_view);
    (void)app.step(0);

    std::vector<ckv::Cell> outside_baseline;
    outside_baseline.reserve(80U * 24U);
    for (int row = 0; row < 24; ++row)
        for (int column = 0; column < 80; ++column)
            outside_baseline.push_back(app.current_frame().at(ckv::Point{column, row}));

    const auto frame_contains = [&app](std::string_view needle) {
        std::string content;
        for (int row = 2; row < 14; ++row)
            for (int column = 2; column < 42; ++column)
                content += app.current_frame().at(ckv::Point{column, row}).grapheme();
        return content.find(needle) != std::string::npos;
    };
    const auto pump_frame_until = [&](std::string_view needle) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
        while (std::chrono::steady_clock::now() < deadline) {
            wait_for_readiness(session);
            (void)app.step(0);
            if (frame_contains(needle)) return true;
            if (session.state() == TerminalSubsessionState::Failed) return false;
        }
        return false;
    };

    CK_CHECK(pump_frame_until("NESTED-CKVISION"));
    outer.inject_event(ckv::TextEvent{"go"});
    (void)app.step(0);
    CK_CHECK(pump_frame_until("NESTED-INPUT-OK"));
    bool leaked = false;
    for (int row = 0; row < 24; ++row) {
        for (int column = 0; column < 80; ++column) {
            const bool inside = column >= 2 && column < 42 && row >= 2 && row < 14;
            if (!inside && app.current_frame().at(ckv::Point{column, row}) !=
                               outside_baseline[static_cast<std::size_t>(row * 80 + column)])
                leaked = true;
        }
    }
    CK_CHECK(!leaked);
    outer.inject_event(ckv::TextEvent{"q"});
    (void)app.step(0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (std::chrono::steady_clock::now() < deadline &&
           session.state() != TerminalSubsessionState::Exited &&
           session.state() != TerminalSubsessionState::Failed) {
        wait_for_readiness(session);
        (void)app.step(0);
    }
    CK_CHECK(session.state() == TerminalSubsessionState::Exited);
    CK_CHECK(session.status().exit_code == 0);
}

CK_TEST(windows_conpty_two_ckvision_applications_keep_input_and_lifetime_isolated) {
    const auto launch = [] {
        auto spec = ckv::term::TerminalLaunchSpec::program(CKV_NESTED_TERMINAL_CHILD_PATH);
        spec.profile.cells = {40, 12};
        spec.exit_policy = ckv::term::TerminalExitPolicy::TerminateAfterGrace;
        return WindowsTerminalSubsession::launch(std::move(spec));
    };
    auto first = launch();
    auto second = launch();
    CK_CHECK(first->state() != TerminalSubsessionState::Failed);
    CK_CHECK(second->state() != TerminalSubsessionState::Failed);
    CK_CHECK(pump_until(*first, "NESTED-CKVISION"));
    CK_CHECK(pump_until(*second, "NESTED-CKVISION"));

    first->send_input("go");
    CK_CHECK(pump_until(*first, "NESTED-INPUT-OK"));
    (void)second->drain(16 * 1024);
    CK_CHECK(screen_text(second->snapshot()).find("NESTED-INPUT-OK") == std::string::npos);
    CK_CHECK(second->state() == TerminalSubsessionState::Running);

    second->send_input("go");
    CK_CHECK(pump_until(*second, "NESTED-INPUT-OK"));
    first->send_input("q");
    CK_CHECK(pump_until_exit(*first));
    CK_CHECK(first->exit_code() == 0);
    CK_CHECK(second->state() == TerminalSubsessionState::Running);
    CK_CHECK(screen_text(second->snapshot()).find("NESTED-INPUT-OK") != std::string::npos);

    second->send_input("q");
    CK_CHECK(pump_until_exit(*second));
    CK_CHECK(second->exit_code() == 0);
}

CK_TEST(windows_conpty_child_sixel_degrades_under_an_explicit_no_graphics_profile) {
    auto text_only = child({"sixel"});
    text_only.profile.sixel = false;
    auto session = WindowsTerminalSubsession::launch(std::move(text_only));
    CK_CHECK(pump_until_exit(*session));
    const auto snapshot = session->snapshot();
    CK_CHECK(screen_text(snapshot).find("GRAPHIC-DONE") != std::string::npos);
    CK_CHECK(snapshot.rasters.empty());
}

CK_TEST(windows_conpty_child_sixel_matches_the_effective_host_profile) {
    auto session = WindowsTerminalSubsession::launch(child({"sixel"}));
    CK_CHECK(pump_until_exit(*session));
    const auto snapshot = session->snapshot();
    CK_CHECK(screen_text(snapshot).find("GRAPHIC-DONE") != std::string::npos);
    const char* const require_modern = std::getenv("CKVISION_EXPECT_MODERN_CONPTY");
    if (require_modern != nullptr && std::string_view(require_modern) == "1")
        CK_CHECK(session->profile().sixel);
    if (session->profile().sixel) {
        CK_CHECK(snapshot.rasters.size() == 1U);
        if (snapshot.rasters.size() == 1U) {
            CK_CHECK(snapshot.rasters[0].image != nullptr);
            const auto pixel = snapshot.rasters[0].image->pixel(0, 0);
            CK_CHECK(pixel.r == 255 && pixel.g == 0 && pixel.b == 0 && pixel.a == 255);
        }
    } else {
        CK_CHECK(snapshot.rasters.empty());
    }
}

CK_TEST(windows_conpty_concurrent_graphics_survive_independent_resize_storms) {
    auto red = WindowsTerminalSubsession::launch(child({"sixel-hold", "red", "55", "10"}));
    auto green = WindowsTerminalSubsession::launch(child({"sixel-hold", "green", "44", "9"}));
    CK_CHECK(red->state() != TerminalSubsessionState::Failed);
    CK_CHECK(green->state() != TerminalSubsessionState::Failed);
    CK_CHECK(red->process_id() != green->process_id());
    red->set_raster_identity(310);
    green->set_raster_identity(311);
    CK_CHECK(pump_until(*red, "RED-READY"));
    CK_CHECK(pump_until(*green, "GREEN-READY"));

    const bool graphics = red->profile().sixel;
    CK_CHECK(green->profile().sixel == graphics);
    const char* const require_modern = std::getenv("CKVISION_EXPECT_MODERN_CONPTY");
    if (require_modern != nullptr && std::string_view(require_modern) == "1") CK_CHECK(graphics);
    const std::array<ckv::Size, 5> red_sizes{{{40, 7}, {27, 6}, {58, 11}, {37, 8}, {55, 10}}};
    const std::array<ckv::Size, 5> green_sizes{{{42, 7}, {31, 6}, {60, 12}, {36, 8}, {44, 9}}};
    for (std::size_t index = 0; index < red_sizes.size(); ++index) {
        red->resize(red_sizes[index], {8 + static_cast<int>(index), 16 + static_cast<int>(index)});
        green->resize(green_sizes[index], {9 + static_cast<int>(index), 18 + static_cast<int>(index)});
        (void)red->drain(4096);
        (void)green->drain(4096);
    }
    CK_CHECK(red->state() != TerminalSubsessionState::Failed);
    CK_CHECK(green->state() != TerminalSubsessionState::Failed);
    CK_CHECK((red->profile().cells == ckv::Size{55, 10}));
    CK_CHECK((green->profile().cells == ckv::Size{44, 9}));
    CK_CHECK((red->profile().cell_pixels == ckv::PixelSize{12, 20}));
    CK_CHECK((green->profile().cell_pixels == ckv::PixelSize{13, 22}));
    // ConPTY repaints each resized buffer asynchronously. A single drain after
    // the last resize can sample the screen mid-repaint, cleared but not yet
    // redrawn, which is what a loaded host showed. The claim is about the
    // settled screen, so wait for each child's text to be back.
    CK_CHECK(pump_until(*red, "RED-READY"));
    CK_CHECK(pump_until(*green, "GREEN-READY"));

    const auto check_raster = [graphics](const ckv::term::TerminalSnapshot& snapshot,
                                         int identity, ckv::Image::Rgba expected) {
        if (!graphics) {
            CK_CHECK(snapshot.rasters.empty());
            return;
        }
        CK_CHECK(snapshot.rasters.size() == 1U);
        if (snapshot.rasters.size() != 1U || snapshot.rasters[0].image == nullptr) return;
        CK_CHECK(snapshot.rasters[0].id == identity);
        const auto pixel = snapshot.rasters[0].image->pixel(0, 0);
        CK_CHECK(pixel.r == expected.r && pixel.g == expected.g &&
                 pixel.b == expected.b && pixel.a == 255);
    };
    check_raster(red->snapshot(), 310, ckv::Image::Rgba{255, 0, 0, 255});
    check_raster(green->snapshot(), 311, ckv::Image::Rgba{0, 255, 0, 255});

    red->send_input("red-storm\r");
    green->send_input("green-storm\r");
    CK_CHECK(pump_until_exit(*red, 8'000));
    CK_CHECK(pump_until_exit(*green, 8'000));
    CK_CHECK(red->exit_code() == 0);
    CK_CHECK(green->exit_code() == 0);
    const auto red_final = red->snapshot();
    const auto green_final = green->snapshot();
    CK_CHECK((red_final.cells == ckv::Size{55, 10}));
    CK_CHECK((green_final.cells == ckv::Size{44, 9}));
    CK_CHECK(red_final.cursor.position.x >= 0 && red_final.cursor.position.x < red_final.cells.width);
    CK_CHECK(red_final.cursor.position.y >= 0 && red_final.cursor.position.y < red_final.cells.height);
    CK_CHECK(green_final.cursor.position.x >= 0 && green_final.cursor.position.x < green_final.cells.width);
    CK_CHECK(green_final.cursor.position.y >= 0 && green_final.cursor.position.y < green_final.cells.height);
    CK_CHECK(screen_text(red_final).find("SIZE:55x10") != std::string::npos);
    CK_CHECK(screen_text(green_final).find("SIZE:44x9") != std::string::npos);
    CK_CHECK(screen_text(red_final).find("RED-ECHO:red-storm") != std::string::npos);
    CK_CHECK(screen_text(green_final).find("GREEN-ECHO:green-storm") != std::string::npos);
    CK_CHECK(screen_text(red_final).find("GREEN-ECHO") == std::string::npos);
    CK_CHECK(screen_text(green_final).find("RED-ECHO") == std::string::npos);
    check_raster(red_final, 310, ckv::Image::Rgba{255, 0, 0, 255});
    check_raster(green_final, 311, ckv::Image::Rgba{0, 255, 0, 255});
}

CK_TEST(windows_conpty_graphics_sessions_survive_peer_close_and_reallocate) {
    auto red = WindowsTerminalSubsession::launch(child({"sixel-hold", "red"}));
    auto green = WindowsTerminalSubsession::launch(child({"sixel-hold", "green"}));
    CK_CHECK(red->state() != TerminalSubsessionState::Failed);
    CK_CHECK(green->state() != TerminalSubsessionState::Failed);
    CK_CHECK(red->process_id() != green->process_id());
    CK_CHECK(pump_until(*red, "RED-READY"));
    CK_CHECK(pump_until(*green, "GREEN-READY"));
    CK_CHECK(screen_text(red->snapshot()).find("GREEN-READY") == std::string::npos);
    CK_CHECK(screen_text(green->snapshot()).find("RED-READY") == std::string::npos);

    const bool graphics = red->profile().sixel;
    CK_CHECK(green->profile().sixel == graphics);
    const char* const require_modern = std::getenv("CKVISION_EXPECT_MODERN_CONPTY");
    if (require_modern != nullptr && std::string_view(require_modern) == "1") CK_CHECK(graphics);
    const auto check_raster = [graphics](const ckv::term::TerminalSnapshot& snapshot,
                                         ckv::Image::Rgba expected) {
        if (!graphics) {
            CK_CHECK(snapshot.rasters.empty());
            return;
        }
        CK_CHECK(snapshot.rasters.size() == 1U);
        if (snapshot.rasters.size() != 1U || snapshot.rasters[0].image == nullptr) return;
        const auto pixel = snapshot.rasters[0].image->pixel(0, 0);
        CK_CHECK(pixel.r == expected.r && pixel.g == expected.g &&
                 pixel.b == expected.b && pixel.a == 255);
    };
    check_raster(red->snapshot(), ckv::Image::Rgba{255, 0, 0, 255});
    check_raster(green->snapshot(), ckv::Image::Rgba{0, 255, 0, 255});

    const HANDLE red_process = ::OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(red->process_id()));
    CK_CHECK(red_process != nullptr);
    red->close();
    CK_CHECK(red->state() == TerminalSubsessionState::Closed);
    if (red_process != nullptr) {
        CK_CHECK(::WaitForSingleObject(red_process, 0) == WAIT_OBJECT_0);
        (void)::CloseHandle(red_process);
    }
    CK_CHECK(green->state() != TerminalSubsessionState::Failed);
    check_raster(green->snapshot(), ckv::Image::Rgba{0, 255, 0, 255});
    green->send_input("survivor\r");
    CK_CHECK(pump_until_exit(*green, 8'000));
    CK_CHECK(green->exit_code() == 0);
    CK_CHECK(screen_text(green->snapshot()).find("GREEN-ECHO:survivor") != std::string::npos);

    auto replacement = WindowsTerminalSubsession::launch(child({"sixel-hold", "red"}));
    CK_CHECK(replacement->state() != TerminalSubsessionState::Failed);
    CK_CHECK(pump_until(*replacement, "RED-READY"));
    CK_CHECK(replacement->profile().sixel == graphics);
    check_raster(replacement->snapshot(), ckv::Image::Rgba{255, 0, 0, 255});
    replacement->send_input("reused\r");
    CK_CHECK(pump_until_exit(*replacement, 8'000));
    CK_CHECK(replacement->exit_code() == 0);
    CK_CHECK(screen_text(replacement->snapshot()).find("RED-ECHO:reused") != std::string::npos);
}

CK_TEST(windows_conpty_abrupt_child_exit_preserves_peer_and_reallocates) {
    auto abrupt = WindowsTerminalSubsession::launch(child({"sixel-abrupt-exit", "red"}));
    auto peer = WindowsTerminalSubsession::launch(child({"sixel-hold", "green"}));
    CK_CHECK(abrupt->state() != TerminalSubsessionState::Failed);
    CK_CHECK(peer->state() != TerminalSubsessionState::Failed);
    CK_CHECK(abrupt->process_id() != peer->process_id());
    CK_CHECK(pump_until(*abrupt, "RED-READY"));
    CK_CHECK(pump_until(*peer, "GREEN-READY"));
    const bool graphics = abrupt->profile().sixel;
    CK_CHECK(peer->profile().sixel == graphics);
    const char* const require_modern = std::getenv("CKVISION_EXPECT_MODERN_CONPTY");
    if (require_modern != nullptr && std::string_view(require_modern) == "1") CK_CHECK(graphics);
    const auto check_raster = [graphics](const ckv::term::TerminalSnapshot& snapshot,
                                         ckv::Image::Rgba expected) {
        if (!graphics) {
            CK_CHECK(snapshot.rasters.empty());
            return;
        }
        CK_CHECK(snapshot.rasters.size() == 1U);
        if (snapshot.rasters.size() != 1U || snapshot.rasters[0].image == nullptr) return;
        const auto pixel = snapshot.rasters[0].image->pixel(0, 0);
        CK_CHECK(pixel.r == expected.r && pixel.g == expected.g &&
                 pixel.b == expected.b && pixel.a == 255);
    };
    check_raster(abrupt->snapshot(), ckv::Image::Rgba{255, 0, 0, 255});
    check_raster(peer->snapshot(), ckv::Image::Rgba{0, 255, 0, 255});

    const HANDLE terminated_process =
        ::OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(abrupt->process_id()));
    CK_CHECK(terminated_process != nullptr);
    abrupt->send_input("crash\r");
    CK_CHECK(pump_until_exit(*abrupt, 8'000));
    CK_CHECK(abrupt->exit_code() == 37);
    if (terminated_process != nullptr) {
        CK_CHECK(::WaitForSingleObject(terminated_process, 0) == WAIT_OBJECT_0);
        (void)::CloseHandle(terminated_process);
    }
    CK_CHECK(peer->state() != TerminalSubsessionState::Failed);
    check_raster(peer->snapshot(), ckv::Image::Rgba{0, 255, 0, 255});
    abrupt->close();
    CK_CHECK(abrupt->wait_handles().empty());

    auto replacement = WindowsTerminalSubsession::launch(child({"sixel-hold", "red"}));
    CK_CHECK(replacement->state() != TerminalSubsessionState::Failed);
    CK_CHECK(pump_until(*replacement, "RED-READY"));
    CK_CHECK(replacement->process_id() != peer->process_id());
    CK_CHECK(replacement->profile().sixel == graphics);
    check_raster(replacement->snapshot(), ckv::Image::Rgba{255, 0, 0, 255});

    peer->send_input("still-here\r");
    replacement->send_input("fresh\r");
    CK_CHECK(pump_until_exit(*peer, 8'000));
    CK_CHECK(pump_until_exit(*replacement, 8'000));
    CK_CHECK(peer->exit_code() == 0);
    CK_CHECK(replacement->exit_code() == 0);
    CK_CHECK(screen_text(peer->snapshot()).find("GREEN-ECHO:still-here") != std::string::npos);
    CK_CHECK(screen_text(replacement->snapshot()).find("RED-ECHO:fresh") != std::string::npos);
}

CK_TEST(windows_conpty_recovers_after_a_sixel_payload_limit_without_touching_a_graphics_peer) {
    ckv::term::TerminalSubsessionOptions options;
    options.max_graphics_payload_bytes = 128;
    auto limited = WindowsTerminalSubsession::launch(child({"sixel-limit-recover"}), options);
    auto peer = WindowsTerminalSubsession::launch(child({"sixel-hold", "green"}));
    CK_CHECK(limited->state() != TerminalSubsessionState::Failed);
    CK_CHECK(peer->state() != TerminalSubsessionState::Failed);
    CK_CHECK(limited->process_id() != peer->process_id());
    CK_CHECK(pump_until(*limited, "REJECTED-READY"));
    CK_CHECK(pump_until(*peer, "GREEN-READY"));
    const bool graphics = limited->profile().sixel;
    CK_CHECK(peer->profile().sixel == graphics);
    const char* const require_modern = std::getenv("CKVISION_EXPECT_MODERN_CONPTY");
    if (require_modern != nullptr && std::string_view(require_modern) == "1") CK_CHECK(graphics);
    CK_CHECK(limited->snapshot().rasters.empty());
    if (graphics) {
        bool exceeded = false;
        for (const auto& diagnostic : limited->diagnostics())
            if (diagnostic.kind == ckv::term::TerminalDiagnostic::Kind::LimitExceeded &&
                diagnostic.message == "child Sixel payload exceeded configured limit")
                exceeded = true;
        CK_CHECK(exceeded);
        const auto before = peer->snapshot();
        CK_CHECK(before.rasters.size() == 1U);
        if (before.rasters.size() == 1U && before.rasters[0].image != nullptr) {
            const auto pixel = before.rasters[0].image->pixel(0, 0);
            CK_CHECK(pixel.r == 0 && pixel.g == 255 && pixel.b == 0 && pixel.a == 255);
        }
    }

    limited->send_input("continue\r");
    CK_CHECK(pump_until_exit(*limited, 8'000));
    CK_CHECK(limited->exit_code() == 0);
    CK_CHECK(screen_text(limited->snapshot()).find("RECOVERED:continue") != std::string::npos);
    const auto recovered = limited->snapshot();
    if (graphics) {
        CK_CHECK(recovered.rasters.size() == 1U);
        if (recovered.rasters.size() == 1U && recovered.rasters[0].image != nullptr) {
            const auto pixel = recovered.rasters[0].image->pixel(0, 0);
            CK_CHECK(pixel.r == 255 && pixel.g == 0 && pixel.b == 0 && pixel.a == 255);
        }
    } else CK_CHECK(recovered.rasters.empty());
    CK_CHECK(peer->state() != TerminalSubsessionState::Failed);
    peer->send_input("peer-ok\r");
    CK_CHECK(pump_until_exit(*peer, 8'000));
    CK_CHECK(peer->exit_code() == 0);
    CK_CHECK(screen_text(peer->snapshot()).find("GREEN-ECHO:peer-ok") != std::string::npos);
}

CK_TEST(windows_conpty_bounded_drains_preserve_flood_tail) {
    auto session = WindowsTerminalSubsession::launch(child({"flood"}));
    CK_CHECK(pump_until_exit(*session, 30'000));
    CK_CHECK(screen_text(session->snapshot()).find("FLOOD-TAIL") != std::string::npos);
}

CK_TEST(windows_conpty_bounded_close_ends_an_uncooperative_child) {
    auto session = WindowsTerminalSubsession::launch(child({"linger"}));
    CK_CHECK(pump_until(*session, "LINGER-READY"));
    const HANDLE child_process = ::OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(session->process_id()));
    CK_CHECK(child_process != nullptr);
    const auto started = std::chrono::steady_clock::now();
    session->close();
    const auto elapsed = std::chrono::steady_clock::now() - started;
    CK_CHECK(elapsed < std::chrono::seconds(5));
    CK_CHECK(::WaitForSingleObject(child_process, 0) == WAIT_OBJECT_0);
    CK_CHECK(session->state() == TerminalSubsessionState::Closed);
    session->close();
    (void)::CloseHandle(child_process);
}

CK_TEST(windows_portable_termination_keeps_a_peer_responsive_until_job_escalation) {
    auto stubborn = WindowsTerminalSubsession::launch(child({"linger-tree"}));
    auto peer = WindowsTerminalSubsession::launch(child({"echo"}));
    CK_CHECK(pump_until(*stubborn, "LINGER-READY"));
    ckv::term::TerminalSubsession& portable = *stubborn;
    const auto resources = portable.process_resources();
    CK_CHECK(resources.state == ckv::core::ProcessResourceState::Available);
    CK_CHECK(resources.cpu_scope == ckv::core::ProcessCpuScope::OwnedJobLifetime);
    CK_CHECK(resources.live_processes == 2);
    CK_CHECK(resources.rss_bytes.has_value());
    CK_CHECK(resources.private_rss_bytes.has_value());
    const HANDLE process = ::OpenProcess(SYNCHRONIZE, FALSE,
                                         static_cast<DWORD>(portable.process_id()));
    CK_CHECK(process != nullptr);
    if (process == nullptr) return;
    const std::string text = screen_text(portable.snapshot());
    const auto marker = text.find("DESCENDANT:");
    CK_CHECK(marker != std::string::npos);
    DWORD descendant_id = 0;
    if (marker != std::string::npos) {
        const char* const first = text.data() + marker + std::string_view("DESCENDANT:").size();
        const auto parsed = std::from_chars(first, text.data() + text.size(), descendant_id);
        CK_CHECK(parsed.ec == std::errc{} && descendant_id != 0);
    }
    const HANDLE descendant = descendant_id == 0 ? nullptr :
        ::OpenProcess(SYNCHRONIZE, FALSE, descendant_id);
    CK_CHECK(descendant != nullptr);
    portable.request_termination();
    portable.request_termination();
    // The child deliberately ignores Control-C. Requests leave its private
    // transport open and the owning loop free to serve another real child.
    peer->send_input("peer-still-live\r");
    CK_CHECK(pump_until_exit(*peer));
    CK_CHECK(screen_text(peer->snapshot()).find("peer-still-live") != std::string::npos);
    CK_CHECK(::WaitForSingleObject(process, 0) == WAIT_TIMEOUT);
    if (descendant != nullptr) CK_CHECK(::WaitForSingleObject(descendant, 0) == WAIT_TIMEOUT);
    CK_CHECK(!portable.wait_handles().empty());
    portable.request_kill();
    portable.request_kill();
    CK_CHECK(pump_until_exit(*stubborn));
    CK_CHECK(::WaitForSingleObject(process, 0) == WAIT_OBJECT_0);
    if (descendant != nullptr) {
        CK_CHECK(::WaitForSingleObject(descendant, 5'000) == WAIT_OBJECT_0);
        (void)::CloseHandle(descendant);
    }
    CK_CHECK(portable.status().exit_code == 1);
    const auto exited_resources = portable.process_resources();
    CK_CHECK(portable.process_id() == -1);
    CK_CHECK(exited_resources.cpu_time_nanos.has_value());
    CK_CHECK(exited_resources.live_processes == 0);
    CK_CHECK(exited_resources.rss_bytes == 0);
    portable.close();
    CK_CHECK(portable.process_resources().state == ckv::core::ProcessResourceState::Gone);
    portable.close();
    portable.request_termination();
    portable.request_kill();
    CK_CHECK(portable.wait_handles().empty());
    (void)::CloseHandle(process);
}

CK_TEST(windows_portable_selective_snapshot_omits_unrequested_payloads) {
    auto concrete = WindowsTerminalSubsession::launch(child({"output"}));
    CK_CHECK(pump_until_exit(*concrete));
    ckv::term::TerminalSubsession& portable = *concrete;
    const auto selected = portable.snapshot({.include_scrollback = false,
                                             .include_rasters = false});
    CK_CHECK(!selected.cell_buffer.empty());
    CK_CHECK(selected.scrollback.empty());
    CK_CHECK(selected.rasters.empty());
    CK_CHECK(selected.cells == portable.status().cells);
    CK_CHECK(selected.state == TerminalSubsessionState::Exited);
    CK_CHECK(!portable.snapshot().cell_buffer.empty());
}

CK_TEST(windows_conpty_launch_failure_has_no_native_wait_sources) {
    auto spec = child({"output"});
    spec.executable = "C:\\ckvision\\missing-child.exe";
    auto session = WindowsTerminalSubsession::launch(std::move(spec));
    CK_CHECK(session->state() == TerminalSubsessionState::Failed);
    CK_CHECK(session->wait_handles().empty());
    CK_CHECK(!session->diagnostics().empty());
    ckv::term::TerminalSubsession& portable = *session;
    portable.request_termination();
    portable.request_kill();
    CK_CHECK(portable.state() == TerminalSubsessionState::Failed);
}

#endif
