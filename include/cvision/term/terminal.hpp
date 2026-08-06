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
