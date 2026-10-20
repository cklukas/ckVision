// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cvision/widgets/log_controller.hpp>
#include <cvision/ui/application.hpp>
#include <cvision/ui/standard_roles.hpp>
#include <cvision/term/log_file_service.hpp>
#include <cvision/term/terminal_clipboard.hpp>
#include <limits>
#include <iostream>
#if defined(_WIN32)
#include <cvision/term/windows_clock.hpp>
#include <cvision/term/windows_terminal.hpp>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cvision/term/posix_clock.hpp>
#include <cvision/term/posix_terminal.hpp>
#endif
namespace {
int monitor(std::string path) {
    using namespace ckv;
#if defined(_WIN32)
    term::WindowsClock clock; term::WindowsTerminal terminal(clock);
#else
    term::PosixClock clock; term::PosixTerminal terminal(clock);
#endif
    auto wake = std::make_shared<term::TerminalLogWake>(terminal);
    auto activity = std::make_shared<LogActivity>(wake);
    term::NativeLogFileService files(1, kLogBatchBytes, term::LogWatchPreference::Native, activity);
    widgets::LogModel model; widgets::LogFilter filter(model); widgets::LogSearch search(filter);
    auto session = files.open_session(); if (!session) return 1;
    widgets::LogFollower follower(model, std::move(session));
    if (!follower.start(path, 1, clock.now_nanos())) { std::cerr << "An absolute log path is required.\n"; return 2; }
    term::TerminalClipboardWriter clipboard(terminal);
    // Application/views die before their borrowed models; services enclose sessions.
    ui::Application app(terminal, clock, clipboard);
    app.set_frame_delivery_policy(ui::FrameDeliveryPolicy::Bounded);
    const auto roles = ui::intern_standard_roles(app.roles()); app.set_theme(ui::make_classic_theme(app.roles(), roles));
    auto* panel = app.root().make<widgets::LogPanel>(filter); panel->set_search(&search);
    widgets::LogController controller({}, activity.get());
    const auto lane = controller.attach({&model, &filter, &search, panel, &follower, nullptr});
    if (!lane) return 1;
    panel->on_query_changed = [&](const std::string& text) { widgets::LogFilterQuery query; query.include = text; filter.set_query(std::move(query)); controller.kick(lane); };
    app.commands().declare({.key = "logclient.quit", .title = "Quit", .chord = "Ctrl+q", .handler = [&] { app.request_quit(); }});
    app.set_focus(&panel->view());
    while (!app.quit_requested()) {
        controller.step(clock.now_nanos());
        app.step(controller.next_deadline().value_or(std::numeric_limits<std::int64_t>::max()));
    }
    controller.detach(lane); follower.stop(); return 0;
}
}
#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    const std::wstring_view input(argv[1]);
    if (input.size() > 32768) return 2;
    const auto size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return 2;
    std::string path(static_cast<std::size_t>(size), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()), path.data(), size, nullptr, nullptr) != size) return 2;
    try { return monitor(std::move(path)); } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
#else
int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: log_file_monitor /absolute/log/path (use the Quit command to exit)\n"; return 2; }
    try { return monitor(argv[1]); } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
#endif
