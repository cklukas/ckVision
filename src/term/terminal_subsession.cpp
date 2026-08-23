// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/terminal_subsession.hpp"
#include "cvision/term/terminal_emulator.hpp"

#if !defined(_WIN32)
#include "cvision/term/posix_terminal_subsession.hpp"
#endif

namespace ckv::term {

std::unique_ptr<TerminalSubsession> launch_terminal_subsession(TerminalLaunchSpec spec,
                                                                 TerminalSubsessionOptions options) {
#if !defined(_WIN32)
    return PosixTerminalSubsession::launch(std::move(spec), std::move(options));
#else
    auto failed = std::make_unique<TerminalEmulator>(spec.profile, options);
    failed->mark_failed("Windows ConPTY terminal subsessions are not available in this build");
    return failed;
#endif
}

}  // namespace ckv::term
