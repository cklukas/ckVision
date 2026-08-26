// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision TODO — interactive POSIX host. Environment, user records, wall
// time, and the real filesystem are resolved here and injected inward.
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <pwd.h>
#include <unistd.h>

#include "cvision/core/version.hpp"
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_filesystem.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "json_todo_repository.hpp"
#include "memory_todo_repository.hpp"
#include "todo_app.hpp"
#include "todo_host.hpp"

namespace {

struct UserIdentity {
    std::string home;
    std::string name;
};

UserIdentity current_user() {
    UserIdentity result;
    if (const passwd* record = ::getpwuid(::getuid())) {
        if (record->pw_dir != nullptr) result.home = record->pw_dir;
        if (record->pw_name != nullptr) result.name = record->pw_name;
    }
    if (const char* home = std::getenv("HOME"); result.home.empty() && home != nullptr) result.home = home;
    if (const char* user = std::getenv("USER"); result.name.empty() && user != nullptr) result.name = user;
    if (result.name.empty()) result.name = "local-user";
    return result;
}

std::vector<std::string_view> arguments(int argc, char** argv) {
    std::vector<std::string_view> result;
    for (int index = 1; index < argc; ++index) result.emplace_back(argv[index]);
    return result;
}

int run_demo(const ckv::todo::TodoLaunchOptions& options) {
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
    ckv::term::TerminalClipboardWriter clipboard(terminal);
    ckv::ui::Application app(terminal, clock, clipboard);
    ckv::todo::FixedCalendarClock calendar(
        {ckv::todo::IsoTimestamp{"2026-08-25T12:00:00Z"},
         ckv::todo::IsoDate{"2026-08-25"},
         ckv::todo::IsoTime{"14:30"}});
    const auto guided = ckv::todo::TodoWorkspace::guided(
        {ckv::todo::IsoTimestamp{"2026-08-25T12:00:00Z"}, "demo"});
    if (!guided) {
        std::cerr << "ckvision_todo: cannot create the demo workspace: " << guided.error.diagnostic << '\n';
        return 1;
    }
    ckv::todo::MemoryTodoRepository repository(*guided.value);
    ckv::todo::TodoApp todo(app, repository, calendar, "demo",
                            {.initial_board_name = options.board_name,
                             .workspace_description = "in-memory demo workspace"});
    app.run();
    return 0;
}

int run_persistent(const ckv::todo::TodoLaunchOptions& options) {
    const UserIdentity user = current_user();
    if (!options.data_directory && user.home.empty()) {
        std::cerr << "ckvision_todo: cannot resolve a home directory; use --data-dir PATH\n";
        return 2;
    }
    const std::string directory = options.data_directory ? *options.data_directory : user.home + "/.ckvision/todo";
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
    ckv::term::TerminalClipboardWriter clipboard(terminal);
    ckv::term::PosixFileSystem filesystem;
    ckv::ui::Application app(terminal, clock, clipboard);
    ckv::todo::SystemCalendarClock calendar;
    ckv::todo::JsonTodoRepository repository(filesystem, directory);
    ckv::todo::TodoApp todo(app, repository, calendar, user.name,
                            {.initial_board_name = options.board_name,
                             .workspace_description = "persistent workspace: " + directory});
    app.run();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const auto parsed = ckv::todo::parse_todo_arguments(arguments(argc, argv));
    if (!parsed) {
        std::cerr << "ckvision_todo: " << parsed.diagnostic << "\n\n" << ckv::todo::todo_usage();
        return 2;
    }
    if (parsed.value->show_help) {
        std::cout << ckv::todo::todo_usage();
        return 0;
    }
    if (parsed.value->show_version) {
        std::cout << "ckvision_todo " << ckv::version_string() << "\n";
        return 0;
    }
    return parsed.value->demo ? run_demo(*parsed.value) : run_persistent(*parsed.value);
}
