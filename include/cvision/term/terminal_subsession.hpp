// Copyright (c) 2026 C. Klukas. All rights reserved.
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
using core::TerminalDiagnostic;
using core::TerminalExitPolicy;
using core::TerminalEnvironmentPolicy;
using core::TerminalLaunchSpec;
using core::TerminalMouseEncoding;
using core::TerminalOscPolicy;
using core::TerminalQueryPolicy;
using core::TerminalRaster;
using core::TerminalSnapshot;
using core::TerminalSubsessionOptions;
using core::TerminalSubsessionState;

using core::embedded_xterm_sixel_profile;

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
    virtual void set_raster_identity(int identity) noexcept { (void)identity; }

    // Borrowed native readiness sources. Application presents them to the
    // outer Terminal's combined wait operation but never owns or closes them.
    virtual std::span<const WaitHandle> wait_handles() const noexcept { return {}; }
};

std::unique_ptr<TerminalSubsession> launch_terminal_subsession(TerminalLaunchSpec spec,
                                                                 TerminalSubsessionOptions options = {});

}  // namespace ckv::term
