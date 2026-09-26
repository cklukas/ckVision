// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision Echo -- shows each input event the terminal delivers as a decoded
// line; see docs/example-apps.md. Usage: ckvision_echo [transcript-file].
// With a file named, every line is also appended to it as it is made, so an
// interactive check of a host leaves a record behind.
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>

#include "../example_diagnostics.hpp"
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "echo_app.hpp"

namespace {

struct CloseFile {
    void operator()(std::FILE* stream) const noexcept { std::fclose(stream); }
};

}  // namespace

int main(int argc, char** argv) {
    if (argc > 2) {
        std::fprintf(stderr, "usage: %s [transcript-file]\n", argv[0]);
        return EXIT_FAILURE;
    }
    std::unique_ptr<std::FILE, CloseFile> transcript;
    if (argc == 2) {
        transcript.reset(std::fopen(argv[1], "w"));
        if (transcript == nullptr) {
            std::perror(argv[1]);
            return EXIT_FAILURE;
        }
    }
    return ckv::examples::run_reporting_failure([&transcript] {
        ckv::term::PosixClock clock;
        ckv::term::PosixTerminal terminal(clock);
        const ckv::examples::ExampleDiagnostics diagnostics(clock);
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
