// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// The Terminal interface (ARCHITECTURE.md §4): capability report,
// input event stream, raw output, clipboard write, title, bell. Every
// backend (headless, posix, record/replay, later windows) implements
// this one seam; ui-layer code and Presenter never depend on a
// concrete backend.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "cvision/core/event.hpp"
#include "cvision/core/geometry.hpp"
#include "cvision/term/capabilities.hpp"

namespace ckv::term {

// Signals that a runtime probe refined the capability profile enough
// to matter for rendering (ARCHITECTURE.md §4) — routed like resize,
// triggering a defined re-present.
struct CapabilityChangedEvent {
    Capabilities capabilities;
    friend bool operator==(const CapabilityChangedEvent&, const CapabilityChangedEvent&) = default;
};

using TerminalEvent =
    std::variant<KeyEvent, MouseEvent, TextEvent, FocusEvent, ResizeEvent, CapabilityChangedEvent>;

class Terminal {
public:
    virtual ~Terminal() = default;

    virtual Capabilities capabilities() const noexcept = 0;
    virtual Size size() const noexcept = 0;

    // Blocks for new input up to `deadline_nanos` (absolute time on the
    // backend's Clock). Returns an empty vector on timeout — never
    // blocks past the deadline. May return more than one event per
    // call (a batch), consistent with the ui-layer's batched loop.
    virtual std::vector<TerminalEvent> poll(std::int64_t deadline_nanos) = 0;

    // Interrupts an in-flight poll(), if this backend can wait. This is
    // the cross-thread companion to ui::Application::post()/wake(): it
    // must be safe to call from a non-owning thread, must not deliver a
    // spurious input event, and may be a no-op for deterministic
    // non-blocking terminals such as HeadlessTerminal and replay.
    // Implementations return from poll() promptly and leave it to
    // Application to drain the posted work in its ordinary batch.
    virtual void wake() noexcept {}

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
