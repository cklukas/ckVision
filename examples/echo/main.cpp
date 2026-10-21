// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision Echo -- shows each input event the terminal delivers as a decoded
// line; see docs/example-apps.md. Usage: ckvision_echo [transcript-file].
// With a file named, every line is also appended to it as it is made, so an
// interactive check of a host leaves a record behind.
#include "../native_host.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>

#include "../example_diagnostics.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "echo_app.hpp"

namespace {

struct CloseFile {
    void operator()(std::FILE* stream) const noexcept { std::fclose(stream); }
};

}  // namespace

int run_main(const std::vector<std::string>& args) {
    const auto argc = args.size();
    if (argc > 2) {
        std::fprintf(stderr, "usage: %s [transcript-file]\n", args[0].c_str());
        return EXIT_FAILURE;
    }
    std::unique_ptr<std::FILE, CloseFile> transcript;
    if (argc == 2) {
#if defined(_WIN32)
        transcript.reset(::_wfopen(std::filesystem::path(std::u8string(args[1].begin(), args[1].end())).c_str(), L"w"));
#else
        transcript.reset(std::fopen(args[1].c_str(), "w"));
#endif
        if (transcript == nullptr) {
            std::perror(args[1].c_str());
            return EXIT_FAILURE;
        }
    }
    return ckv::examples::run_reporting_failure([&transcript] {
        ckv::examples::NativeClock clock;
        const ckv::examples::ExampleDiagnostics diagnostics(clock);
        ckv::examples::NativeTerminal terminal(clock);
        diagnostics.attach(terminal);
        ckv::term::TerminalClipboardWriter clipboard(terminal);
        ckv::ui::Application app(terminal, clock, clipboard);
        diagnostics.attach(app);
        std::FILE* const stream = transcript.get();
        ckv::echo::EchoApp echo(app, [stream](std::string_view line) {
            if (stream == nullptr) return;
            std::fwrite(line.data(), 1, line.size(), stream);
            std::fputc('\n', stream);
            std::fflush(stream);
        });
        app.run();
    });
}

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    return ckv::examples::run_reporting_failure([&] { return run_main(ckv::examples::native_arguments(argc, argv)); });
}
