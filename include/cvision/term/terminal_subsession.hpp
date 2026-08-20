// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Platform-facing embedded-terminal contracts (D-042). The deterministic
// session values and UI seam live in cvision/core; this layer adds readiness
// sources and the launch factory.
#pragma once

#include <cstddef>
#include <memory>
#include <span>

#include "cvision/core/terminal_subsession.hpp"
#include "cvision/term/terminal.hpp"

namespace ckv::term {

using core::TerminalCapabilityProfile;
using core::TerminalClipboardPolicy;
using core::TerminalDiagnostic;
using core::TerminalExitPolicy;
using core::TerminalKeyboardFlags;
using core::TerminalEnvironmentPolicy;
using core::TerminalLaunchSpec;
using core::TerminalMouseEncoding;
using core::TerminalMouseTracking;
using core::TerminalOscPolicy;
using core::TerminalPrinterJob;
using core::TerminalPrinterPolicy;
using core::TerminalQueryPolicy;
using core::TerminalRaster;
using core::TerminalDamage;
using core::TerminalSnapshot;
using core::TerminalSnapshotOptions;
using core::TerminalStatus;
using core::TerminalSubsessionOptions;
using core::TerminalSubsessionState;

using core::embedded_xterm_sixel_profile;
using core::has_flag;
using core::supported_terminal_keyboard_flags;

class TerminalSubsession : public core::TerminalSubsession {
public:
    ~TerminalSubsession() override = default;

    // Adapter-only operations. They are intentionally outside the core seam:
    // readiness, process teardown, and scene identity are platform/application
    // ownership concerns rather than deterministic terminal model state.
    virtual bool drain(std::size_t byte_budget) {
        (void)byte_budget;
        return false;
    }
    virtual void close() noexcept {}
    // The identity a host assigns so this session's rasters can be told
    // apart from every other session's. A session that decodes graphics MUST
    // carry it: a raster left at the default id is dropped by the view that
    // would have drawn it, without a word, and the graphics never appear.
    //
    // Pure rather than defaulted, because a default that quietly does
    // nothing is precisely how that came to be true of the POSIX session for
    // as long as it had existed -- it forwarded every other call to its
    // emulator and silently swallowed this one.
    virtual void set_raster_identity(int identity) noexcept = 0;

    // Borrowed native readiness sources. Application presents them to the
    // outer Terminal's combined wait operation but never owns or closes them.
    virtual std::span<const WaitHandle> wait_handles() const noexcept { return {}; }
};

std::unique_ptr<TerminalSubsession> launch_terminal_subsession(TerminalLaunchSpec spec,
                                                                 TerminalSubsessionOptions options = {});

}  // namespace ckv::term
