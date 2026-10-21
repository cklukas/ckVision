// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <stdexcept>
#include <vector>

#include "../example_diagnostics.hpp"
#include "cvision/term/directory_service.hpp"
#if defined(_WIN32)
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_filesystem.hpp"
#include "cvision/term/windows_terminal.hpp"
#include "cvision/term/windows_text.hpp"
#else
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_filesystem.hpp"
#include "cvision/term/posix_terminal.hpp"
#endif
#include "cvision/term/terminal_clipboard.hpp"
#include "completion_app.hpp"

namespace {
int run_completion(int argc, const char* const* argv) {
    if (argc != 3 || std::string_view(argv[1]) == "--help") {
        std::fprintf(stderr, "usage: ckvision_completion <absolute-directory> <exclusive-empty-index-directory>\n"
                             "On macOS choose a unique index directory beneath /Volumes/PRO-BLADE/tmp.\n");
        return argc == 2 && std::string_view(argv[1]) == "--help" ? 0 : 2;
    }
    return ckv::examples::run_reporting_failure([&] {
        auto filesystem = std::make_shared<ckv::examples::NativeFileSystem>();
        ckv::examples::NativeClock clock;
        const ckv::examples::ExampleDiagnostics diagnostics(clock);
        if (!filesystem->is_absolute_path(argv[1])) throw std::invalid_argument("Initial directory must be absolute");
        // Construct before Application, join after its views are destroyed.
        // Index storage is chosen by the host, never by a library default.
        ckv::term::NativeDirectoryService directories(filesystem, argv[2]);
        ckv::examples::NativeTerminal terminal(clock);
        diagnostics.attach(terminal);
        ckv::term::TerminalClipboardWriter clipboard(terminal);
        ckv::ui::Application app(terminal, clock, clipboard);
        diagnostics.attach(app);
        ckv::completion_demo::CompletionApp demo(app, directories, argv[1]);
        app.run();
    });
}
} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
    // The CRT's narrow argv depends on the ANSI code page. Preserve native
    // Unicode paths before crossing the library's UTF-8 filesystem boundary.
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        auto argument = ckv::term::windows_utf8(argv[index]);
        if (!argument) {
            std::fprintf(stderr, "Completion Lab arguments must contain valid Unicode\n");
            return 2;
        }
        arguments.push_back(std::move(*argument));
    }
    std::vector<const char*> pointers;
    pointers.reserve(arguments.size());
    for (const auto& argument : arguments) pointers.push_back(argument.c_str());
    return run_completion(argc, pointers.data());
}
#else
int main(int argc, char** argv) { return run_completion(argc, argv); }
#endif
