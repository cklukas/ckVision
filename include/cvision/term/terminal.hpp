// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The Terminal interface (the architecture §4): capability report,
// input event stream, raw output, clipboard write, title, bell. Every
// backend (headless, posix, record/replay, later windows) implements
// this one seam; ui-layer code and Presenter never depend on a
// concrete backend.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "cvision/core/event.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/term/capabilities.hpp"

namespace ckv::term {

// Signals that a runtime probe refined the capability profile enough
// to matter for rendering (the architecture §4) — routed like resize,
// triggering a defined re-present.
struct CapabilityChangedEvent {
    Capabilities capabilities;
    friend bool operator==(const CapabilityChangedEvent&, const CapabilityChangedEvent&) = default;
};

using TerminalEvent =
    std::variant<KeyEvent, MouseEvent, TextEvent, FocusEvent, ResizeEvent, CapabilityChangedEvent>;

// A borrowed platform-native source a host may include in its own wait loop.
// The handle set is empty for deterministic backends. A session-owning backend
// exposes every source that can make poll() useful (for POSIX, terminal input
// and the cross-thread wake channel); callers must neither close nor alter it.
enum class WaitHandleKind : std::uint8_t {
    PosixFileDescriptor,
    WindowsHandle,
};

struct WaitHandle {
    WaitHandleKind kind;
    std::uintptr_t value;

    friend bool operator==(const WaitHandle&, const WaitHandle&) = default;
};

class Terminal {
public:
    virtual ~Terminal() = default;

    virtual Capabilities capabilities() const noexcept = 0;
    virtual Size size() const noexcept = 0;

    // How many frame-completion replies this backend has decoded (see
    // InputDecoder::frame_acknowledgements). A backend with no input path
    // of its own answers zero and the facility simply stays unavailable —
    // the caller sees markers that are never acknowledged and gives up on
    // them, which is the same thing it does for a terminal that does not
    // answer.
    virtual std::size_t frame_acknowledgements() const noexcept { return 0; }

    // Blocks for new input up to `deadline_nanos` (absolute time on the
    // backend's Clock). Returns an empty vector on timeout — never
    // blocks past the deadline. May return more than one event per
    // call (a batch), consistent with the ui-layer's batched loop.
    virtual std::vector<TerminalEvent> poll(std::int64_t deadline_nanos) = 0;

    // Waits for this terminal's own sources plus borrowed readiness sources
    // supplied by a higher-level owner. A POSIX backend combines these with
    // poll(2), allowing a private child PTY to wake an Application without an
    // unrelated outer-terminal input event. Backends without such a facility
    // preserve the ordinary polling behavior.
    virtual std::vector<TerminalEvent> poll(std::int64_t deadline_nanos,
                                            std::span<const WaitHandle> additional_wait_handles) {
        (void)additional_wait_handles;
        return poll(deadline_nanos);
    }

    // Returns a borrowed, allocation-free view of the backend sources a host
    // may wait on before calling poll() or Application::step(). The returned
    // view is valid only while this Terminal remains alive and its native
    // session remains active. Empty is correct for non-blocking deterministic
    // backends such as HeadlessTerminal and ReplayTerminal.
    virtual std::span<const WaitHandle> wait_handles() const noexcept { return {}; }

    // Interrupts an in-flight poll(), if this backend can wait. This is
    // the cross-thread companion to ui::Application::post()/wake(): it
    // must be safe to call from a non-owning thread, must not deliver a
    // spurious input event, and may be a no-op for deterministic
    // non-blocking terminals such as HeadlessTerminal and replay.
    // Implementations return from poll() promptly and leave it to
    // Application to drain the posted work in its ordinary batch.
    virtual void wake() noexcept {}

    // Ends any terminal session this backend owns. Application calls this
    // exactly once during destruction, before flushing its buffered
    // diagnostics; implementations must make it idempotent because the
    // backend's own destructor performs the same final cleanup. Deterministic
    // in-memory terminals own no external session and keep the default no-op.
    virtual void restore() noexcept {}

    // Emits one diagnostic only after restore() completed. The default is for
    // terminal-less deterministic backends; session-owning backends may use
    // their platform-native diagnostic channel. Application never writes
    // stderr directly, preserving the ui/term impurity boundary.
    virtual void write_diagnostic_after_restore(std::string_view message) noexcept {
        std::fwrite(message.data(), 1, message.size(), stderr);
    }

    // Application event, draw, timer, posted-work, command, focus, and loop
    // predicate callbacks are noexcept-in-effect (the architecture §11).  The
    // callback boundary calls this instead of permitting an exception to
    // escape through an active terminal session. A backend that owns a live
    // terminal session must override it to restore that session before it
    // emits the diagnostic and terminates. The default is for deterministic
    // in-memory backends, which own no external terminal state.
    [[noreturn]] virtual void terminate_after_callback_failure() noexcept {
        std::fputs("ckVision contract violation: application callback threw\n", stderr);
        std::abort();
    }

    // The sole output sink: raw bytes to the terminal. Presenter is
    // the only intended caller for frame content; title/bell/clipboard
    // below are separate because they are not part of a frame's cell
    // diff and may be capability-gated independently.
    virtual void write(std::string_view bytes) = 0;

    virtual void set_title(std::string_view title) = 0;
    virtual void bell() = 0;

    // No-op when capabilities().clipboard_write is false — callers
    // never need to branch on the capability themselves.
    virtual void write_clipboard(std::string_view text) = 0;
};

}  // namespace ckv::term
