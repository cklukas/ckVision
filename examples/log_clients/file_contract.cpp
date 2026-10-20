// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cvision/widgets/log_controller.hpp>
#include <cvision/term/headless_terminal.hpp>
#include <cvision/term/log_file_service.hpp>
#include <cvision/ui/application.hpp>
#include <cvision/ui/standard_roles.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#if defined(_WIN32)
#include <cvision/term/windows_clock.hpp>
#include <cvision/term/windows_filesystem.hpp>
using ClientClock = ckv::term::WindowsClock;
using ClientFiles = ckv::term::WindowsFileSystem;
#else
#include <cvision/term/posix_clock.hpp>
#include <cvision/term/posix_filesystem.hpp>
using ClientClock = ckv::term::PosixClock;
using ClientFiles = ckv::term::PosixFileSystem;
#endif
namespace {
bool verify(const std::filesystem::path& scratch, ckv::term::LogWatchPreference watch) {
    using namespace ckv;
    ClientClock clock; ClientFiles filesystem;
    const auto path = scratch / ("public-log-" + std::to_string(clock.now_nanos()) + ".txt");
    const auto encoded = path.generic_u8string(); const std::string name(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    if (filesystem.write_file_atomic(name, "first\n", FileWriteExpectation::must_not_exist()).status != FileWriteStatus::Ok) return false;
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); } } cleanup{path};
    auto activity = std::make_shared<LogActivity>();
    term::NativeLogFileService files(1, kLogBatchBytes, watch, activity);
    widgets::LogModel model; widgets::LogFilter filter(model);
    widgets::LogFollowOptions options; options.start = LogStartMode::Beginning; options.reconcile_nanos = 10'000'000;
    widgets::LogFollower follower(model, files.open_session(), options);
    if (!follower.start(name, 1, clock.now_nanos())) return false;
    term::HeadlessTerminal terminal({80, 25}); ui::Application app(terminal, clock);
    const auto roles = ui::intern_standard_roles(app.roles()); app.set_theme(ui::make_classic_theme(app.roles(), roles));
    auto* panel = app.root().make<widgets::LogPanel>(filter);
    widgets::LogController controller({}, activity.get());
    const auto lane = controller.attach({&model, &filter, nullptr, panel, &follower, nullptr}); if (!lane) return false;
    const auto step = [&] { controller.step(clock.now_nanos()); if (app.frames_awaiting_terminal()) terminal.inject_bytes("\x1B[0n", clock.now_nanos()); app.step(clock.now_nanos()); };
    const auto wait = [&](std::size_t count) {
        const auto deadline = clock.now_nanos() + 10'000'000'000;
        while (clock.now_nanos() < deadline) { step(); if (model.size() >= count) return true; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        return false;
    };
    bool ok = wait(1) && model.at(0)->text == "first";
    panel->view().pause(); { std::ofstream output(path, std::ios::app); output << "appended 中\n"; if (!output) ok = false; }
    ok = wait(2) && ok && model.at(1)->text == "appended 中" && !panel->view().state().following;
    panel->view().resume(); ok = ok && panel->view().state().following;
    controller.detach(lane); follower.stop(); return ok;
}
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path scratch(argv[1]);
    if (!scratch.is_absolute() || !std::filesystem::is_directory(scratch)) return 2;
    if (!verify(scratch, ckv::term::LogWatchPreference::Native) || !verify(scratch, ckv::term::LogWatchPreference::Polling)) return 1;
    std::cout << "Installed public native/polling file composition, append and live pause/resume passed.\n"; return 0;
}
