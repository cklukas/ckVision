// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Standalone POSIX launch contract. It remains independently runnable when
// the aggregate test binary is temporarily blocked by an unrelated suite.
#if !defined(_WIN32)

#include <poll.h>

#include <string>

#include "cvision/term/posix_terminal_subsession.hpp"

namespace {

std::string screen_text(const ckv::term::TerminalSnapshot& snapshot) {
    std::string text;
    for (int row = 0; row < snapshot.cells.height; ++row) {
        for (int column = 0; column < snapshot.cells.width; ++column) {
            const ckv::Cell& cell = snapshot.cell_buffer[static_cast<std::size_t>(
                row * snapshot.cells.width + column)];
            if (!cell.is_continuation()) text += cell.grapheme();
        }
    }
    return text;
}

}  // namespace

int main() {
    ckv::term::TerminalLaunchSpec missing_spec =
        ckv::term::TerminalLaunchSpec::program("/definitely/missing/ckvision-child");
    missing_spec.exit_policy = ckv::core::TerminalExitPolicy::TerminateAfterGrace;
    auto missing = ckv::term::PosixTerminalSubsession::launch(std::move(missing_spec));
    if (missing->state() != ckv::term::TerminalSubsessionState::Failed ||
        missing->file_descriptor() >= 0 || missing->snapshot().diagnostics.empty())
        return 1;

    ckv::term::TerminalLaunchSpec success_spec =
        ckv::term::TerminalLaunchSpec::program("/bin/sh", {"-c", "printf ready"});
    success_spec.exit_policy = ckv::core::TerminalExitPolicy::TerminateAfterGrace;
    auto success = ckv::term::PosixTerminalSubsession::launch(std::move(success_spec));
    for (int attempt = 0; attempt < 100 && screen_text(success->snapshot()).find("ready") == std::string::npos;
         ++attempt) {
        pollfd ready{success->file_descriptor(), POLLIN | POLLHUP, 0};
        (void)::poll(&ready, 1, 10);
        (void)success->drain(16 * 1024);
    }
    return screen_text(success->snapshot()).find("ready") == std::string::npos ? 1 : 0;
}

#endif
