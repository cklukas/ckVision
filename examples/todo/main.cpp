// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision TODO — interactive native host. Environment, user records, wall
// time, and the real filesystem are resolved here and injected inward.
#include "../native_host.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>


#include "../example_diagnostics.hpp"
#include "cvision/core/version.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "json_todo_repository.hpp"
#include "memory_todo_repository.hpp"
#include "todo_app.hpp"
#include "todo_host.hpp"

namespace {

std::vector<std::string_view> arguments(const std::vector<std::string>& args) {
    std::vector<std::string_view> result;
    for (std::size_t index = 1; index < args.size(); ++index) result.emplace_back(args[index]);
    return result;
}

int run_demo(const ckv::todo::TodoLaunchOptions& options) {
    ckv::examples::NativeClock clock;
    const ckv::examples::ExampleDiagnostics diagnostics(clock);
    ckv::examples::NativeTerminal terminal(clock);
    diagnostics.attach(terminal);
    ckv::term::TerminalClipboardWriter clipboard(terminal);
    ckv::ui::Application app(terminal, clock, clipboard);
    diagnostics.attach(app);
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
    const auto user = ckv::examples::current_user();
    if (!options.data_directory && user.home.empty()) {
        std::cerr << "ckvision_todo: cannot resolve a home directory; use --data-dir PATH\n";
        return 2;
    }
    const std::string directory = options.data_directory ? *options.data_directory : user.home + "/.ckvision/todo";
    ckv::examples::NativeClock clock;
    const ckv::examples::ExampleDiagnostics diagnostics(clock);
    ckv::examples::NativeTerminal terminal(clock);
    diagnostics.attach(terminal);
    ckv::term::TerminalClipboardWriter clipboard(terminal);
    ckv::examples::NativeFileSystem filesystem;
    ckv::ui::Application app(terminal, clock, clipboard);
    diagnostics.attach(app);
    ckv::todo::SystemCalendarClock calendar;
    ckv::todo::JsonTodoRepository repository(filesystem, directory);
    ckv::todo::TodoApp todo(app, repository, calendar, user.name,
                            {.initial_board_name = options.board_name,
                             .workspace_description = "persistent workspace: " + directory});
    app.run();
    return 0;
}

}  // namespace

int run_main(const std::vector<std::string>& args) {
    const auto parsed = ckv::todo::parse_todo_arguments(arguments(args));
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
    return ckv::examples::run_reporting_failure(
        [&] { return parsed.value->demo ? run_demo(*parsed.value) : run_persistent(*parsed.value); });
}

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    return ckv::examples::run_reporting_failure([&] { return run_main(ckv::examples::native_arguments(argc, argv)); });
}
