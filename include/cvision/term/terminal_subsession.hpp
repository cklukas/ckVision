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

#include "cvision/core/diagnostics.hpp"
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

// A core::TerminalSubsession that may own a live child process, adding the platform-side
// operations the deterministic model cannot express. launch_terminal_subsession() returns the
// platform's concrete session; hosts and fakes may derive their own.
class TerminalSubsession : public core::TerminalSubsession {
public:
    // Concrete platform sessions call close() on destruction.
    ~TerminalSubsession() override = default;

    // Adapter-only operations. They are intentionally outside the core seam:
    // readiness, process teardown, and scene identity are platform/application
    // ownership concerns rather than deterministic terminal model state.
    //
    // drain() moves at most `byte_budget` bytes of pending child output into the model
    // without blocking, flushes any replies the model generated back to the child, and
    // notices a child that has exited. Returns true when anything observable changed (output
    // was consumed or the session state moved). The default, for a session with no child,
    // does nothing and returns false.
    virtual bool drain(std::size_t byte_budget) {
        (void)byte_budget;
        return false;
    }
    // Ends the child according to the launch spec's exit policy and releases the native
    // resources; idempotent. It may block: WaitForExit waits for the child without limit,
    // TerminateAfterGrace waits a bounded grace period and then kills it. The default is a
    // no-op.
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

    // Where this session's emulator reports its graphics work (D-077): what a child asked about
    // graphics and was told, each picture decoded or rejected, pictures erased, and resizes. The
    // sink and clock are borrowed; the owning Application hands over its own trace on adoption
    // and withdraws it on release. Pure for the same reason as set_raster_identity: a session
    // that quietly dropped it would leave a host reading an empty trace and trusting it.
    virtual void set_graphics_trace(GraphicsTrace trace) noexcept = 0;

    // Borrowed native readiness sources. Application presents them to the
    // outer Terminal's combined wait operation but never owns or closes them.
    virtual std::span<const WaitHandle> wait_handles() const noexcept { return {}; }
};

// Starts `spec`'s program on a private pseudo-terminal (a POSIX PTY, or ConPTY on Windows)
// and returns its session. Never returns null and never throws for a launch failure: a spec
// without an exit policy, or a child that cannot be started, yields a session already in the
// Failed state with the reason recorded in its diagnostics(), so the failure reaches the
// view like any other session state.
std::unique_ptr<TerminalSubsession> launch_terminal_subsession(TerminalLaunchSpec spec,
                                                                 TerminalSubsessionOptions options = {});

}  // namespace ckv::term
